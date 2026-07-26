package com.example.nukedsc55

/**
 * 연속 MIDI 바이트 스트림을 완성된 메시지로 콜백
 * Running Status 지원, SysEx 지원(F0..F7)
 * USB-serial 38400bps로 들어오는 raw MIDI 바이트용
 * (munt-android에서 그대로 포팅 → 검증된 구현)
 */
class MidiStreamParser(private val onMessage: (ByteArray) -> Unit) {

    private var runningStatus = 0
    private val buf = ByteArray(512)
    private var bufPos = 0
    private var expected = 0
    private var inSysEx = false

    fun reset() { runningStatus = 0; bufPos = 0; expected = 0; inSysEx = false }

    fun feed(data: ByteArray) {
        for (b in data) feedByte(b.toInt() and 0xFF)
    }

    private fun feedByte(b: Int) {
        if (b >= 0xF8) { onMessage(byteArrayOf(b.toByte())); return }

        if (b == 0xF7) {
            if (inSysEx && bufPos > 0) {
                buf[bufPos++] = 0xF7.toByte()
                onMessage(buf.copyOf(bufPos))
            }
            inSysEx = false; bufPos = 0; expected = 0
            return
        }

        if (b == 0xF0) {
            inSysEx = true; bufPos = 0; expected = 0
            buf[bufPos++] = 0xF0.toByte()
            return
        }

        if (inSysEx) {
            // FIX (특정 곡에서 소리 안 나는 문제 대응): SysEx 중 0xF7 없이 상태바이트(0x80~0xEF)가
            // 섞이면 손상/미종료된 SysEx로 보고 즉시 중단한다. 이전에는 0xF0/0xF7이 다시
            // 오기 전까지 무조건 바이트를 계속 흡수해서, 전송 에러로 끝맺은 SysEx 뒤로는
            // 그 곡이 끝날 때까지 모든 MIDI(Note On/Off포함)가 통째로 사라지는 버그가 있었음.
            if (b and 0x80 != 0) {
                inSysEx = false; bufPos = 0
                // return 하지 않고 아래로 이어져 이 바이트를 새 메시지의 상태바이트로 재동기화
            } else {
                if (bufPos < buf.size) buf[bufPos++] = b.toByte()
                return
            }
        }

        if (b and 0x80 != 0) {
            // MIDI spec: only 0x80-0xEF (Channel Voice/Mode) may be stored
            // as Running Status. 0xF1-0xF6 (System Common) must CLEAR it.
            runningStatus = if (b < 0xF0) b else 0
            bufPos = 0
            buf[bufPos++] = b.toByte()
            expected = messageLength(b)
            if (expected == 1) { emit(); return }
            return
        }

        if (runningStatus == 0) return
        if (bufPos == 0) {
            buf[bufPos++] = runningStatus.toByte()
            expected = messageLength(runningStatus)
        }
        if (bufPos < buf.size) buf[bufPos++] = b.toByte()
        if (bufPos >= expected) emit()
    }

    private fun emit() {
        onMessage(buf.copyOf(bufPos))
        bufPos = 0
    }

    private fun messageLength(status: Int): Int = when (status and 0xF0) {
        0x80, 0x90, 0xA0, 0xB0, 0xE0 -> 3
        0xC0, 0xD0                    -> 2
        0xF0 -> when (status) { 0xF2 -> 3; 0xF1, 0xF3 -> 2; else -> 1 }
        else -> 1
    }
}
