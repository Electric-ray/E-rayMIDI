package com.example.nukedsc55

import android.content.Context
import android.net.*
import android.util.Log
import java.io.File

/**
 * MuntEngine.kt
 * munt(mt32emu) 기반 MT-32/CM-32L 재생 엔진. munt-android 프로젝트에서
 * mt32emu 코어 C++ 라이브러리(cpp/munt/mt32emu/)만 이식하고, RtpMidiSession/
 * UsbMidiManager는 이 프로젝트(nuked-sc55-android)에 이미 있는, SysEx
 * 프래그멘테이션 버그가 수정된 버전을 그대로 재사용한다 — munt-android의
 * 구버전 RtpMidiSession/UsbMidiManager는 절대 가져오지 않는다
 * (통합작업순서.md Phase 0 확정 사항).
 *
 * SC55Engine / SoundFontEngine과 동일한 IEngine 계약을 구현하므로
 * MainActivity는 세 엔진을 동일한 방식으로 전환할 수 있다.
 *
 * 샘플레이트: mt32emu 네이티브 32000Hz 고정 (MuntBridge.cpp에서 자체 AAudio
 * 스트림을 열며, SC-55/SoundFont와 마찬가지로 엔진 전환 시 이전 엔진을
 * 완전히 stop()한 뒤에만 시작되므로 리샘플링 공유 레이어는 불필요).
 *
 * GS Reset 정책: SC-55mk2 경로의 "자동 GS Reset 금지" 결정은 이 엔진과
 * 무관하다 — MT-32 Master Reset SysEx는 mt32emu 코어 자체의 리셋 방식이며
 * SC55Bridge.cpp와 완전히 독립된 코드 경로(MuntBridge.cpp)에서만 처리된다.
 */
class MuntEngine(val ctx: Context) : IEngine {

    companion object {
        private const val TAG = "MuntEngine"
        private const val CTRL_ROM = "MT32_CONTROL.ROM"
        private const val PCM_ROM  = "MT32_PCM.ROM"
        init { System.loadLibrary("munt-jni") }
    }

    // ROM 폴더는 SC-55 ROM(rom_sc55)과 분리 — 통합작업순서.md Phase 4 권장사항
    val ROM_DIR: String = File(
        android.os.Environment.getExternalStoragePublicDirectory(
            android.os.Environment.DIRECTORY_DOWNLOADS
        ), "rom_munt"
    ).absolutePath

    // ── Native 선언 (MuntBridge.cpp, com.example.nukedsc55.MuntEngine 심볼) ──
    external fun nativeInit(ctrlRom: ByteArray, pcmRom: ByteArray): Boolean
    external fun nativeStart()
    external fun nativeStop()
    external fun nativeTerm()
    external fun nativeSendMidi(packed: Int)
    external fun nativeSendSysEx(data: ByteArray, len: Int)
    external fun nativeResetSynth()
    external fun nativeGetStats(): String
    external fun nativeGetSampleRate(): Int

    private var rtpSession: RtpMidiSession? = null
    private var usbMgr: UsbMidiManager? = null
    private var netCallback: ConnectivityManager.NetworkCallback? = null

    override var onStatus: ((String) -> Unit)? = null
    override var engineRunning = false

    // ── ROM 파일 존재 확인 (SC55Engine.getRomFileList()와 동일한 형태로
    //    MainActivity가 세 엔진을 동일하게 다룰 수 있게 함) ────────────────
    fun getRomFileList(): List<String> = listOf(CTRL_ROM, PCM_ROM)

    fun getMissingRoms(): List<String> =
        getRomFileList().filter { !File(ROM_DIR, it).exists() }

    fun getRomHelpText(): String = buildString {
        appendLine("📂 ROM 파일 경로:")
        appendLine("$ROM_DIR/")
        appendLine()
        appendLine("  1. $CTRL_ROM")
        appendLine("  2. $PCM_ROM")
        appendLine()
        appendLine("※ ROM은 저작권 보호 대상입니다.")
    }

    // ── 엔진 초기화 ──────────────────────────────────────────────────────
    fun initEngine(): Boolean {
        if (engineRunning) return true
        val missing = getMissingRoms()
        if (missing.isNotEmpty()) {
            onStatus?.invoke("❌ 없는 ROM: ${missing.joinToString(", ")}")
            return false
        }
        val ctrl = File(ROM_DIR, CTRL_ROM).readBytes()
        val pcm  = File(ROM_DIR, PCM_ROM).readBytes()
        onStatus?.invoke("⏳ munt(MT-32) 초기화 중...")
        val ok = nativeInit(ctrl, pcm)
        if (!ok) { onStatus?.invoke("❌ munt 초기화 실패"); return false }
        // MuntBridge.cpp의 nativeInit()은 ROM 로드+synth open+AAudio 시작까지
        // 한 번에 수행한다(원본 munt-android 구조 유지) — nativeStart()는
        // 다른 두 엔진과의 호출 순서 일관성을 위한 no-op 래퍼.
        nativeStart()
        engineRunning = true
        onStatus?.invoke("✅ munt(MT-32) 준비됨 — MIDI 대기 중")
        return true
    }

    // ── RTP-MIDI (이 프로젝트의 검증된 RtpMidiSession 재사용) ────────────
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
        val cm = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        netCallback?.let { try { cm.unregisterNetworkCallback(it) } catch (_: Exception) {} }
        netCallback = null
        try { cm.bindProcessToNetwork(null) } catch (_: Exception) {}
        rtpSession?.stop(); rtpSession = null
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

    // ── MT-32 SysEx 캐시 (재연결 후 악기 복구, munt-android 원본 기능 유지) ──
    private val mt32SysexCache = mutableListOf<ByteArray>()
    private val mt32CacheLock  = Any()

    // ── MIDI 디스패치 ────────────────────────────────────────────────────
    override fun dispatchMidi(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        if (bytes[0] == 0xF0.toByte()) {
            if (bytes.size >= 4 && (bytes[3].toInt() and 0xFF) == 0x16) {
                synchronized(mt32CacheLock) { mt32SysexCache.add(bytes.copyOf()) }
                Log.d(TAG, "MT-32 SysEx 캐시: ${bytes.size}B (총 ${mt32SysexCache.size}개)")
            }
            nativeSendSysEx(bytes, bytes.size)
        } else {
            val st = bytes.getOrElse(0) { 0 }.toInt() and 0xFF
            val d1 = bytes.getOrElse(1) { 0 }.toInt() and 0xFF
            val d2 = bytes.getOrElse(2) { 0 }.toInt() and 0xFF
            nativeSendMidi(st or (d1 shl 8) or (d2 shl 16))
        }
    }

    override fun allNotesOff() {
        for (ch in 0..8) {
            nativeSendMidi((0xB0 or ch) or (123 shl 8))
            nativeSendMidi((0xB0 or ch) or (120 shl 8))
        }
    }

    override fun getNativeSampleRate(): Int = nativeGetSampleRate()

    // hard=true → MT-32 Master Reset SysEx까지 재생. SC-55 경로의 GS Reset
    // 금지 정책과는 무관한, munt 전용 리셋 경로(MuntBridge.cpp)이다.
    override fun resetEngine(hard: Boolean) {
        nativeResetSynth()
        if (hard) replaySysexCache()
        onStatus?.invoke(if (hard) "🔄 MT-32 리셋 완료 — 게임 음악을 재시작하세요" else "🔇 All Notes/Sound Off")
    }

    private fun replaySysexCache() {
        val cache = synchronized(mt32CacheLock) { mt32SysexCache.toList() }
        if (cache.isEmpty()) return
        Log.i(TAG, "MT-32 SysEx 재생: ${cache.size}개")
        onStatus?.invoke("🔄 MT-32 상태 복구 중 (${cache.size}개 SysEx 재생)")
        cache.forEach { nativeSendSysEx(it, it.size) }
    }

    fun stats(): String = nativeGetStats()

    // ── UI용 악기명 패널 텍스트 (SC-55 LCD를 대체하는 간단한 텍스트 표시)
    // MT-32 관습: Part1-8 = MIDI 채널 2-9, Rhythm = 채널 10 (채널 1은 미사용)
    fun getPartPanelText(): String {
        val stats = nativeGetStats()
        val namesLine = stats.lineSequence().firstOrNull { it.startsWith("names:") }
            ?: return "악기 정보 대기 중…"
        val names = namesLine.removePrefix("names:").split(",")
        return buildString {
            for (i in names.indices) {
                val label = if (i < 8) "CH${i + 2} (Part${i + 1})" else "CH10 (Rhythm)"
                appendLine("$label: ${names.getOrElse(i) { "---" }.trim()}")
            }
        }
    }

    override fun stop() {
        stopRtp(); stopUsb()
        if (engineRunning) {
            nativeStop(); nativeTerm()
            engineRunning = false
        }
    }
}
