package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

/**
 * MU2000Engine — tarboh/S-MU2000(SH7042+SWP30x2, YAMAHA MU2000 LLE 에뮬레이터)를
 * 감싼 IEngine 구현체. 고정 커밋 d44b0891cb9550567db818c269122184be6fe158.
 *
 * GearmulatorEngine과의 핵심 차이:
 *  - MIDI가 3바이트 패킹이 아니라 바이트 스트림(SysEx 포함 그대로 흘려보냄).
 *  - ROM은 88lib처럼 내용 기반 자동인식이 아니라 **정확한 파일명/폴더 구조**가
 *    필요하다 (프로그램 ROM은 아무 이름이나 되지만, 웨이브 ROM 폴더 안의
 *    4개 파일명은 정확히 일치해야 함 — getRomHelpText() 참고).
 *  - 별도의 "부팅 완료까지 블로킹" 단계가 없다. reset() 후 렌더 스레드가
 *    바로 재생을 시작하고, 그 자체가 부팅 과정이다. 부팅 중엔 MIDI가
 *    무시되므로 isBootReady()로 안내만 한다.
 */
class MU2000Engine(val ctx: Context) : IEngine {

    companion object {
        private const val TAG = "MU2000Engine"
        init { System.loadLibrary("mu2000-jni") }
    }

    val ROM_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "rom_mu2000"
    ).absolutePath

    // ── Native 선언 ──────────────────────────────────────────────────────
    external fun nativeInit(programPath: String, waveDir: String, sintabPath: String, lcdFontPath: String): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeRestartAudio()
    override fun restartAudio() { if (engineRunning) nativeRestartAudio() }
    external fun nativeTerm()
    external fun nativeSendMidi(data: ByteArray, len: Int)
    external fun nativeAllSoundOff()
    external fun nativeIsBootDone(): Boolean
    external fun nativeGetSampleRate(): Int
    external fun nativeGetLastError(): String
    // 실제 기기 LCD 프레임: (width shl 16) or height, 아직 없으면 0
    external fun nativeGetLcdSize(): Int
    // 화면 내용이 바뀔 때마다 증가 (같으면 다시 그릴 필요 없음)
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

    fun getRomHelpText(): String = buildString {
        appendLine("📂 ROM 폴더: $ROM_DIR/")
        appendLine()
        appendLine("1) 프로그램 ROM (4MB, 파일명 무관)")
        appendLine("   → $ROM_DIR/mu2000_flash.bin")
        appendLine()
        appendLine("2) 웨이브 ROM 4개 (총 32MB, 파일명이 정확히 일치해야 함)")
        appendLine("   → $ROM_DIR/dump/xv364a0.ic49")
        appendLine("   → $ROM_DIR/dump/xv365a0.ic50")
        appendLine("   → $ROM_DIR/dump/xw848a0.ic53")
        appendLine("   → $ROM_DIR/dump/xw849a0.ic54")
        appendLine()
        appendLine("3) (선택) sin 테이블 64KB — 없어도 동작함")
        appendLine("   → $ROM_DIR/standin/sin-table.bin")
        appendLine()
        appendLine("4) (선택) LCD 문자 ROM 4KB hd44780u_b04.bin — 없으면 내장 폰트로 LCD 표시")
        appendLine("   → $ROM_DIR/hd44780u_b04.bin")
        appendLine()
        appendLine("※ ROM은 저작권 보호 대상입니다.")
    }

    private fun romPaths(): Quad {
        val base = File(ROM_DIR)
        val program = File(base, "mu2000_flash.bin")
        val waveDir = File(base, "dump")
        val sintab = File(base, "standin/sin-table.bin")
        // LCD 문자 ROM(4KB). S-MU2000 본가와 같은 위치들을 찾는다. 없으면 네이티브가 내장 폰트를 쓴다.
        val font = listOf(File(base, "hd44780u_b04.bin"), File(base, "standin/hd44780u_b04.bin"),
            File(waveDir, "hd44780u_b04.bin")).firstOrNull { it.isFile && it.length() == 0x1000L }
        return Quad(program.absolutePath, waveDir.absolutePath,
            if (sintab.exists()) sintab.absolutePath else "", font?.absolutePath ?: "")
    }
    private data class Quad(val a: String, val b: String, val c: String, val d: String)

    fun isRomAvailable(): Boolean {
        val base = File(ROM_DIR)
        val program = File(base, "mu2000_flash.bin")
        val waveDir = File(base, "dump")
        if (!program.exists() || !waveDir.isDirectory) return false
        val need = listOf("xv364a0.ic49", "xv365a0.ic50", "xw848a0.ic53", "xw849a0.ic54")
        return need.all { File(waveDir, it).exists() }
    }

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(): Boolean {
        if (engineRunning) return true
        if (!isRomAvailable()) {
            onStatus?.invoke("❌ MU2000 ROM이 없습니다 — ROM 도움말을 확인하세요")
            return false
        }
        val (program, waveDir, sintab, lcdFont) = romPaths()
        val ok = nativeInit(program, waveDir, sintab, lcdFont)
        if (!ok) { onStatus?.invoke("❌ MU2000 초기화 실패"); return false }
        nativeStart()
        engineRunning = true
        startBootMonitor()
        startSustainWatchdog()
        startNoteWatchdog()
        return true
    }

    private fun startBootMonitor() {
        onStatus?.invoke("⏳ MU2000 부팅 중...")
        Thread {
            val start = System.currentTimeMillis()
            while (engineRunning && !nativeIsBootDone()) {
                Thread.sleep(50)
            }
            if (!engineRunning) return@Thread
            val elapsed = System.currentTimeMillis() - start
            Log.i(TAG, "부팅 완료: ${elapsed}ms")
            onStatus?.invoke("✅ MU2000 준비됨 (${elapsed}ms) — MIDI 대기 중")
        }.also { it.isDaemon = true }.start()
    }

    // ── RTP-MIDI ─────────────────────────────────────────────────────────
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

    // ── MIDI 디스패치 ────────────────────────────────────────────────────
    // S-MU2000은 바이트 스트림 방식 — SysEx도 별도 분기 없이 그대로 넘긴다.
    override fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        if (!bypassWatchdogs) {
            trackSustain(bytes)
            trackNote(bytes)
        }
        trackPartInfo(bytes)
        nativeSendMidi(bytes, bytes.size)
    }

    override fun allNotesOff() {
        nativeAllSoundOff()
        resetPartTracking()
    }

    override fun getNativeSampleRate(): Int = nativeGetSampleRate()

    // S-MU2000에는 "소프트/하드 리셋" 구분 API가 없다 — All Sound Off류로
    // 갈음한다 (다른 엔진들의 "자동 경로에서는 전체 리셋을 걸지 않는다"는
    // 정책과 자연스럽게 맞음). 실제 GS/XG 리셋 SysEx는 곡 파일이 직접
    // 보내는 것을 그대로 통과시킨다.
    override fun resetEngine(hard: Boolean) {
        nativeAllSoundOff()
        resetPartTracking()
    }

    fun version(): String = "S-MU2000 (d44b089)"

    // ── 파트/채널 상태 추적 (LED 패널용, GearmulatorEngine과 동일한 패턴) ──
    private val channelProgram = IntArray(16) { -1 }
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

    fun getPartInfo(): PartInfo {
        val names = (0..15).map { ch ->
            val pc = channelProgram[ch]
            if (pc < 0) "--" else "PC%d".format(pc + 1)
        }
        return PartInfo(channelActiveMask, names)
    }

    // ── 서스테인(CC64) 워치독 ────────────────────────────────────────────
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
                            nativeSendMidi(byteArrayOf((0xB0 or ch).toByte(), 64, 0), 3)
                            it.remove()
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "서스테인 워치독 예외(계속진행): $e")
                }
            }
        }.also { it.isDaemon = true; it.start() }
    }

    // ── 노트 워치독 ──────────────────────────────────────────────────────
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
                            nativeSendMidi(byteArrayOf((0x80 or ch).toByte(), note.toByte(), 0), 3)
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
