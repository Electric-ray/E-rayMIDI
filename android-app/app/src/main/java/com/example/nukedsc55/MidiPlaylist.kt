package com.example.nukedsc55

import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger

/** 곡이 끝났을 때 무엇을 할지 (UI의 모드 버튼이 순서대로 돌린다). */
enum class PlayMode(val label: String) {
    SINGLE("➡ 1곡"),            // 고른 곡 1곡만 재생하고 멈춤
    FOLDER("⇒ 목록 전체"),       // 고른 곡부터 목록(폴더 또는 플레이리스트) 끝까지 순서대로 재생하고 멈춤
    LOOP_FOLDER("🔁 목록 반복"), // 목록 전체를 계속 반복
    LOOP_ONE("🔂 1곡 반복");     // 고른 곡만 계속 반복

    fun next(): PlayMode = values()[(ordinal + 1) % values().size]
}

/**
 * MidiPlaylist.kt — 재생 목록(폴더 안의 MIDI 파일들) + 이전/다음/모드 처리.
 *
 * MidiFilePlayer(한 곡 재생)를 감싸서, 파일 읽기/파싱은 별도 스레드에서 하고(UI 멈춤 방지),
 * 곡이 끝나면 PlayMode에 따라 다음 곡을 이어서 재생한다. 안드로이드 API를 쓰지 않는다.
 */
class MidiPlaylist(private val player: MidiFilePlayer) {

    companion object {
        val EXTENSIONS = setOf("mid", "midi", "kar", "rmi")
        private const val MAX_FILE_BYTES = 16L * 1024 * 1024

        /** 폴더 안의 MIDI 파일 목록 (하위 폴더 제외, 숫자를 크기순으로 인식하는 이름 정렬: 2.mid < 10.mid). */
        fun scan(dir: File): List<File> =
            dir.listFiles { f -> f.isFile && f.extension.lowercase() in EXTENSIONS }
                ?.sortedWith(Comparator { a, b -> compareNatural(a.name.lowercase(), b.name.lowercase()) })
                ?: emptyList()

        fun compareNatural(a: String, b: String): Int {
            var i = 0
            var j = 0
            while (i < a.length && j < b.length) {
                val ca = a[i]
                val cb = b[j]
                if (ca.isDigit() && cb.isDigit()) {
                    var ie = i
                    while (ie < a.length && a[ie].isDigit()) ie++
                    var je = j
                    while (je < b.length && b[je].isDigit()) je++
                    val na = a.substring(i, ie).trimStart('0')
                    val nb = b.substring(j, je).trimStart('0')
                    if (na.length != nb.length) return na.length - nb.length
                    val c = na.compareTo(nb)
                    if (c != 0) return c
                    i = ie
                    j = je
                } else {
                    if (ca != cb) return ca.compareTo(cb)
                    i++
                    j++
                }
            }
            return (a.length - i) - (b.length - j)
        }
    }

    @Volatile var files: List<File> = emptyList()
        private set
    @Volatile var index: Int = 0
        private set
    @Volatile var mode: PlayMode = PlayMode.FOLDER

    /** 재생할 곡이 정해질 때 호출 (UI 제목 갱신용). 어느 스레드에서든 호출될 수 있다. */
    @Volatile var onTrackChanged: ((Int, File) -> Unit)? = null
    /** 안내/오류 메시지 (예: 깨진 파일). */
    @Volatile var onMessage: ((String) -> Unit)? = null

    private val requestId = AtomicInteger(0)
    private val failStreak = AtomicInteger(0)
    private val loader = Executors.newSingleThreadExecutor { r ->
        Thread(r, "MidiPlaylistLoader").apply { isDaemon = true }
    }

    init {
        player.onFinished = { onNaturalEnd() }
    }

    /** 재생 목록을 바꾼다 (재생은 시작하지 않음). */
    fun setQueue(list: List<File>, startIndex: Int = 0) {
        requestId.incrementAndGet()
        player.stop()
        files = list
        index = if (list.isEmpty()) 0 else startIndex.coerceIn(0, list.size - 1)
        failStreak.set(0)
    }

    /**
     * 재생을 끊지 않고 목록만 바꾼다 (플레이리스트 편집: 추가/삭제/순서 변경/비우기용).
     * 지금 곡이 새 목록에 남아 있으면 index만 그 위치로 맞추고 소리는 그대로 이어진다.
     * 지금 곡이 목록에서 빠졌으면 같은 자리에 온 곡(= 다음 곡)으로 넘어간다 — 재생/일시정지 중이었을 때만 재생하고,
     * 목록이 비면 정지한다. 같은 곡은 목록에 한 번만 들어간다는 전제로 경로(absolutePath)로 현재 곡을 찾는다.
     */
    fun updateQueue(newList: List<File>) {
        val cur = files.getOrNull(index)
        val wasActive = player.state != MidiFilePlayer.State.STOPPED
        files = newList
        if (newList.isEmpty()) {
            stop()
            index = 0
            return
        }
        val pos = if (cur == null) -1 else newList.indexOfFirst { it.absolutePath == cur.absolutePath }
        if (pos >= 0) {
            index = pos
            return
        }
        index = index.coerceIn(0, newList.size - 1)
        if (wasActive) playIndex(index) else onTrackChanged?.invoke(index, newList[index])
    }

    /** ▶/⏸ 버튼: 재생 중이면 일시정지, 일시정지면 이어서, 정지 상태면 현재 곡을 처음부터. */
    fun playPause() {
        if (player.state == MidiFilePlayer.State.PLAYING) pause() else play()
    }

    /**
     * 명시적 "재생" (블루투스/유선 리모컨의 PLAY 전용 버튼, 미디어 세션의 onPlay() 등 —
     * 토글이 아니라 반드시 재생 쪽으로만 가야 하는 호출용). 이미 재생 중이면 아무 것도 안 한다.
     */
    fun play() {
        when (player.state) {
            MidiFilePlayer.State.PLAYING -> {}
            MidiFilePlayer.State.PAUSED -> player.play()
            MidiFilePlayer.State.STOPPED -> playIndex(index)
        }
    }

    /** 명시적 "일시정지" (PAUSE 전용 버튼, 미디어 세션의 onPause() 등). 재생 중이 아니면 무시. */
    fun pause() {
        if (player.state == MidiFilePlayer.State.PLAYING) player.pause()
    }

    fun playIndex(i: Int) {
        val list = files
        if (i !in list.indices) return
        index = i
        val file = list[i]
        val req = requestId.incrementAndGet()
        player.stop() // 지금 나던 소리는 즉시 정리
        onTrackChanged?.invoke(i, file)
        loader.execute {
            if (req != requestId.get()) return@execute
            val seq = try {
                if (file.length() > MAX_FILE_BYTES) throw SmfParseException("파일이 너무 큼")
                SmfParser.parse(file.readBytes())
            } catch (e: Throwable) {
                if (req == requestId.get()) onLoadFailed(i, file, e)
                return@execute
            }
            if (req != requestId.get()) return@execute
            failStreak.set(0)
            player.load(seq)
            player.play()
        }
    }

    fun stop() {
        requestId.incrementAndGet()
        player.stop()
    }

    fun next() {
        val n = files.size
        if (n == 0) return
        playIndex((index + 1) % n)
    }

    /** 3초 넘게 재생했으면 현재 곡 처음으로, 아니면 이전 곡으로. */
    fun prev() {
        val n = files.size
        if (n == 0) return
        if (player.state != MidiFilePlayer.State.STOPPED && player.positionMs > 3000L) playIndex(index)
        else playIndex((index - 1 + n) % n)
    }

    fun shutdown() {
        requestId.incrementAndGet()
        player.onFinished = null
        player.stop()
        loader.shutdownNow()
    }

    private fun onNaturalEnd() {
        val n = files.size
        val i = index
        when (mode) {
            PlayMode.SINGLE -> {}
            PlayMode.LOOP_ONE -> playIndex(i)
            PlayMode.FOLDER -> if (i + 1 < n) playIndex(i + 1)
            PlayMode.LOOP_FOLDER -> if (n > 0) playIndex((i + 1) % n)
        }
    }

    private fun onLoadFailed(i: Int, file: File, e: Throwable) {
        onMessage?.invoke("⚠️ ${file.name}: ${e.message ?: e.javaClass.simpleName}")
        // 연속 재생 모드에서는 깨진 파일을 건너뛴다 (전부 깨졌으면 무한 반복하지 않도록 목록 크기까지만)
        val n = files.size
        val skip = (mode == PlayMode.FOLDER || mode == PlayMode.LOOP_FOLDER) && failStreak.incrementAndGet() < n
        if (skip) {
            if (mode == PlayMode.FOLDER && i + 1 >= n) return
            playIndex((i + 1) % n)
        }
    }
}
