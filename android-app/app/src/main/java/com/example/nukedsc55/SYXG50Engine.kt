package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

/**
 * SYXG50Engine.kt
 * madaha(Rust) 기반 S-YXG50 에뮬레이션 엔진. SoundFontEngine과 동일한 외부
 * 인터페이스(initEngine/startRtp/startUsb/dispatchMidi/allNotesOff/stop)를
 * 제공해서 MainActivity가 네 엔진을 동일한 방식으로 다룰 수 있게 한다.
 *
 * ROM: Downloads/rom_s-yxg50/{sxgbin41.tbl, sxgwave4.tbl} 두 파일이 필요.
 * (SoundFontEngine과 동일하게 Downloads 하위 폴더에서 직접 읽음 — SAF 불필요)
 */
class SYXG50Engine(val ctx: Context) : IEngine {

    companion object {
        private const val TAG = "SYXG50Engine"
        init { System.loadLibrary("syxg50-jni") }
    }

    val ROM_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "rom_s-yxg50"
    ).absolutePath

    private val binFile: File get() = File(ROM_DIR, "sxgbin41.tbl")
    private val waveFile: File get() = File(ROM_DIR, "sxgwave4.tbl")

    // ── Native 선언 ──────────────────────────────────────────────────────
    external fun nativeInit(binPath: String, wavePath: String): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeTerm()
    external fun nativeSendMidi(packed: Int)
    external fun nativeSendSysEx(data: ByteArray, len: Int)
    external fun nativeIsReady(): Boolean
    external fun nativeGetVersion(): String
    external fun nativeGetSampleRate(): Int
    external fun nativeGetLastError(): String
    external fun nativeGetChannelActiveMask(): Int
    external fun nativeGetChannelProgramText(channel: Int): String

    private var rtpSession: RtpMidiSession? = null
    private var usbMgr: UsbMidiManager? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    override var onStatus: ((String) -> Unit)? = null
    override var engineRunning = false

    // ── ROM 상태 UI 공통 계약 (SC55Engine/MuntEngine과 동일한 이름) ─────────
    fun getRomFileList(): List<String> = listOf("sxgbin41.tbl", "sxgwave4.tbl")

    fun getRomHelpText(): String =
        "S-YXG50 롬 파일 2개가 필요합니다:\n\n" +
        "  sxgbin41.tbl\n" +
        "  sxgwave4.tbl\n\n" +
        "다음 폴더에 넣어주세요:\n$ROM_DIR"

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(): Boolean {
        if (engineRunning) return true
        if (!binFile.exists() || !waveFile.exists()) {
            onStatus?.invoke("❌ S-YXG50 롬 파일 없음: $ROM_DIR (sxgbin41.tbl / sxgwave4.tbl)")
            return false
        }
        onStatus?.invoke("⏳ S-YXG50 롬 로딩 중...")
        val ok = nativeInit(binFile.absolutePath, waveFile.absolutePath)
        if (!ok) {
            onStatus?.invoke("❌ S-YXG50 초기화 실패: ${nativeGetLastError()}")
            return false
        }
        nativeStart()
        engineRunning = true
        startSustainWatchdog()
        startNoteWatchdog()
        onStatus?.invoke("✅ S-YXG50 준비됨 — MIDI 대기 중")
        return true
    }

    // ── RTP-MIDI (SoundFontEngine과 동일한 구조) ──────────────────────────
    override fun startRtp() {
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

    override fun stopRtp() {
        rtpSession?.stop(); rtpSession = null
        val cm = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        netCallback?.let { try { cm.unregisterNetworkCallback(it) } catch (_: Exception) {} }
        netCallback = null
        try { cm.bindProcessToNetwork(null) } catch (_: Exception) {}
    }

    // ── USB-MIDI ─────────────────────────────────────────────────────────
    override fun startUsb(): Boolean {
        stopRtp()
        return UsbMidiManager(ctx, ::dispatchMidi).also { m ->
            m.onStatus = { msg -> onStatus?.invoke(msg) }
            usbMgr = m
        }.connect()
    }

    override fun stopUsb() { usbMgr?.disconnect(); usbMgr = null }

    // ── MIDI 디스패치 (SoundFontEngine과 동일한 근거의 재트리거 보정 포함) ──
    override fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
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

    override fun allNotesOff() {
        for (ch in 0..15) {
            nativeSendMidi((0xB0 or ch) or (123 shl 8))
            nativeSendMidi((0xB0 or ch) or (120 shl 8))
        }
    }

    fun version(): String = nativeGetVersion()

    // MuntEngine/SoundFontEngine과 동일한 형태의 LED 패널 데이터 (16채널용).
    // S-YXG50 ROM(TBL)에는 SoundFont 같은 악기명 테이블이 없어 뱅크/프로그램
    // 번호를 대신 보여준다 (nativeGetChannelProgramText 참고).
    fun getPartInfo(): PartInfo {
        val states = runCatching { nativeGetChannelActiveMask() }.getOrDefault(0).toLong()
        val names = (0..15).map { ch ->
            runCatching { nativeGetChannelProgramText(ch) }.getOrDefault("---").ifBlank { "---" }
        }
        return PartInfo(states, names)
    }

    override fun getNativeSampleRate(): Int = nativeGetSampleRate()

    override fun resetEngine(hard: Boolean) {
        // S-YXG50은 XG SysEx(Reset All Parameters)로 하드 리셋을 지원하지만,
        // SC55Engine과 동일하게 자동 전환 경로에서는 안전하게 All Notes/Sound
        // Off만 수행한다. XG Reset은 필요 시 사용자가 명시적으로 누르는
        // 수동 리셋 버튼 경로에서만 별도 처리할 것.
        allNotesOff()
    }

    // ── 서스테인(CC64) 워치독 — SoundFontEngine과 동일한 근거로 필요 ────────
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
    private val noteOnSince = java.util.concurrent.ConcurrentHashMap<Int, Long>()
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

    override fun stop() {
        stopRtp(); stopUsb()
        if (engineRunning) {
            nativeStop(); nativeTerm()
            engineRunning = false
        }
    }
}
