package com.example.nukedsc55

import java.util.concurrent.locks.LockSupport

/**
 * MidiFilePlayer.kt — 파싱된 MIDI 곡(MidiSequence) 한 곡을 재생하는 시퀀서.
 *
 * "MIDI 파일 재생"을 새 오디오 경로로 만들지 않고, RTP-MIDI/USB와 똑같은 "또 하나의 MIDI 입력원"으로
 * 취급한다: 정해진 시각이 되면 메시지를 IEngine.dispatchMidi()에 넣을 뿐이다. 그래서 SC-55 / MT-32 /
 * SoundFont / S-YXG50 네 엔진이 코드 수정 없이 모두 파일을 재생할 수 있다.
 *
 * 안드로이드 API를 쓰지 않는다(스레드 우선순위는 threadInit으로 주입) → JVM에서 그대로 테스트 가능.
 *
 * 스레드 모델
 *  - 재생 중에는 전용 스레드 1개가 시각에 맞춰 메시지를 보낸다.
 *  - 일시정지/정지/시크/다른 곡 로드는 session 번호를 올려서 이전 스레드가 스스로 물러나게 한다.
 *    메시지 전송과 session 확인은 같은 lock 안에서 하므로, 정지 직후에 Note On이 새어 나가 음이 걸리는 일이 없다.
 *
 * 볼륨
 *  엔진 네이티브 게인은 Kotlin에서 접근할 수 없으므로, 파일 재생 볼륨은 채널 볼륨(CC7)에 마스터 비율을 곱하는
 *  방식으로 구현했다. 곡이 보내는 CC7은 가로채서 비율을 곱해 전달하고, 슬라이더를 움직이면 16채널에
 *  CC7을 다시 보낸다. (곡이 정한 값보다 키울 수는 없고 줄이기만 한다. 라이브 입력에는 영향 없음)
 */
class MidiFilePlayer(
    private val engineProvider: () -> IEngine?,
    private val threadInit: () -> Unit = {}
) {
    enum class State { STOPPED, PLAYING, PAUSED }

    companion object {
        private const val END_PAD_US = 1_000_000L   // 마지막 이벤트 뒤 잔향이 사라질 여유
        private const val MAX_PARK_NS = 20_000_000L // 대기 중에도 20ms마다 정지/일시정지 요청 확인
    }

    @Volatile var state: State = State.STOPPED
        private set

    /** 곡이 끝까지 자연스럽게 재생되었을 때만 호출 (정지/일시정지/다른 곡 전환에서는 호출 안 됨). 재생 스레드에서 호출됨. */
    @Volatile var onFinished: (() -> Unit)? = null

    private val lock = Any()
    @Volatile private var session = 0
    @Volatile private var startNano = 0L      // PLAYING 중: 곡 위치 0에 해당하는 System.nanoTime() 값
    private var pausedPosUs = 0L
    @Volatile private var seq: MidiSequence? = null
    private val chanVol = IntArray(16) { 100 } // 곡이 마지막으로 지정한 채널별 CC7 (GM 기본값 100)
    @Volatile private var master = 100          // 0..100 (%)

    val hasSequence: Boolean get() = seq != null
    val durationMs: Long get() = (seq?.durationUs ?: 0L) / 1000L
    val positionMs: Long get() = positionUs() / 1000L
    val volume: Int get() = master

    // ── 공개 API ─────────────────────────────────────────────────────────

    /** 곡을 불러온다 (재생 중이던 곡은 정지). 재생은 play()로 시작. */
    fun load(s: MidiSequence) {
        synchronized(lock) {
            if (state != State.STOPPED) haltLocked()
            seq = s
            pausedPosUs = 0L
        }
    }

    /** 정지 상태면 처음부터, 일시정지 상태면 그 자리부터 재생. */
    fun play() {
        synchronized(lock) {
            val s = seq ?: return
            when (state) {
                State.PLAYING -> {}
                State.PAUSED -> startWorkerLocked(s, pausedPosUs)
                State.STOPPED -> { prepareSongLocked(); startWorkerLocked(s, 0L) }
            }
        }
    }

    fun pause() {
        synchronized(lock) {
            if (state != State.PLAYING) return
            pausedPosUs = positionUs()
            session++
            state = State.PAUSED
            engineProvider()?.allNotesOff()
        }
    }

    fun stop() {
        synchronized(lock) { haltLocked() }
    }

    fun seekTo(posMs: Long) {
        synchronized(lock) {
            val s = seq ?: return
            val target = (posMs * 1000L).coerceIn(0L, s.durationUs)
            val wasPlaying = state == State.PLAYING
            if (state == State.STOPPED) prepareSongLocked()
            session++
            engineProvider()?.allNotesOff()
            chaseLocked(s, target)
            pausedPosUs = target
            if (wasPlaying) startWorkerLocked(s, target) else state = State.PAUSED
        }
    }

    /** 파일 재생 볼륨 0..100(%). */
    fun setVolume(percent: Int) {
        synchronized(lock) {
            master = percent.coerceIn(0, 100)
            if (state != State.STOPPED) {
                val e = engineProvider() ?: return
                for (ch in 0 until 16) e.dispatchMidi(cc7(ch))
            }
        }
    }

    // ── 내부 구현 ────────────────────────────────────────────────────────

    private fun positionUs(): Long {
        val s = seq ?: return 0L
        return when (state) {
            State.PLAYING -> ((System.nanoTime() - startNano) / 1000L).coerceIn(0L, s.durationUs)
            State.PAUSED -> pausedPosUs
            State.STOPPED -> 0L
        }
    }

    private fun haltLocked() {
        session++
        state = State.STOPPED
        pausedPosUs = 0L
        engineProvider()?.allNotesOff()
    }

    /** 곡을 처음부터 재생하기 직전: 소리 정리 + 컨트롤러 초기화 + 채널 볼륨을 기본값(100)에 마스터 비율 적용. */
    private fun prepareSongLocked() {
        val e = engineProvider() ?: return
        e.allNotesOff()
        for (ch in 0 until 16) chanVol[ch] = 100
        for (ch in 0 until 16) {
            e.dispatchMidi(byteArrayOf((0xB0 or ch).toByte(), 121, 0)) // Reset All Controllers
            e.dispatchMidi(cc7(ch))
        }
    }

    private fun startWorkerLocked(s: MidiSequence, posUs: Long) {
        session++
        val my = session
        pausedPosUs = posUs
        val t0 = System.nanoTime() - posUs * 1000L
        startNano = t0
        state = State.PLAYING
        val startIdx = lowerBound(s.timesUs, posUs)
        Thread({
            threadInit()
            runLoop(s, my, t0, startIdx)
        }, "MidiFilePlayer").apply { isDaemon = true; start() }
    }

    private fun runLoop(s: MidiSequence, my: Int, t0: Long, startIdx: Int) {
        val n = s.messages.size
        val times = s.timesUs
        val endUs = maxOf(s.durationUs, if (n > 0) times[n - 1] else 0L) + END_PAD_US
        var i = startIdx
        while (true) {
            if (session != my) return
            val target = t0 + (if (i < n) times[i] else endUs) * 1000L
            val wait = target - System.nanoTime()
            if (wait > 0L) {
                LockSupport.parkNanos(minOf(wait, MAX_PARK_NS))
                continue
            }
            if (i >= n) break
            synchronized(lock) {
                if (session != my) return
                sendLocked(s.messages[i])
            }
            i++
        }
        // 곡이 끝까지 재생됨
        val done = synchronized(lock) {
            if (session != my) false else { state = State.STOPPED; pausedPosUs = 0L; true }
        }
        if (done) {
            engineProvider()?.allNotesOff() // 끝나지 않은 노트/서스테인 정리
            onFinished?.invoke()
        }
    }

    /** lock 안에서만 호출. 곡이 보내는 CC7은 가로채서 마스터 볼륨 비율을 곱해 전달한다. */
    private fun sendLocked(m: ByteArray) {
        val e = engineProvider() ?: return
        if (m.size == 3 && (m[0].toInt() and 0xF0) == 0xB0 && m[1].toInt() == 7) {
            val ch = m[0].toInt() and 0x0F
            chanVol[ch] = m[2].toInt() and 0x7F
            e.dispatchMidi(cc7(ch))
        } else {
            e.dispatchMidi(m)
        }
    }

    private fun cc7(ch: Int): ByteArray =
        byteArrayOf((0xB0 or ch).toByte(), 7, ((chanVol[ch] * master + 50) / 100).toByte())

    /**
     * 시크 후 재생 위치의 "악기 상태"를 복구한다: target 이전의 컨트롤러/프로그램/피치벤드/SysEx를
     * (노트는 제외하고) 다시 보낸다. 같은 컨트롤러가 여러 번 나오면 마지막 값만 보내고, 발생 순서는 유지한다.
     */
    private fun chaseLocked(s: MidiSequence, targetUs: Long) {
        val end = lowerBound(s.timesUs, targetUs)
        val latest = LinkedHashMap<Long, ByteArray>()
        var sysexId = 0L
        for (i in 0 until end) {
            val m = s.messages[i]
            val st = m[0].toInt() and 0xFF
            val hi = st and 0xF0
            when {
                st == 0xF0 -> { sysexId++; latest[-sysexId] = m }
                hi == 0xB0 && m.size >= 3 -> {
                    val key = (st.toLong() shl 8) or (m[1].toLong() and 0x7F)
                    latest.remove(key); latest[key] = m
                }
                hi == 0xC0 || hi == 0xE0 -> {
                    val key = (st.toLong() shl 8) or 0x80L
                    latest.remove(key); latest[key] = m
                }
            }
        }
        for (m in latest.values) sendLocked(m)
    }

    /** timesUs(오름차순)에서 key 이상인 첫 인덱스. */
    private fun lowerBound(a: LongArray, key: Long): Int {
        var lo = 0
        var hi = a.size
        while (lo < hi) {
            val mid = (lo + hi) ushr 1
            if (a[mid] < key) lo = mid + 1 else hi = mid
        }
        return lo
    }
}
