package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

/**
 * GearmulatorEngine — dsp56300/gearmulator의 88emu(88lib)를 감싼 IEngine 구현체.
 *
 * (E-rayMIDI_88emu_통합_개발계획서_v2.0 Phase 4/5 참고)
 *
 * SC55Engine/MuntEngine과 다른 점: 88lib 하나가 SC-55/SC-88/SC-88Pro/SC-8850과
 * MT-32/CM-32L/CM-32P/CM-64 계열을 전부 커버하므로, "엔진 하나 = 디바이스 하나"가
 * 아니라 "엔진 하나 = 선택 가능한 Model 여러 개"다. 계획서 §7의 최종 목표
 * (GearmulatorEngine 하나가 여러 Roland 기기를 선택적으로 구동)를 그대로 따른다.
 *
 * 계획서 §11 Phase 5 원칙대로, 첫 통합 대상은 SC-55다 — 이미 기존 SC55Engine이
 * 있어 A/B 비교 기준을 만들 수 있기 때문이다. 나머지 기종은 default Model 목록에는
 * 포함해 두되(§10 "디바이스 전환은 재초기화 방식"), 실기 검증은 Phase 6 이후.
 */
class GearmulatorEngine(val ctx: Context) : IEngine {

    companion object {
        private const val TAG = "GearmulatorEngine"
        init { System.loadLibrary("gearmulator-jni") }
    }

    /**
     * cliId는 88emuPlayer의 CLI ID와 동일 문자열(GearmulatorBridge.cpp의
     * resolveDeviceId 테이블과 반드시 일치해야 함). label은 UI 표시용.
     * 계획서 §7의 24개 기종 중, 우선순위(§11 Phase 5~9)에 따라 순서를 정렬했다.
     */
    enum class Model(val cliId: String, val label: String) {
        SC55("sc55", "SC-55"),
        SC55MK2("sc55mk2", "SC-55mk2"),
        SC88("sc88", "SC-88"),
        SC88PRO("sc88pro", "SC-88Pro"),
        SC8820("sc8820", "SC-8820"),
        SC8850("sc8850", "SC-8850"),
        MT32_OLD("mt32old", "MT-32 (구형 기판)"),
        MT32_NEW("mt32new", "MT-32 (신형 기판)"),
        CM32L("cm32l", "CM-32L"),
        CM32P("cm32p", "CM-32P"),
        CM64("cm64", "CM-64"),
    }

    /** 현재 선택된 기종. initEngine()에서 확정되고 stop() 전까지 바뀌지 않는다
     *  (계획서 §10: 기종 전환은 재초기화 방식 — hot swap 미지원). */
    var currentModel: Model = Model.SC55MK2
        private set

    /** UI의 기종 선택 다이얼로그에서 호출. 엔진이 이미 돌아가는 중에는
     *  바꿀 수 없다 (계획서 §10: 재초기화 방식, hot swap 미지원 — 먼저
     *  연결을 끊어야 함). 성공하면 true. */
    fun selectModel(model: Model): Boolean {
        if (engineRunning) return false
        currentModel = model
        return true
    }

    val ROM_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "rom_gearmulator"
    ).absolutePath

    // ── Native 선언 ──────────────────────────────────────────────────────
    external fun nativeInit(romDir: String, deviceCliId: String): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeRestartAudio()
    override fun restartAudio() { if (engineRunning) nativeRestartAudio() }
    external fun nativeTerm()
    external fun nativeSendMidi(packed: Int)
    external fun nativeSendSysEx(data: ByteArray, len: Int)
    external fun nativeResetSynth()
    external fun nativeHardReset()
    external fun nativeIsBootDone(): Boolean
    external fun nativeGetSampleRate(): Int
    external fun nativeGetStats(): String
    external fun nativeGetVersion(): String
    external fun nativeAddRomPath(path: String)
    external fun nativeIsDeviceAvailable(deviceCliId: String): Boolean
    external fun nativeDescribeRoms(deviceCliId: String): String
    external fun nativeRescanRoms()
    external fun nativeGetDisplayText(screen: Int): String
    external fun nativeIsDisplayOn(screen: Int): Boolean
    external fun nativeGetPanelLeds(): Int
    // 실제 기기 LCD 프레임: (width shl 16) or height, LCD 없는 기종/미연결이면 0
    external fun nativeGetLcdSize(): Int
    // 화면 내용이 바뀔 때마다 증가하는 카운터 (같으면 다시 그릴 필요 없음)
    external fun nativeGetLcdSeq(): Long
    // ARGB_8888 비트맵(크기 == nativeGetLcdSize)에 LCD를 합성
    external fun nativeGetLcdFrame(bitmap: android.graphics.Bitmap): Boolean

    private var rtpSession: RtpMidiSession? = null
    private var usbMgr: UsbMidiManager? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    override var onStatus: ((String) -> Unit)? = null
    override var engineRunning = false

    // MIDI 파일 재생 중에는 true (IEngine.bypassWatchdogs 참고) — 유실 없는 입력이라 워치독/재발음 보정을 건너뜀
    @Volatile override var bypassWatchdogs = false

    // ── ROM 유틸 ─────────────────────────────────────────────────────────
    // 88lib는 파일명이 아니라 내용으로 ROM을 식별하므로(§9 "content, never by
    // name"), SC55Engine처럼 정확한 파일명 목록을 직접 비교하지 않고 88lib
    // 자신의 판단(emu88_is_device_available / emu88_describe_device_roms)을
    // 그대로 신뢰한다. ROM_DIR가 아직 등록되지 않았을 수 있으니 먼저 등록한다.
    private fun ensureRomPathRegistered() {
        File(ROM_DIR).mkdirs()
        nativeAddRomPath(ROM_DIR)
    }

    fun isModelAvailable(model: Model = currentModel): Boolean {
        ensureRomPathRegistered()
        return nativeIsDeviceAvailable(model.cliId)
    }

    fun getRomHelpText(model: Model = currentModel): String {
        ensureRomPathRegistered()
        return buildString {
            appendLine("📂 ROM 폴더: $ROM_DIR/")
            appendLine("   (하위 폴더에 넣어도 된다 — 88lib가 내용으로 알아서 찾는다)")
            appendLine()
            append(nativeDescribeRoms(model.cliId))
            appendLine()
            appendLine("※ ROM은 저작권 보호 대상입니다.")
        }
    }

    /** ROM 폴더에 파일을 새로 추가한 뒤(파일 관리자 등으로) 다시 스캔하고 싶을 때. */
    fun rescanRoms() {
        ensureRomPathRegistered()
        nativeRescanRoms()
    }

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(model: Model = Model.SC55MK2): Boolean {
        if (engineRunning) return true
        ensureRomPathRegistered()
        if (!nativeIsDeviceAvailable(model.cliId)) {
            onStatus?.invoke("❌ ${model.label} ROM이 없습니다 — ROM 도움말을 확인하세요")
            return false
        }
        val ok = nativeInit(ROM_DIR, model.cliId)
        if (!ok) { onStatus?.invoke("❌ ${model.label} 초기화 실패"); return false }
        currentModel = model
        nativeStart()
        engineRunning = true
        resetPartTracking()
        startBootMonitor()
        startSustainWatchdog()
        startNoteWatchdog()
        return true
    }

    private fun startBootMonitor() {
        onStatus?.invoke("⏳ ${currentModel.label} 부팅 중...")
        Thread {
            val start = System.currentTimeMillis()
            while (engineRunning && !nativeIsBootDone()) {
                Thread.sleep(100)
            }
            if (!engineRunning) return@Thread
            val elapsed = System.currentTimeMillis() - start
            Log.i(TAG, "부팅 완료: ${elapsed}ms (device=${currentModel.label})")
            onStatus?.invoke("✅ ${currentModel.label} 준비됨 (${elapsed}ms) — MIDI 대기 중")
        }.also { it.isDaemon = true }.start()
    }

    // ── RTP-MIDI (SC55Engine과 동일한 패턴 — RtpMidiSession은 엔진 독립적) ──
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
        // SC55Engine과 동일한 순서(FIX 이력 참고): BY 종료 패킷이 나갈 때까지
        // 네트워크 바인딩을 유지한 뒤 마지막에 해제한다.
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

    // ── MIDI 디스패치 ────────────────────────────────────────────────────
    override fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        if (!bypassWatchdogs) {
            trackSustain(bytes)
            trackNote(bytes)
        }
        trackPartInfo(bytes)
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
        resetPartTracking()
    }

    override fun getNativeSampleRate(): Int = nativeGetSampleRate()

    // SC55Engine과 동일한 정책: 자동/전환 경로에서는 절대 디바이스 전체
    // 리셋(GS Reset 등)을 걸지 않는다. All Sound Off류만 수행한다.
    // 전체 리셋은 hardResetDevice()를 통해 수동 버튼에서만 호출할 것.
    override fun resetEngine(hard: Boolean) {
        nativeResetSynth()
        resetPartTracking()
    }

    /** 수동 "리셋" 버튼 전용 — 디바이스 고유의 완전 리셋(GS Reset 등). */
    fun hardResetDevice() {
        nativeHardReset()
        resetPartTracking()
    }

    fun version(): String = nativeGetVersion()

    // ── 실제 문자 LCD (MT-32/CM-32L류 전용) ─────────────────────────────
    // SC-55 등 그래픽 LCD 기종에서는 88lib가 빈 문자열을 준다(§API 주석
    // "on a graphic display like the SC-8850's") — 그 경우 MainActivity가
    // 자동으로 LED 패널만 보여주도록 hasTextDisplay()로 판별한다.
    fun getLcdText(): String {
        if (!engineRunning) return ""
        return nativeGetDisplayText(0)
    }

    /** 이 기종이 비트맵으로 그릴 수 있는 실제 LCD를 가졌는지 (SC-55/88, SC-8850, MT-32/CM). */
    fun hasGraphicLcd(): Boolean = engineRunning && nativeGetLcdSize() != 0

    fun hasTextDisplay(): Boolean = engineRunning && nativeIsDisplayOn(0) && nativeGetDisplayText(0).isNotBlank()

    // ── 파트/채널 상태 추적 (LED 패널용) ────────────────────────────────
    // 88lib의 C API는 per-channel 상태 조회를 제공하지 않으므로(플레이어
    // 재생 전용 API), SoundFontEngine/SYXG50Engine과 마찬가지로 디스패치되는
    // MIDI를 클라이언트 사이드에서 직접 추적해 PartInfo를 만든다.
    private val channelProgram = IntArray(16) { -1 }       // -1 = 아직 프로그램 체인지 없음(GM 기본값 0으로 표기)
    private var channelActiveMask = 0L

    private fun resetPartTracking() {
        channelProgram.fill(-1)
        channelActiveMask = 0L
    }

    private fun trackPartInfo(bytes: ByteArray) {
        if (bytes.isEmpty() || bytes[0] == 0xF0.toByte()) return
        val st = bytes[0].toInt() and 0xFF
        val ch = st and 0x0F
        when (st and 0xF0) {
            0xC0 -> if (bytes.size >= 2) channelProgram[ch] = bytes[1].toInt() and 0x7F
            0x90 -> if (bytes.size >= 3) {
                val vel = bytes[2].toInt() and 0xFF
                channelActiveMask = if (vel > 0) channelActiveMask or (1L shl ch)
                                     else channelActiveMask and (1L shl ch).inv()
            }
            0x80 -> channelActiveMask = channelActiveMask and (1L shl ch).inv()
        }
    }

    /** MainActivity의 LED 패널이 사용 (SoundFontEngine.getPartInfo()와 동일한 패턴). */
    fun getPartInfo(): PartInfo {
        val names = (0..15).map { ch ->
            val pc = channelProgram[ch]
            if (pc < 0) "--" else "PC%d".format(pc + 1)
        }
        return PartInfo(channelActiveMask, names)
    }

    // ── 서스테인(CC64) 워치독 — SC55Engine과 동일 ───────────────────────
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
        if (value >= 64) sustainOnSince.putIfAbsent(ch, System.currentTimeMillis())
        else sustainOnSince.remove(ch)
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
                            Log.w(TAG, "⚠️ CC64 ch=$ch ${(now - since) / 1000}초째 유지 → 강제 해제")
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

    // ── 노트 워치독 — SC55Engine과 동일 ──────────────────────────────────
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
                            val ch = key / 128; val note = key % 128
                            Log.w(TAG, "⚠️ Note ch=$ch note=$note ${(now - since) / 1000}초째 울림 → 강제 Off")
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
        sustainWatchdog = null
        noteWatchdog = null
        resetPartTracking()
    }
}
