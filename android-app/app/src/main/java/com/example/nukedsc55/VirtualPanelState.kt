package com.example.nukedsc55

import android.graphics.Bitmap
import android.os.SystemClock
import kotlin.math.exp
import kotlin.math.max
import kotlin.math.min

/**
 * 가상 패널 상태 — MIDI 스트림을 보고 "MU2000 스타일 LCD"에 필요한 값을 추적한다.
 *
 * LCD가 없는 엔진(SoundFont/FluidSynth, SC-8820)은 펌웨어가 그려 주는 화면이 없으므로, 앱이
 * 같은 MIDI 입력을 한 번 더 읽어서 파트별로 이런 값을 따라간다:
 *   레벨미터(노트온 벨로시티를 올리고 시간에 따라 감쇠), 프로그램/뱅크, 볼륨(CC7), 익스프레션(CC11),
 *   팬(CC10), 리버브(CC91), 코러스(CC93), 베리에이션(CC94).
 * 표시 중심 파트("포커스")는 **한 번 정해지면 그대로 유지**한다 — 노트가 울릴 때마다 따라다니면 여러 파트가
 * 번갈아 울리는 곡에서 음색명/리버브/악기 모양이 계속 뒤바뀌어 어지럽기 때문이다. S-MU2000 화면이 선택된
 * 파트 하나를 보여 주면서 레벨미터만 전체 파트가 움직이는 것과 같은 방식이다. 처음 노트가 울린 파트(드럼
 * 제외)가 포커스가 되고, 그 파트가 12초 넘게 조용했을 때만 새로 소리 나는 파트로 옮긴다. GM/GS/XG 리셋(새 곡)이
 * 오면 처음부터 다시 정한다. 화면을 탭하면 파트를 1→16 순서로 직접 고를 수 있다(길게 누르면 자동 선택으로 복귀).
 *
 * onMidi()는 MIDI 스레드에서, tick()/render()는 LCD 백그라운드 스레드에서 불린다.
 *
 * @param nameOf 파트의 음색명을 돌려준다 (SoundFont는 FluidSynth 프리셋명, 그 외는 "PC 001" 등)
 */
class VirtualPanelState(private val nameOf: (channel: Int) -> String) {

    private companion object {
        /** 표시 중인 파트가 이만큼 조용하면 새로 소리 나는 파트로 포커스를 옮긴다 */
        const val FOCUS_IDLE_MS = 12_000L
    }

    private val lock = Any()
    private val bank = IntArray(16)
    private val prog = IntArray(16)
    private val vol = IntArray(16) { 100 }
    private val expr = IntArray(16) { 127 }
    private val pan = IntArray(16) { 64 }
    private val rev = IntArray(16) { 40 }
    private val cho = IntArray(16)
    private val vr = IntArray(16)
    private val level = FloatArray(16)
    private val held = IntArray(16)

    // -1 = 아직 정해지지 않음(첫 노트를 기다리는 중 — 그동안은 파트 1을 보여 준다)
    @Volatile private var focus = -1
    private var manual = false
    private val lastNoteAt = LongArray(16)
    private var lastTick = 0L
    private var seq = 0L
    private var lastHash = 0
    private val params = IntArray(32)

    // 음색명은 네이티브 호출이라 포커스가 바뀌었거나 250ms가 지났을 때만 갱신한다
    private var nameBytes = ByteArray(8) { 0x20 }
    private var nameCh = -1
    private var nameAt = 0L

    /** 0 없음, 1 XG, 2 GS, 3 PERFORM — LCD의 모드 표시 ▶ */
    @Volatile var mode = 0

    fun onMidi(b: ByteArray) {
        if (b.isEmpty()) return
        val st = b[0].toInt() and 0xFF
        if (st == 0xF0) { if (isResetSysEx(b)) synchronized(lock) { resetAll() }; return }
        if (st < 0x80 || st >= 0xF0) return
        val ch = st and 0x0F
        val d1 = if (b.size > 1) b[1].toInt() and 0x7F else 0
        val d2 = if (b.size > 2) b[2].toInt() and 0x7F else 0
        synchronized(lock) {
            when (st and 0xF0) {
                0x90 -> if (d2 > 0) noteOn(ch, d2) else noteOff(ch)
                0x80 -> noteOff(ch)
                0xB0 -> when (d1) {
                    0 -> bank[ch] = d2
                    7 -> vol[ch] = d2
                    10 -> pan[ch] = d2
                    11 -> expr[ch] = d2
                    91 -> rev[ch] = d2
                    93 -> cho[ch] = d2
                    94 -> vr[ch] = d2
                    120, 123 -> held[ch] = 0
                }
                0xC0 -> prog[ch] = d1   // 포커스는 옮기지 않는다 (표시 중인 파트의 프로그램이 바뀔 때만 화면이 바뀜)
            }
        }
    }

    private fun noteOn(ch: Int, vel: Int) {
        held[ch]++
        val lv = vel / 127f * (0.35f + 0.65f * vol[ch] / 127f)
        if (lv > level[ch]) level[ch] = lv
        val now = SystemClock.uptimeMillis()
        if (!manual) {
            val f = focus
            val move = when {
                f < 0 -> true                               // 처음 소리 난 파트
                ch == f -> false
                f == 9 -> ch != 9                           // 드럼에 머물러 있다가 멜로디 파트가 나오면 옮긴다
                ch == 9 -> false                            // 드럼 때문에는 옮기지 않는다
                else -> now - lastNoteAt[f] > FOCUS_IDLE_MS // 지금 파트가 한참 조용했을 때만 옮긴다
            }
            if (move) focus = ch
        }
        lastNoteAt[ch] = now
    }

    private fun isResetSysEx(b: ByteArray): Boolean {
        fun at(i: Int) = if (i < b.size) b[i].toInt() and 0xFF else -1
        val gm = at(1) == 0x7E && at(3) == 0x09 && at(4) == 0x01                                   // GM System On
        val gs = at(1) == 0x41 && at(3) == 0x42 && at(4) == 0x12 && at(5) == 0x40 && at(6) == 0x00 && at(7) == 0x7F  // GS Reset
        val xg = at(1) == 0x43 && at(3) == 0x4C && at(4) == 0x00 && at(5) == 0x00 && at(6) == 0x7E  // XG System On
        return gm || gs || xg
    }

    /** 새 곡/리셋: 파트별 값과 레벨을 초기화하고 포커스를 다시 정한다 (사용자가 직접 고른 파트는 유지) */
    private fun resetAll() {
        bank.fill(0); prog.fill(0); vol.fill(100); expr.fill(127); pan.fill(64)
        rev.fill(40); cho.fill(0); vr.fill(0)
        level.fill(0f); held.fill(0); lastNoteAt.fill(0L)
        if (!manual) focus = -1
    }

    private fun cur(): Int = if (focus < 0) 0 else focus

    private fun noteOff(ch: Int) {
        if (held[ch] > 0) held[ch]--
    }

    /** 모든 소리를 끌 때(All Notes Off, 엔진 정지 등) 레벨미터도 즉시 내린다 */
    fun silence() = synchronized(lock) {
        held.fill(0)
        level.fill(0f)
    }

    /** 화면을 탭: 다음 파트를 직접 고른다 (1→16→1 ...) */
    fun cycleFocus() = synchronized(lock) {
        manual = true
        focus = (cur() + 1) % 16
    }

    /** 길게 누르기: 마지막으로 소리 난 파트를 자동으로 따라간다 */
    fun autoFocus() = synchronized(lock) { manual = false }

    /**
     * LCD 스레드가 ~30fps로 부른다. 레벨미터를 시간에 따라 내리고, 화면에 보이는 값이 달라졌으면
     * 카운터를 올려서 돌려준다 (LcdFramePump는 카운터가 같으면 다시 그리지 않는다).
     */
    fun tick(): Long {
        val now = SystemClock.uptimeMillis()
        refreshName(now)
        synchronized(lock) {
            val dt = if (lastTick == 0L) 33L else min(200L, now - lastTick)
            lastTick = now
            for (c in 0 until 16) {
                // 키를 누르고 있으면 천천히, 뗐으면 빠르게 감쇠
                val tau = if (held[c] > 0) 1600f else 380f
                level[c] *= exp(-dt / tau)
                if (level[c] < 0.004f) level[c] = 0f
            }
            val f = cur()
            var h = f
            for (c in 0 until 16) h = h * 31 + (level[c] * 15f).toInt()
            h = h * 31 + bank[f]; h = h * 31 + prog[f]; h = h * 31 + vol[f]; h = h * 31 + expr[f]
            h = h * 31 + pan[f]; h = h * 31 + rev[f]; h = h * 31 + cho[f]; h = h * 31 + vr[f]
            h = h * 31 + mode
            h = h * 31 + nameBytes.contentHashCode()
            if (h != lastHash) { lastHash = h; seq++ }
            return seq
        }
    }

    private fun refreshName(now: Long) {
        val f = cur()
        if (f == nameCh && now - nameAt < 250L) return
        val s = runCatching { nameOf(f) }.getOrDefault("")
        val out = ByteArray(8) { 0x20 }
        for (i in 0 until min(8, s.length)) {
            val c = s[i].code
            out[i] = if (c in 0x20..0x7E) c.toByte() else '?'.code.toByte()
        }
        synchronized(lock) {
            nameBytes = out
            nameCh = f
            nameAt = now
        }
    }

    fun render(bitmap: Bitmap): Boolean {
        val p: IntArray
        val nm: ByteArray
        synchronized(lock) {
            val f = cur()
            p = params
            p.fill(0)
            p[0] = 1                    // LCD 켜짐
            p[1] = 2                    // 콘트라스트
            p[2] = f + 1                // 파트 번호
            p[3] = bank[f]; p[4] = prog[f]
            p[5] = vol[f]; p[6] = expr[f]; p[7] = pan[f]
            p[8] = rev[f]; p[9] = cho[f]; p[10] = vr[f]
            p[11] = 0                   // KEY(노트 시프트)는 추적하지 않음
            p[12] = mode
            p[13] = if (f == 9) 1 else 0
            for (c in 0 until 16) p[16 + c] = max(0, min(127, (level[c] * 127f).toInt()))
            nm = nameBytes.copyOf()
        }
        return VirtualLcd.renderPanel(bitmap, p, nm)
    }
}
