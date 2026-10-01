package com.example.nukedsc55

import java.io.ByteArrayOutputStream

/**
 * SmfParser.kt — Standard MIDI File(.mid / .midi / .kar / .rmi) 파서.
 *
 * 파일을 "재생 시각이 붙은 MIDI 메시지 배열"(MidiSequence)로 풀어준다.
 * 각 메시지는 RtpMidiSession/UsbMidiManager가 엔진에 넘기는 것과 똑같은 형태
 * (채널 메시지 2~3바이트, SysEx는 F0..F7 전체)라서 IEngine.dispatchMidi()에 그대로 넣으면 된다.
 *
 * - Format 0 / 1 / 2 모두 처리 (모든 트랙을 tick 기준으로 안정 병합 — 같은 tick이면 트랙 순서 유지)
 * - 템포 맵을 미리 계산해서 모든 이벤트를 마이크로초 절대 시각으로 변환 (재생 중 tick 계산 불필요)
 * - PPQN / SMPTE division 지원
 * - 러닝 스테이터스, 여러 패킷으로 나뉜 SysEx, 잘린(truncated) 파일, RIFF(RMID) 래핑 허용
 * - Meta 이벤트는 템포(0x51)와 End of Track(0x2F)만 사용하고 나머지(가사/트랙명 등)는 버린다
 *
 * 안드로이드 API를 전혀 쓰지 않는 순수 Kotlin이라 JVM 단위 테스트가 가능하다.
 */
class MidiSequence(
    /** 각 메시지의 재생 시각(마이크로초, 곡 시작 기준). 오름차순. */
    val timesUs: LongArray,
    /** dispatchMidi()에 그대로 넘길 수 있는 완성된 MIDI 메시지. */
    val messages: Array<ByteArray>,
    /** 곡 전체 길이(마이크로초) — 가장 늦은 End of Track 기준. */
    val durationUs: Long
)

class SmfParseException(message: String) : Exception(message)

object SmfParser {

    private const val MAX_FILE_BYTES = 16 * 1024 * 1024
    private const val MAX_TRACKS = 1024

    /** 파싱 중간 표현. data==null && tempo>0 → 템포 변경, isEnd → 곡 끝 표식, 그 외 → MIDI 메시지 */
    private class Raw(val tick: Long, val data: ByteArray?, val tempo: Int, val isEnd: Boolean)

    private class Cur(val d: ByteArray, var pos: Int, val end: Int) {
        val eof: Boolean get() = pos >= end
        fun peek(): Int = d[pos].toInt() and 0xFF
        fun u8(): Int = d[pos++].toInt() and 0xFF

        /** Variable-length quantity (최대 4바이트) */
        fun varLen(): Long {
            var v = 0L
            var n = 0
            while (pos < end) {
                val b = u8()
                v = (v shl 7) or (b and 0x7F).toLong()
                n++
                if (b and 0x80 == 0 || n >= 4) break
            }
            return v
        }

        /** 남은 길이를 넘으면 잘라서 반환 (잘린 파일 대응) */
        fun bytes(n: Int): ByteArray {
            val m = minOf(n, end - pos).coerceAtLeast(0)
            val r = d.copyOfRange(pos, pos + m)
            pos += m
            return r
        }
    }

    fun parse(data: ByteArray): MidiSequence {
        if (data.size > MAX_FILE_BYTES) throw SmfParseException("파일이 너무 큼 (${data.size / 1024}KB)")
        val h = findHeader(data)
        if (h < 0 || h + 14 > data.size) throw SmfParseException("MIDI 파일이 아님 (MThd 헤더 없음)")

        val hdrLen = be32(data, h + 4)
        val division = be16(data, h + 12)
        var pos = h + 8 + (if (hdrLen in 6..1024) hdrLen else 6)

        val all = ArrayList<Raw>()
        var maxEndTick = 0L
        var tracks = 0
        while (pos + 8 <= data.size && tracks < MAX_TRACKS) {
            val len = be32(data, pos + 4)
            val bodyStart = pos + 8
            val bodyEnd = if (len < 0 || bodyStart.toLong() + len > data.size) data.size else bodyStart + len
            if (isTag(data, pos, "MTrk")) {
                val endTick = parseTrack(data, bodyStart, bodyEnd, all)
                if (endTick > maxEndTick) maxEndTick = endTick
                tracks++
            }
            pos = bodyEnd
        }
        if (all.none { it.data != null }) throw SmfParseException("재생할 MIDI 이벤트가 없음")

        all.add(Raw(maxEndTick, null, 0, true))
        val sorted = all.sortedBy { it.tick } // 안정 정렬: 같은 tick이면 원래(트랙) 순서 유지

        // tick → 마이크로초 변환 (템포 맵 반영)
        val smpte = (division and 0x8000) != 0
        val ppqn = if (division == 0) 480 else division
        val usPerTickSmpte: Double = if (smpte) {
            val fpsRaw = 256 - (division shr 8)        // 0xE8→24, 0xE7→25, 0xE3→29(29.97), 0xE2→30
            val fps = if (fpsRaw == 29) 29.97 else fpsRaw.toDouble()
            val tpf = maxOf(division and 0xFF, 1)
            1_000_000.0 / (maxOf(fps, 1.0) * tpf)
        } else 0.0

        val n = sorted.count { it.data != null }
        val times = LongArray(n)
        val msgs = Array(n) { ByteArray(0) }
        var idx = 0
        var lastTick = 0L
        var t = 0.0
        var usPerQuarter = 500_000.0
        var duration = 0L
        for (e in sorted) {
            val dt = e.tick - lastTick
            if (dt > 0) t += dt * (if (smpte) usPerTickSmpte else usPerQuarter / ppqn)
            lastTick = e.tick
            when {
                e.isEnd -> duration = t.toLong()
                e.data == null -> if (!smpte && e.tempo > 0) usPerQuarter = e.tempo.toDouble()
                else -> { times[idx] = t.toLong(); msgs[idx] = e.data; idx++ }
            }
        }
        if (n > 0 && times[n - 1] > duration) duration = times[n - 1]
        return MidiSequence(times, msgs, duration)
    }

    /** 트랙 하나를 파싱해 out에 추가하고, 이 트랙의 끝 tick(End of Track 또는 마지막 이벤트)을 반환한다. */
    private fun parseTrack(d: ByteArray, start: Int, end: Int, out: ArrayList<Raw>): Long {
        val c = Cur(d, start, end)
        var tick = 0L
        var running = 0
        var pending: ByteArrayOutputStream? = null // 여러 패킷으로 나뉜 SysEx 조립용
        var pendingTick = 0L
        var eot = -1L

        loop@ while (!c.eof) {
            tick += c.varLen()
            if (c.eof) break@loop

            var status = c.peek()
            if (status < 0x80) {
                if (running == 0) { c.pos++; continue@loop } // 상태 바이트 없는 잘못된 데이터 — 건너뜀
                status = running
            } else {
                c.pos++
                // 채널 메시지만 러닝 스테이터스 대상. (Meta/SysEx 뒤에도 유지 — 스펙은 해제하라고 하지만
                // 그런 파일이 실제로 있고, 스펙을 지킨 파일에서는 어차피 데이터 바이트가 바로 올 일이 없다)
                if (status < 0xF0) running = status
            }

            when {
                status == 0xFF -> {
                    if (c.eof) break@loop
                    val type = c.u8()
                    val payload = c.bytes(c.varLen().toInt())
                    if (type == 0x51 && payload.size == 3) {
                        val tempo = ((payload[0].toInt() and 0xFF) shl 16) or
                                ((payload[1].toInt() and 0xFF) shl 8) or
                                (payload[2].toInt() and 0xFF)
                        if (tempo > 0) out.add(Raw(tick, null, tempo, false))
                    } else if (type == 0x2F) {
                        eot = tick
                        break@loop
                    }
                }
                status == 0xF0 -> {
                    val payload = c.bytes(c.varLen().toInt())
                    val msg = ByteArray(payload.size + 1)
                    msg[0] = 0xF0.toByte()
                    System.arraycopy(payload, 0, msg, 1, payload.size)
                    if (payload.isNotEmpty() && payload[payload.size - 1] == 0xF7.toByte()) {
                        out.add(Raw(tick, msg, 0, false))
                        pending = null
                    } else {
                        pending = ByteArrayOutputStream().also { it.write(msg, 0, msg.size) }
                        pendingTick = tick
                    }
                }
                status == 0xF7 -> {
                    val payload = c.bytes(c.varLen().toInt())
                    val p = pending
                    if (p != null) { // SysEx 이어붙이기 패킷
                        p.write(payload, 0, payload.size)
                        if (payload.isNotEmpty() && payload[payload.size - 1] == 0xF7.toByte()) {
                            out.add(Raw(pendingTick, p.toByteArray(), 0, false))
                            pending = null
                        }
                    } // p == null → 임의 바이트 이스케이프(엔진에 보낼 의미 없음) — 무시
                }
                (status and 0xF0) in 0x80..0xE0 -> {
                    val need = if ((status and 0xF0) == 0xC0 || (status and 0xF0) == 0xD0) 1 else 2
                    if (c.pos + need > c.end) {
                        c.pos = c.end // 잘린 파일
                    } else {
                        val m = ByteArray(1 + need)
                        m[0] = status.toByte()
                        m[1] = (c.u8() and 0x7F).toByte()
                        if (need == 2) m[2] = (c.u8() and 0x7F).toByte()
                        out.add(Raw(tick, m, 0, false))
                    }
                }
                else -> { /* 0xF1~0xF6, 0xF8~0xFE: SMF에서는 의미 없음 — 무시 */ }
            }
        }
        return if (eot >= 0) eot else tick
    }

    private fun findHeader(d: ByteArray): Int {
        var i = 0
        val limit = d.size - 4
        while (i <= limit) {
            if (isTag(d, i, "MThd")) return i
            i++
        }
        return -1
    }

    private fun isTag(d: ByteArray, p: Int, tag: String): Boolean {
        if (p + 4 > d.size) return false
        for (i in 0 until 4) if (d[p + i].toInt() != tag[i].code) return false
        return true
    }

    private fun be16(d: ByteArray, p: Int): Int =
        ((d[p].toInt() and 0xFF) shl 8) or (d[p + 1].toInt() and 0xFF)

    private fun be32(d: ByteArray, p: Int): Int = (be16(d, p) shl 16) or be16(d, p + 2)
}
