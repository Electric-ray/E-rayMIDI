package com.example.nukedsc55

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import java.io.File

/**
 * MidiPlayerPanel.kt — LINK를 "MIDI 파일"로 선택했을 때 화면에 나타나는 플레이어 UI(activity_main.xml의 llPlayerBar).
 *
 * MidiFilePlayer(한 곡 재생) + MidiPlaylist(목록/이전/다음/모드)를 만들어서 버튼과 슬라이더에 연결하고,
 * 폴더 탐색 다이얼로그(📂)를 제공한다. MainActivity는 start()/stop()/stopSong()/onResume()/onPause()만 부르면 된다.
 *
 * 기본 MIDI 폴더: Downloads/midi (ROM/사운드폰트 폴더와 같은 방식). 마지막으로 연 폴더는 기억한다.
 * 파일 접근은 java.io.File 직접 접근 — 이미 MANAGE_EXTERNAL_STORAGE 권한으로 ROM/SF2를 읽는 것과 같은 경로라 SAF 불필요.
 *
 * ── 블루투스(차량 AVRCP)/유선 이어폰 리모컨 지원 ─────────────────────────
 * android.media.session.MediaSession 하나를 만들어 재생/정지 상태와 "재생/일시정지/정지/이전곡/다음곡"
 * 액션을 올려두면, 블루투스로 연결된 차량 헤드유닛의 스티어링휠 버튼이나 유선 이어폰의 리모컨 버튼을
 * 눌렀을 때 안드로이드 시스템이 그 명령을 자동으로 이 세션의 Callback으로 보내준다(앱이 직접
 * 블루투스나 헤드셋 버튼 신호를 파싱할 필요가 없다 — OS가 표준 미디어 키 이벤트로 변환해서 꽂아준다).
 * 추가로 AudioFocus를 요청해두면, 다른 앱이 오디오를 가져갈 때(전화 수신 등) 자동으로 일시정지된다.
 */
class MidiPlayerPanel(
    private val activity: AppCompatActivity,
    private val prefs: SharedPreferences,
    private val status: (String) -> Unit,
    /** 엔진이 MIDI를 받을 준비가 됐는지 (SC-55는 초기화 워밍업이 끝나야 함). */
    private val canPlay: () -> Boolean
) {
    companion object {
        private const val KEY_DIR = "midi_dir"
        private const val KEY_LAST = "midi_last_file"   // 마지막으로 재생한 곡(절대 경로) — 엔진 전환/앱 재시작 후 이어서
        private const val KEY_MODE = "midi_mode"
        private const val KEY_VOL = "midi_volume"
        private const val KEY_SRC = "midi_src"          // 마지막 재생 목록의 출처: 폴더 / 사용자 플레이리스트
        private const val SRC_FOLDER = "folder"
        private const val SRC_PLAYLIST = "playlist"
        private const val TICK_MS = 250L
        private const val MEDIA_SESSION_TAG = "E-rayMIDI"

        private const val SESSION_ACTIONS =
            PlaybackState.ACTION_PLAY or
            PlaybackState.ACTION_PAUSE or
            PlaybackState.ACTION_PLAY_PAUSE or
            PlaybackState.ACTION_STOP or
            PlaybackState.ACTION_SKIP_TO_NEXT or
            PlaybackState.ACTION_SKIP_TO_PREVIOUS
    }

    private val bar: View = activity.findViewById(R.id.llPlayerBar)
    private val tvTitle: TextView = activity.findViewById(R.id.tvTrackTitle)
    private val tvTime: TextView = activity.findViewById(R.id.tvTrackTime)
    private val sbProgress: SeekBar = activity.findViewById(R.id.sbProgress)
    private val btnPrev: Button = activity.findViewById(R.id.btnTrackPrev)
    private val btnPlay: Button = activity.findViewById(R.id.btnTrackPlay)
    private val btnStop: Button = activity.findViewById(R.id.btnTrackStop)
    private val btnNext: Button = activity.findViewById(R.id.btnTrackNext)
    private val btnMode: Button = activity.findViewById(R.id.btnPlayMode)
    private val btnBrowse: Button = activity.findViewById(R.id.btnBrowse)
    private val sbVolume: SeekBar = activity.findViewById(R.id.sbVolume)
    private val btnPlaylist: Button = activity.findViewById(R.id.btnPlaylist)

    private var player: MidiFilePlayer? = null
    private var playlist: MidiPlaylist? = null
    private var boundEngine: IEngine? = null
    private var currentDir: File? = null
    private var seeking = false
    private var currentTrackTitle: String = "곡 없음"

    // ── 플레이리스트 ───────────────────────────────────────────────────────────────────────
    // 재생 목록(MidiPlaylist)은 폴더(탐색기에서 고른 곳) 또는 사용자 플레이리스트(store) 중 하나에서 만들어진다.
    // 플레이리스트는 폴더를 옮겨 다녀도 별도로 남아 있다 (폴더를 고르면 재생 목록만 교체되고 store는 그대로).
    private val store = PlaylistStore(prefs)
    @Volatile private var playingFromPlaylist = false
    private var plDialog: PlaylistDialog? = null

    private val plHost = object : PlaylistHost {
        override fun nowPlayingIndex(): Int = if (playingFromPlaylist) (playlist?.index ?: -1) else -1
        override fun playFromPlaylist(index: Int) { this@MidiPlayerPanel.playFromPlaylist(index) }
        override fun onPlaylistEdited() { syncPlaylistEdit() }
        override fun applyPlaylist() {
            if (playlist == null) return
            if (playingFromPlaylist) { status("📋 플레이리스트가 이미 적용되어 있습니다"); return }
            loadPlaylist(null) // 재생 목록을 플레이리스트로 교체 (1번 곡 준비, 재생은 ▶ 로)
        }
        override fun pickerStartDir(): File = currentDir ?: defaultDir()
        override fun saveDir(): File = File(defaultDir(), "playlists")
    }

    private var mediaSession: MediaSession? = null
    private var audioManager: AudioManager? = null
    private var focusRequest: AudioFocusRequest? = null
    // 통화 같은 "잠깐 뺏김"(LOSS_TRANSIENT)으로 우리가 일시정지했는지 — 포커스가 돌아오면 이어서 재생한다
    private var resumeOnFocusGain = false
    private var hasFocus = false

    private val ui = Handler(Looper.getMainLooper())
    private var tickerOn = false
    private val ticker = object : Runnable {
        override fun run() {
            refresh()
            if (tickerOn) ui.postDelayed(this, TICK_MS)
        }
    }

    val isActive: Boolean get() = player != null

    init {
        btnPlay.setOnClickListener { if (guard()) { playlist?.playPause(); refresh() } }
        btnStop.setOnClickListener { playlist?.stop(); refresh() }
        btnNext.setOnClickListener { if (guard()) { playlist?.next(); refresh() } }
        btnPrev.setOnClickListener { if (guard()) { playlist?.prev(); refresh() } }
        btnBrowse.setOnClickListener { showBrowser(currentDir ?: defaultDir()) }
        btnPlaylist.setOnClickListener { showPlaylist() }
        btnMode.setOnClickListener {
            val pl = playlist ?: return@setOnClickListener
            pl.mode = pl.mode.next()
            btnMode.text = pl.mode.label
            prefs.edit().putInt(KEY_MODE, pl.mode.ordinal).apply()
        }

        sbProgress.max = 1000
        sbProgress.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                if (!fromUser) return
                val dur = player?.durationMs ?: 0L
                tvTime.text = "${fmt(dur * progress / 1000L)} / ${fmt(dur)}"
            }
            override fun onStartTrackingTouch(sb: SeekBar?) { seeking = true }
            override fun onStopTrackingTouch(sb: SeekBar?) {
                seeking = false
                val p = player ?: return
                if (p.hasSequence) p.seekTo(p.durationMs * (sb?.progress ?: 0) / 1000L)
                refresh()
            }
        })

        sbVolume.max = 100
        sbVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) player?.setVolume(progress)
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {
                prefs.edit().putInt(KEY_VOL, sb?.progress ?: 100).apply()
            }
        })
    }

    // ── MainActivity가 부르는 진입점 ─────────────────────────────────────

    /** "MIDI 파일" 모드로 연결 시작 (엔진은 이미 초기화되어 EngineRegistry.active로 등록된 상태). */
    fun start(engine: IEngine) {
        if (player != null) stop()
        boundEngine = engine
        playingFromPlaylist = false
        // 파일 재생은 유실이 없는 입력원이므로 RTP용 노트/서스테인 워치독을 꺼야 8초 넘는 긴 음이 끊기지 않는다.
        engine.bypassWatchdogs = true

        val p = MidiFilePlayer({ EngineRegistry.active }) {
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO)
        }
        p.setVolume(prefs.getInt(KEY_VOL, 100))
        val pl = MidiPlaylist(p)
        pl.mode = PlayMode.values().getOrElse(prefs.getInt(KEY_MODE, PlayMode.FOLDER.ordinal)) { PlayMode.FOLDER }
        pl.onTrackChanged = { i, f ->
            prefs.edit().putString(KEY_LAST, f.absolutePath).apply()
            val title = titleOf(pl, i, f)
            activity.runOnUiThread {
                tvTitle.text = title
                currentTrackTitle = title
                updateMediaMetadata()
                plDialog?.refresh() // 플레이리스트 화면이 열려 있으면 현재 곡 강조를 갱신
            }
        }
        pl.onMessage = { msg -> status(msg) }
        // 재생 상태가 바뀔 때마다 MediaSession을 갱신한다. 화면이 꺼져 ticker가 멈춘 상태에서도
        // (자연 종료/다음 곡 자동 재생/비동기 로딩 완료) 세션이 "재생 중"으로 정확해야 블루투스의
        // 재생/일시정지 키가 올바르게 해석된다.
        p.onStateChanged = { activity.runOnUiThread { refresh() } }
        player = p
        playlist = pl

        sbVolume.progress = p.volume
        btnMode.text = pl.mode.label
        sbProgress.progress = 0
        tvTime.text = "00:00 / 00:00"
        currentTrackTitle = "곡 없음"

        requestAudioFocus()
        createMediaSession()

        // 마지막으로 들은 곡부터 시작한다 (엔진을 바꿔도, 앱을 다시 시작해도). 그 곡이 지금 폴더에 없으면 1번 곡.
        val last = prefs.getString(KEY_LAST, null)?.let { File(it) }?.takeIf { it.isFile }
        val dir = File(prefs.getString(KEY_DIR, null) ?: last?.parentFile?.absolutePath ?: defaultDir().absolutePath)
        // 마지막으로 들은 목록이 사용자 플레이리스트였고 아직 곡이 남아 있으면 그 목록으로 이어서, 아니면 폴더로
        currentDir = dir
        if (prefs.getString(KEY_SRC, SRC_FOLDER) == SRC_PLAYLIST && store.items.isNotEmpty()) loadPlaylist(last)
        else loadFolder(dir, last, false)
        bar.visibility = View.VISIBLE
        startTicker()
    }

    /** 연결 종료 (엔진이 살아 있는 상태에서 불러야 소리 정리가 엔진에 전달된다). */
    fun stop() {
        plDialog?.dismiss()
        plDialog = null
        stopTicker()
        player?.onStateChanged = null
        releaseMediaSession()
        abandonAudioFocus()
        playlist?.shutdown()
        playlist = null
        player = null
        boundEngine?.bypassWatchdogs = false
        boundEngine = null
        bar.visibility = View.GONE
    }

    /** 곡만 정지 (연결은 유지) — "초기화" 버튼용. */
    fun stopSong() {
        playlist?.stop()
        refresh()
    }

    fun onResume() { if (player != null) startTicker() }
    fun onPause() { stopTicker() }

    // ── 블루투스/유선 리모컨 (MediaSession + AudioFocus) ───────────────────

    /**
     * 차량(AVRCP)이나 다른 앱이 오디오를 요청하면(전화 수신 등) 자동으로 일시정지한다.
     * 획득 실패해도(다른 앱이 이미 독점 중 등) 치명적이지 않으므로 재생 자체는 계속 진행한다 —
     * 미디어 버튼 라우팅은 AudioFocus 없이도 MediaSession이 활성 상태면 대부분 동작한다.
     */
    private fun requestAudioFocus() {
        val am = activity.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        audioManager = am
        val attrs = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build()
        val req = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attrs)
            .setOnAudioFocusChangeListener { change ->
                when (change) {
                    // 영구 상실(다른 음악 앱 등): 멈추고 자동 재개하지 않는다
                    AudioManager.AUDIOFOCUS_LOSS -> {
                        hasFocus = false
                        resumeOnFocusGain = false
                        playlist?.pause()
                        activity.runOnUiThread { refresh() }
                    }
                    // 잠깐 상실(전화 수신 등): 재생 중이었다면 기억해 두고 포커스가 돌아오면 이어서 재생
                    AudioManager.AUDIOFOCUS_LOSS_TRANSIENT -> {
                        hasFocus = false
                        if (player?.state == MidiFilePlayer.State.PLAYING) resumeOnFocusGain = true
                        playlist?.pause()
                        activity.runOnUiThread { refresh() }
                    }
                    AudioManager.AUDIOFOCUS_GAIN -> {
                        hasFocus = true
                        // 통화 뒤에는 출력 경로가 바뀌어(SCO -> A2DP) AAudio 스트림이 끊겨 있을 수 있다 — 다시 열어 둔다
                        boundEngine?.restartAudio()
                        if (resumeOnFocusGain) {
                            resumeOnFocusGain = false
                            // 경로 전환이 끝날 시간을 잠깐 준 뒤 재생
                            ui.postDelayed({ if (guard()) { playlist?.play(); refresh() } }, 400L)
                        }
                    }
                    else -> {}
                }
            }
            .build()
        focusRequest = req
        hasFocus = am.requestAudioFocus(req) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    }

    /** 다른 앱에 포커스를 뺏긴 뒤 사용자가 재생을 누르면 다시 요청한다 (안 하면 이후 포커스 이벤트도 못 받는다). */
    private fun ensureAudioFocus() {
        val am = audioManager ?: return
        val req = focusRequest ?: return
        if (!hasFocus) hasFocus = am.requestAudioFocus(req) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED
    }

    private fun abandonAudioFocus() {
        focusRequest?.let { audioManager?.abandonAudioFocusRequest(it) }
        focusRequest = null
        audioManager = null
        hasFocus = false
        resumeOnFocusGain = false
    }

    private fun createMediaSession() {
        val session = MediaSession(activity, MEDIA_SESSION_TAG)
        session.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS or MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS)
        session.setCallback(object : MediaSession.Callback() {
            // 블루투스 차량(AVRCP)의 재생/정지/이전곡/다음곡 버튼, 유선 이어폰 리모컨의
            // 싱글/더블/트리플 클릭(재생·일시정지/다음곡/이전곡)이 모두 여기로 들어온다.
            override fun onPlay() { if (guard()) { playlist?.play(); afterRemoteCommand() } }
            override fun onPause() { playlist?.pause(); afterRemoteCommand() }
            override fun onStop() { playlist?.stop(); afterRemoteCommand() }

            // 재생/일시정지 토글 키(블루투스/이어폰 가운데 버튼)는 세션에 기록된 상태가 아니라 "실제 플레이어 상태"로
            // 판단한다. 기본 구현은 PlaybackState를 보고 onPlay/onPause를 고르는데, 세션 상태가 한 박자라도
            // 어긋나 있으면(재생 중인데 STOPPED로 남아 있는 경우 등) onPlay가 "이미 재생 중"으로 무시돼 키가 먹통이 된다.
            @Suppress("DEPRECATION")
            override fun onMediaButtonEvent(mediaButtonIntent: Intent): Boolean {
                val ev = mediaButtonIntent.getParcelableExtra<KeyEvent>(Intent.EXTRA_KEY_EVENT)
                if (ev != null && ev.keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE) {
                    if (ev.action == KeyEvent.ACTION_DOWN && ev.repeatCount == 0) {
                        if (player?.state == MidiFilePlayer.State.PLAYING) playlist?.pause()
                        else if (guard()) playlist?.play()
                        afterRemoteCommand()
                    }
                    return true
                }
                return super.onMediaButtonEvent(mediaButtonIntent)
            }
            override fun onSkipToNext() { if (guard()) { playlist?.next(); afterRemoteCommand() } }
            override fun onSkipToPrevious() { if (guard()) { playlist?.prev(); afterRemoteCommand() } }
        })
        session.isActive = true
        mediaSession = session
        updateMediaMetadata()
        updateMediaSessionState()
    }

    private fun releaseMediaSession() {
        mediaSession?.isActive = false
        mediaSession?.release()
        mediaSession = null
    }

    /** 원격(블루투스/유선) 명령 처리 직후: UI와 세션 상태를 즉시 갱신해 화면/차량 디스플레이가 바로 반영되게 한다. */
    private fun afterRemoteCommand() {
        activity.runOnUiThread { refresh() }
    }

    private fun updateMediaMetadata() {
        mediaSession?.setMetadata(
            MediaMetadata.Builder()
                .putString(MediaMetadata.METADATA_KEY_TITLE, currentTrackTitle)
                .putString(MediaMetadata.METADATA_KEY_ARTIST, "E-rayMIDI")
                .putLong(MediaMetadata.METADATA_KEY_DURATION, player?.durationMs ?: 0L)
                .build()
        )
    }

    private fun updateMediaSessionState() {
        val session = mediaSession ?: return
        val p = player
        val state = when (p?.state) {
            MidiFilePlayer.State.PLAYING -> PlaybackState.STATE_PLAYING
            MidiFilePlayer.State.PAUSED -> PlaybackState.STATE_PAUSED
            else -> PlaybackState.STATE_STOPPED
        }
        session.setPlaybackState(
            PlaybackState.Builder()
                .setActions(SESSION_ACTIONS)
                .setState(state, p?.positionMs ?: 0L, 1.0f)
                .build()
        )
    }

    // ── 내부 ─────────────────────────────────────────────────────────────

    private fun guard(): Boolean {
        val pl = playlist ?: return false
        if (!canPlay()) { status("⏳ SC-55 초기화 중… 잠시 후 다시 눌러주세요"); return false }
        if (pl.files.isEmpty()) { status("⚠️ 재생할 MIDI 파일이 없습니다 — 📂로 폴더를 선택하거나 📋에 곡을 담으세요"); return false }
        ensureAudioFocus()
        return true
    }

    private fun defaultDir(): File =
        File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "midi")

    private fun titleOf(pl: MidiPlaylist, i: Int, f: File): String =
        "${if (playingFromPlaylist) "📋 " else ""}${i + 1}/${pl.files.size}  ${f.nameWithoutExtension}"

    private fun fmt(ms: Long): String {
        val s = (ms / 1000L).coerceAtLeast(0L)
        return "%02d:%02d".format(s / 60L, s % 60L)
    }

    private fun startTicker() {
        if (tickerOn) return
        tickerOn = true
        ui.post(ticker)
    }

    private fun stopTicker() {
        tickerOn = false
        ui.removeCallbacks(ticker)
    }

    private fun refresh() {
        val p = player ?: return
        btnPlay.text = if (p.state == MidiFilePlayer.State.PLAYING) "⏸" else "▶"
        val dur = p.durationMs
        val pos = p.positionMs
        if (!seeking) {
            tvTime.text = "${fmt(pos)} / ${fmt(dur)}"
            sbProgress.progress = if (dur > 0L) (pos * 1000L / dur).toInt() else 0
        }
        updateMediaSessionState()
    }

    // ── 플레이리스트 연동 ───────────────────────────────────────────────────────────────────────

    private fun showPlaylist() {
        if (playlist == null) return
        plDialog?.dismiss()
        plDialog = PlaylistDialog(activity, store, plHost).also { it.show() }
    }

    /** 저장된 플레이리스트를 재생 목록으로 삼는다 (재생은 시작하지 않음). store가 비어 있지 않을 때만 부른다. */
    private fun loadPlaylist(startFile: File?) {
        val pl = playlist ?: return
        val items = store.items
        if (items.isEmpty()) return
        playingFromPlaylist = true
        prefs.edit().putString(KEY_SRC, SRC_PLAYLIST).apply()
        val idx = if (startFile != null) items.indexOfFirst { it.absolutePath == startFile.absolutePath }.coerceAtLeast(0) else 0
        pl.setQueue(items, idx)
        tvTitle.text = titleOf(pl, idx, items[idx])
        status("📋 플레이리스트 ${items.size}곡 준비됨  (▶ 로 재생)")
    }

    /** 플레이리스트 화면에서 곡을 눌렀을 때: 재생 목록을 플레이리스트로 바꾸고 그 곡부터 재생. */
    private fun playFromPlaylist(i: Int) {
        val pl = playlist ?: return
        val items = store.items
        if (i !in items.indices) return
        if (!canPlay()) { status("⏳ SC-55 초기화 중… 잠시 후 다시 눌러주세요"); return }
        ensureAudioFocus()
        if (!playingFromPlaylist) {
            playingFromPlaylist = true
            prefs.edit().putString(KEY_SRC, SRC_PLAYLIST).apply()
            pl.setQueue(items, i)
        }
        pl.playIndex(i)
        refresh()
    }

    /**
     * 플레이리스트가 편집됐을 때: 지금 재생 목록이 플레이리스트인 경우에만 소리를 끊지 않고 맞춘다.
     * (폴더를 재생 중이면 플레이리스트를 고쳐도 재생에는 영향이 없다.)
     */
    private fun syncPlaylistEdit() {
        val pl = playlist ?: return
        if (!playingFromPlaylist) return
        pl.updateQueue(store.items)
        val f = pl.files.getOrNull(pl.index)
        val title = if (f != null) titleOf(pl, pl.index, f) else "플레이리스트 비어 있음"
        tvTitle.text = title
        currentTrackTitle = title
        updateMediaMetadata()
        refresh()
    }

    /** 폴더를 재생 목록으로 삼는다. startFile이 있으면 그 곡부터(목록 안의 위치), play=true면 바로 재생. */
    private fun loadFolder(dir: File, startFile: File?, play: Boolean) {
        val pl = playlist ?: return
        playingFromPlaylist = false
        prefs.edit().putString(KEY_SRC, SRC_FOLDER).apply()
        if (!dir.exists()) dir.mkdirs()
        currentDir = dir
        prefs.edit().putString(KEY_DIR, dir.absolutePath).apply()
        val list = MidiPlaylist.scan(dir)
        if (list.isEmpty()) {
            pl.setQueue(emptyList(), 0)
            tvTitle.text = "MIDI 파일 없음 — ${dir.absolutePath}"
            status("⚠️ .mid 파일이 없습니다: ${dir.absolutePath}  (📂로 다른 폴더 선택)")
            return
        }
        val idx = if (startFile != null) list.indexOfFirst { it.absolutePath == startFile.absolutePath }.coerceAtLeast(0) else 0
        pl.setQueue(list, idx)
        tvTitle.text = titleOf(pl, idx, list[idx])
        if (play) { if (guard()) pl.playIndex(idx) }
        else status("🎵 ${list.size}곡 준비됨 — ${dir.name}  (▶ 로 재생)")
    }

    /** 📂 폴더/파일 탐색 다이얼로그. 파일을 누르면 그 폴더가 재생 목록이 되어 그 곡부터 재생된다. */
    private fun showBrowser(dir: File) {
        val root = Environment.getExternalStorageDirectory()
        val kids = dir.listFiles()?.toList() ?: emptyList()
        val subDirs = kids.filter { it.isDirectory && !it.name.startsWith(".") }
            .sortedWith(Comparator { a, b -> MidiPlaylist.compareNatural(a.name.lowercase(), b.name.lowercase()) })
        val midis = MidiPlaylist.scan(dir)

        val entries = ArrayList<File>()
        val labels = ArrayList<String>()
        val parent = dir.parentFile
        if (dir.absolutePath != root.absolutePath && parent != null) {
            entries.add(parent); labels.add("⬆  ..")
        }
        for (d in subDirs) { entries.add(d); labels.add("📁  ${d.name}") }
        for (f in midis) { entries.add(f); labels.add("🎵  ${f.name}") }

        val shown = dir.absolutePath.removePrefix(root.absolutePath).ifEmpty { "/" }
        val b = AlertDialog.Builder(activity)
            .setTitle("$shown  (${midis.size}곡)")
            .setNegativeButton("닫기", null)
        if (midis.isNotEmpty()) {
            b.setPositiveButton("▶ 이 폴더 전체 재생") { _, _ ->
                val pl = playlist
                if (pl != null && (pl.mode == PlayMode.SINGLE || pl.mode == PlayMode.LOOP_ONE)) {
                    pl.mode = PlayMode.FOLDER
                    btnMode.text = pl.mode.label
                    prefs.edit().putInt(KEY_MODE, pl.mode.ordinal).apply()
                }
                loadFolder(dir, null, true)
            }
        }
        if (labels.isEmpty()) {
            b.setMessage("이 폴더에는 하위 폴더나 MIDI 파일이 없습니다.")
        } else {
            b.setItems(labels.toTypedArray()) { _, which ->
                val picked = entries[which]
                if (picked.isDirectory) showBrowser(picked) else loadFolder(dir, picked, true)
            }
        }
        b.show()
    }
}
