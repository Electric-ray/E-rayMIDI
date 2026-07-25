package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

/**
 * SoundFontEngine.kt
 * TinySoundFont(SF2) 기반 재생 엔진. SC55Engine과 동일한 외부 인터페이스
 * (initEngine/startRtp/startUsb/dispatchMidi/allNotesOff/stop)를 제공해서
 * MainActivity가 두 엔진을 동일한 방식으로 다룰 수 있게 한다.
 *
 * RtpMidiSession/UsbMidiManager는 완전히 엔진 독립적(콜백 기반)이라 그대로
 * 재사용한다 — SC-55 쪽에서 어렵게 잡은 RTP 안정성 수정(SSRC 고정, SysEx
 * 경계 마커 처리, DT1 체크섬 검증, WifiLock 등)을 하나도 건드리지 않고
 * 그대로 물려받는다.
 */
class SoundFontEngine(val ctx: Context) {

    companion object {
        private const val TAG = "SoundFontEngine"
        init { System.loadLibrary("soundfont-jni") }
    }

    val SOUNDFONT_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "soundfont"
    ).absolutePath

    // ── Native 선언 ──────────────────────────────────────────────────────
    external fun nativeInit(sf2Path: String): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeTerm()
    external fun nativeSendMidi(packed: Int)
    external fun nativeSendSysEx(data: ByteArray, len: Int)
    external fun nativeIsReady(): Boolean
    external fun nativeGetActiveVoices(): Int
    external fun nativeGetPresetCount(): Int
    external fun nativeGetChannelPresetName(channel: Int): String
    external fun nativeGetVersion(): String

    private var rtpSession: RtpMidiSession? = null
    private var usbMgr: UsbMidiManager? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    var onStatus: ((String) -> Unit)? = null
    var engineRunning = false

    // ── 사운드폰트 파일 유틸 ─────────────────────────────────────────────
    fun getSoundFontFileList(): List<File> {
        val dir = File(SOUNDFONT_DIR)
        if (!dir.exists()) return emptyList()
        return dir.listFiles { f -> f.isFile && f.name.endsWith(".sf2", ignoreCase = true) }
            ?.sortedBy { it.name.lowercase() } ?: emptyList()
    }

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(sf2Path: String): Boolean {
        if (engineRunning) return true
        if (!File(sf2Path).exists()) {
            onStatus?.invoke("❌ 사운드폰트 파일 없음: $sf2Path")
            return false
        }
        onStatus?.invoke("⏳ 사운드폰트 로딩 중...")
        val ok = nativeInit(sf2Path)
        if (!ok) { onStatus?.invoke("❌ 사운드폰트 로드 실패"); return false }
        nativeStart()
        engineRunning = true
        startSustainWatchdog()
        startNoteWatchdog()
        onStatus?.invoke("✅ 사운드폰트 준비됨 (프리셋 ${nativeGetPresetCount()}개) — MIDI 대기 중")
        return true
    }

    // ── RTP-MIDI (SC55Engine과 동일한 구조) ───────────────────────────────
    fun startRtp() {
        stopUsb()
        val session = RtpMidiSession(
            ctx           = ctx,
            onMidiMessage = ::dispatchMidi,
            onStatus      = { msg -> onStatus?.invoke(msg) },
            onAllNotesOff = ::allNotesOff
        )
        rtpSession = session

        val cm = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .build()

        var started = false
        val cb = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(net: Network) {
                cm.bindProcessToNetwork(net)
                session.updateNetwork(net)
                if (!started) { started = true; session.start(net) }
            }
            override fun onLost(net: Network) {
                session.updateNetwork(null)
            }
        }
        netCallback = cb
        try {
            cm.requestNetwork(request, cb)
        } catch (e: Exception) {
            Log.w(TAG, "requestNetwork 실패: $e")
            session.start(null)
        }
    }

    fun stopRtp() {
        val cm = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        netCallback?.let { try { cm.unregisterNetworkCallback(it) } catch (_: Exception) {} }
        netCallback = null
        try { cm.bindProcessToNetwork(null) } catch (_: Exception) {}
        rtpSession?.stop(); rtpSession = null
    }

    // ── USB-MIDI ─────────────────────────────────────────────────────────
    fun startUsb(): Boolean {
        stopRtp()
        return UsbMidiManager(ctx, ::dispatchMidi).also { m ->
            m.onStatus = { msg -> onStatus?.invoke(msg) }
            usbMgr = m
        }.connect()
    }

    fun stopUsb() { usbMgr?.disconnect(); usbMgr = null }

    // ── MIDI 디스패치 ────────────────────────────────────────────────────
    fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        // FIX (스트링/레거토 같은 음 재트리거 시 잔향 대응): SC55Engine과 동일한 근거.
        // 같은 (채널,노트)이 이미 켜져있다고 추적되는데 새 Note On이 들어오면,
        // 이전 Note Off가 RTP로 유실된 것이므로 워치독 타임아웃을 기다리지 않고 즉시
        // 강제 Note Off를 먼저 보낸다.
        run {
            if (bytes.size >= 3) {
                val st0 = bytes[0].toInt() and 0xFF
                if (st0 and 0xF0 == 0x90 && (bytes[2].toInt() and 0xFF) > 0) {
                    val ch0 = st0 and 0x0F
                    val note0 = bytes[1].toInt() and 0x7F
                    val key0 = ch0 * 128 + note0
                    if (noteOnSince.containsKey(key0)) {
                        nativeSendMidi((0x80 or ch0) or (note0 shl 8))
                        noteOnSince.remove(key0)
                    }
                }
            }
        }
        trackSustain(bytes)
        trackNote(bytes)
        if (bytes[0] == 0xF0.toByte()) {
            nativeSendSysEx(bytes, bytes.size)
        } else {
            val st = bytes.getOrElse(0) { 0 }.toInt() and 0xFF
            val d1 = bytes.getOrElse(1) { 0 }.toInt() and 0xFF
            val d2 = bytes.getOrElse(2) { 0 }.toInt() and 0xFF
            nativeSendMidi(st or (d1 shl 8) or (d2 shl 16))
        }
    }

    fun allNotesOff() {
        for (ch in 0..15) {
            nativeSendMidi((0xB0 or ch) or (123 shl 8))
            nativeSendMidi((0xB0 or ch) or (120 shl 8))
        }
    }

    fun version(): String = nativeGetVersion()

    // ── 서스테인(CC64) 워치독 — SC55Engine과 동일한 근거로 필요 ────────────
    // RTP는 UDP라 패킷이 통째로 유실될 수 있고, 하필 CC64=0(페달 해제)이 유실되면
    // 이후 노트가 곡 끝까지 계속 울릴 수 있다. 페달이 실제로 10초 넘게 계속
    // 눌려있는 경우는 거의 없으므로, 이 시간을 넘으면 RTP 손실로 보고 강제 해제한다.
    private val sustainOnSince = java.util.concurrent.ConcurrentHashMap<Int, Long>()
    private var sustainWatchdog: Thread? = null
    private val SUSTAIN_TIMEOUT_MS = 10_000L

    private fun trackSustain(bytes: ByteArray) {
        if (bytes.size < 3) return
        val st = bytes[0].toInt() and 0xFF
        if (st and 0xF0 != 0xB0) return
        if ((bytes[1].toInt() and 0xFF) != 64) return
        val ch = st and 0x0F
        val value = bytes[2].toInt() and 0xFF
        if (value >= 64) {
            sustainOnSince.putIfAbsent(ch, System.currentTimeMillis())
        } else {
            sustainOnSince.remove(ch)
        }
    }

    private fun startSustainWatchdog() {
        if (sustainWatchdog != null) return
        sustainWatchdog = Thread {
            while (engineRunning) {
                try {
                    Thread.sleep(1500)
                    val now = System.currentTimeMillis()
                    val it = sustainOnSince.entries.iterator()
                    while (it.hasNext()) {
                        val (ch, since) = it.next()
                        if (now - since > SUSTAIN_TIMEOUT_MS) {
                            Log.w(TAG, "⚠️ CC64(서스테인) ch=$ch 가 ${(now - since) / 1000}초째 유지됨 → 강제 해제 (RTP 유실 의심)")
                            nativeSendMidi((0xB0 or ch) or (64 shl 8))
                            it.remove()
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "서스테인 워치독 예외(계속진행): $e")
                }
            }
        }.also { it.isDaemon = true; it.start() }
    }

    // ── 노트 워치독 (개별 Note Off 유실 대비) ───────────────────────────
    private val noteOnSince = java.util.concurrent.ConcurrentHashMap<Int, Long>()  // key = ch*128+note
    private var noteWatchdog: Thread? = null
    private val NOTE_TIMEOUT_MS = 8_000L

    private fun trackNote(bytes: ByteArray) {
        if (bytes.size < 3) return
        val st = bytes[0].toInt() and 0xFF
        val note = bytes[1].toInt() and 0x7F
        val vel  = bytes[2].toInt() and 0xFF
        val ch   = st and 0x0F
        val key  = ch * 128 + note
        when {
            st and 0xF0 == 0x90 && vel > 0 -> noteOnSince[key] = System.currentTimeMillis()
            st and 0xF0 == 0x90 && vel == 0 -> noteOnSince.remove(key)
            st and 0xF0 == 0x80 -> noteOnSince.remove(key)
        }
    }

    private fun startNoteWatchdog() {
        if (noteWatchdog != null) return
        noteWatchdog = Thread {
            while (engineRunning) {
                try {
                    Thread.sleep(1000)
                    val now = System.currentTimeMillis()
                    val it = noteOnSince.entries.iterator()
                    while (it.hasNext()) {
                        val (key, since) = it.next()
                        if (now - since > NOTE_TIMEOUT_MS) {
                            val ch = key / 128
                            val note = key % 128
                            Log.w(TAG, "⚠️ Note ch=$ch note=$note 가 ${(now - since) / 1000}초째 울림 → 강제 Note Off (RTP 유실 의심)")
                            nativeSendMidi((0x80 or ch) or (note shl 8))
                            it.remove()
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "노트 워치독 예외(계속진행): $e")
                }
            }
        }.also { it.isDaemon = true; it.start() }
    }

    fun stop() {
        stopRtp(); stopUsb()
        if (engineRunning) {
            nativeStop(); nativeTerm()
            engineRunning = false
        }
    }
}
