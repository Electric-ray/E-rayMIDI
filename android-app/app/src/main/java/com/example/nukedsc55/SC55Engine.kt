package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

class SC55Engine(val ctx: Context) : IEngine {

    companion object {
        private const val TAG = "SC55Engine"
        init { System.loadLibrary("nuked-sc55-jni") }
    }

    enum class Model(val id: Int, val label: String) {
        SC55_MK2(0, "SC-55mk2"),
    }

    val ROM_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "rom_sc55"
    ).absolutePath

    // ── Native 선언 ──────────────────────────────────────────────────────
    external fun nativeInit(romDir: String, modelId: Int): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeTerm()
    external fun nativeSendMidi(packed: Int)
    external fun nativeSendSysEx(data: ByteArray, len: Int)
    external fun nativeResetSynth()
    external fun nativeIsWarmupDone(): Boolean
    external fun nativeGetRomList(modelId: Int): String
    external fun nativeGetStats(): String
    external fun nativeGetVersion(): String
    external fun nativeGetLcdFrame(bitmap: android.graphics.Bitmap): Boolean
    external fun nativeGetLcdWidth(): Int
    external fun nativeGetLcdHeight(): Int
    external fun nativeGetSampleRate(): Int

    private var rtpSession: RtpMidiSession? = null
    private var usbMgr: UsbMidiManager? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    override var onStatus: ((String) -> Unit)? = null
    override var engineRunning = false

    // ── 진단용 카운터 (RTP vs USB에서 실제로 몇 개의 MIDI 메시지가 엔진까지
    //    도달하는지 1초마다 로그로 비교하기 위함 — LCD 파라미터 바 애니메이션이
    //    RTP에서만 안 되는 문제의 원인이 RTP 파싱 단계의 메시지 드롭인지
    //    확인하는 용도) ───────────────────────────────────────────────────
    private val diagCounts = java.util.concurrent.ConcurrentHashMap<String, java.util.concurrent.atomic.AtomicInteger>()
    private var diagThread: Thread? = null
    private var diagSource: String = "none"

    private fun diagBump(category: String) {
        diagCounts.getOrPut(category) { java.util.concurrent.atomic.AtomicInteger(0) }.incrementAndGet()
    }

    private fun diagCategory(bytes: ByteArray): String {
        if (bytes.isEmpty()) return "empty"
        val st = bytes[0].toInt() and 0xFF
        return when {
            st == 0xF0 -> "sysex"
            st and 0xF0 == 0x80 -> "noteOff"
            st and 0xF0 == 0x90 -> if (bytes.getOrElse(2) { 1 }.toInt() == 0) "noteOff(vel0)" else "noteOn"
            st and 0xF0 == 0xB0 -> "cc"
            st and 0xF0 == 0xC0 -> "progChange"
            st >= 0xF8 -> "realtime"
            else -> "other"
        }
    }

    private fun startDiagLogger(source: String) {
        diagSource = source
        diagCounts.clear()
        diagThread = Thread {
            while (engineRunning) {
                Thread.sleep(1000)
                if (diagCounts.isEmpty()) continue
                val snapshot = diagCounts.entries.joinToString(" ") { (k, v) -> "$k=${v.getAndSet(0)}" }
                Log.i("MidiDispatch", "[$diagSource] $snapshot")
            }
        }.also { it.isDaemon = true; it.start() }
    }

    // ── ROM 유틸 ─────────────────────────────────────────────────────────
    fun getRomFileList(model: Model = Model.SC55_MK2): List<String> =
        nativeGetRomList(model.id).split("|").filter { it.isNotBlank() }

    fun getRomHelpText(model: Model = Model.SC55_MK2): String {
        val files = getRomFileList(model)
        return buildString {
            appendLine("📂 ROM 파일 경로:")
            appendLine("$ROM_DIR/")
            appendLine()
            files.forEachIndexed { i, f -> appendLine("  ${i + 1}. $f") }
            appendLine()
            appendLine("※ ROM은 저작권 보호 대상입니다.")
        }
    }

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(model: Model = Model.SC55_MK2): Boolean {
        if (engineRunning) return true
        val missing = getRomFileList(model).filter { !File(ROM_DIR, it).exists() }
        if (missing.isNotEmpty()) {
            onStatus?.invoke("❌ 없는 ROM: ${missing.joinToString(", ")}")
            return false
        }
        val ok = nativeInit(ROM_DIR, model.id)
        if (!ok) { onStatus?.invoke("❌ SC-55 초기화 실패"); return false }
        nativeStart()
        engineRunning = true
        startWarmupMonitor()
        startSustainWatchdog()
        startNoteWatchdog()
        return true
    }

    private fun startWarmupMonitor() {
        onStatus?.invoke("⏳ SC-55 초기화 중...")
        Thread {
            val start = System.currentTimeMillis()
            while (!nativeIsWarmupDone()) {
                Thread.sleep(100)
            }
            val elapsed = System.currentTimeMillis() - start
            Log.i(TAG, "초기화 완료: ${elapsed}ms")
            onStatus?.invoke("✅ SC-55 준비됨 (${elapsed}ms) — MIDI 대기 중")
        }.also { it.isDaemon = true }.start()
    }

    // ── RTP-MIDI ─────────────────────────────────────────────────────────
    override fun startRtp() {
        stopUsb()
        startDiagLogger("RTP")
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
        // FIX (엔진 전환/재연결 시 "제어 타임아웃"): 이전에는 bindProcessToNetwork(null)을 먼저 호출해
        // 프로세스의 기본 네트워크 바인딩을 해제한 다음 rtpSession.stop()을 불렀다. 그러면
        // rtpSession.stop() 안의 BY(세션종료) UDP 패킷이 ESP32 AP 인터페이스가 아닌 엉눵한 기본 라우트(또는 네트워크 없음)로
        // 나가려하다 실패/손실될 수 있어, ESP32가 세션 종료를 못 받고 자체 타임아웃까지 새 연결을 거부하는
        // 원인으로 추정됨. 해결: BY가 나가기 때까지(rtpSession.stop() 먼저) 네트워크 바인딩을 유지한다.
        rtpSession?.stop(); rtpSession = null
        val cm = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        netCallback?.let { try { cm.unregisterNetworkCallback(it) } catch (_: Exception) {} }
        netCallback = null
        try { cm.bindProcessToNetwork(null) } catch (_: Exception) {}
    }

    // ── USB-MIDI ─────────────────────────────────────────────────────────
    override fun startUsb(): Boolean {
        stopRtp()
        startDiagLogger("USB")
        return UsbMidiManager(ctx, ::dispatchMidi).also { m ->
            m.onStatus = { msg -> onStatus?.invoke(msg) }
            usbMgr = m
        }.connect()
    }

    override fun stopUsb() { usbMgr?.disconnect(); usbMgr = null }

    // ── MIDI 디스패치 ────────────────────────────────────────────────────
    private val dumpCounter = java.util.concurrent.atomic.AtomicInteger(0)
    override fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        diagBump(diagCategory(bytes))
        // FIX (스트링/레거토 같은 음 재트리거 시 잔향 대응): Note On 이 들어왔는데
        // 같은 (채널,노트)이 이미 "켜져있다"고 추적되고 있으면, 이전 Note Off가
        // RTP로 유실되었다는 뜻이다. 워치독 타임아웃(8초)을 기다리지 않고, 새 Note On을
        // 보내기 직전에 즉시 강제 Note Off를 먼저 보낸다 (실제 신디사이저들도 같은 음 재발음 시
        // 이전 발음을 자동으로 컷하는 동작과 동일). 스트링처럼 같은 음을 자주 재트리거하는
        // 파트에서 잔향이 많이 줄어든다. (완전히 다른 음으로 넘어가면서 Note Off만 유실되는
        // 경우는 이것으로는 못 잡고 여전히 워치독에 의존한다.)
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
        val n = dumpCounter.incrementAndGet()
        if (n <= 60) {
            val hex = bytes.joinToString(" ") { String.format("%02X", it) }
            Log.i("MidiRawDump", "[$diagSource] #$n len=${bytes.size} $hex")
        }
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

    // IEngine 공통 계약: getNativeSampleRate / resetEngine
    override fun getNativeSampleRate(): Int = nativeGetSampleRate()

    // SC-55mk2 경로에서는 자동/전환 시점의 GS Reset을 절대 걸지 않기로
    // 확정되어 있다(리버브 과다 및 stuck note 재현됨 — 세 차례 시도 후 폐기).
    // hard 플래그와 무관하게 항상 All Notes/Sound Off만 수행한다.
    override fun resetEngine(hard: Boolean) {
        allNotesOff()
    }

    // ── 서스테인(CC64) 워치독 ──────────────────────────────────────────
    // RTP는 UDP라 패킷이 통채 유실될 수 있고, 하필 CC64=0(페달 해제)이 유실되면
    // 이후 모든 노트가 곽 끝까지 계속 울릴 수 있다 (USB는 유선이라 사실상 이런
    // 손실이 없음). 기존 30초 무신호 안전망은 곱이 계속 재생 중이면 발동하지
    // 않아서 이 케이스를 못 잡는다. 페달이 실제로 25초 넘게 계속 눈릴려있는 경우는
    // 거의 없으므로, 이 시간을 넘으면 RTP 손실로 보고 강제로 해제한다.
    private val sustainOnSince = java.util.concurrent.ConcurrentHashMap<Int, Long>()
    private var sustainWatchdog: Thread? = null
    private val SUSTAIN_TIMEOUT_MS = 10_000L

    private fun trackSustain(bytes: ByteArray) {
        if (bytes.size < 3) return
        val st = bytes[0].toInt() and 0xFF
        if (st and 0xF0 != 0xB0) return
        if ((bytes[1].toInt() and 0xFF) != 64) return  // CC#64 Hold pedal
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
                    // FIX (간헐적으로 안 꺼짐 버그): 예외로 이 스레드가 조용히 죽으면 그 뒤로는
                    // 세션 내내 서스테인 안전망이 전혀 작동하지 않게 된다. 로그만 남기고 계속 돌린다.
                    Log.e(TAG, "서스테인 워치독 예외(계속진행): $e")
                }
            }
        }.also { it.isDaemon = true; it.start() }
    }

    fun version(): String = nativeGetVersion()

    // ── 노트 워치독 (개별 Note Off 유실 대비) ───────────────────
    // CC64 워치독과 달리 페달이 아니라 특정 노트 하나의 Note Off만 유실되는
    // 경우를 잡기 위함. 겁대감 간겪으로 혼자만 계속 울리는 노트는 거의 없으므로,
    // 하나의 노트가 20초 넘게 계속 사운드변경 없이 켜져있으면 RTP 손실로 보고 강제 off.
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
                    // FIX (간헐적으로 안 꺼짐 버그): 예외로 이 스레드가 조용히 죽으면 그 뒤로는
                    // 세션 내내 노트 안전망이 전혀 작동하지 않게 된다. 로그만 남기고 계속 돌린다.
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
