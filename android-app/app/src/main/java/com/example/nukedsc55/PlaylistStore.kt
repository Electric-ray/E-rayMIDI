package com.example.nukedsc55

import android.content.SharedPreferences
import org.json.JSONArray
import java.io.File

/**
 * PlaylistStore.kt — 사용자가 직접 만드는 플레이리스트 (폴더와 무관하게 여러 폴더의 곡을 원하는 순서로).
 *
 * - 현재 플레이리스트는 편집할 때마다 SharedPreferences에 자동 저장되어 앱을 다시 켜도 남는다.
 * - 이름을 붙여 저장/불러오기는 표준 M3U8 파일(Downloads/midi/playlists 폴더의 .m3u8 파일)로 한다 — PC에서 열어 고칠 수도 있다.
 * - 같은 곡은 한 번만 들어간다 (MidiPlaylist.updateQueue가 경로로 현재 곡을 찾기 때문).
 * 모든 메서드는 UI 스레드에서 부른다.
 */
class PlaylistStore(private val prefs: SharedPreferences) {

    companion object {
        private const val KEY_ITEMS = "midi_playlist_items"
        private val M3U_EXT = setOf("m3u8", "m3u")

        /** M3U/M3U8 파일을 읽는다. (찾은 곡들, 없어졌거나 MIDI가 아니라서 건너뛴 줄 수). 상대 경로는 m3u 파일 위치 기준. */
        fun readM3u(f: File): Pair<List<File>, Int> {
            val base = f.parentFile
            val out = ArrayList<File>()
            var skipped = 0
            for (raw in f.readLines(Charsets.UTF_8)) {
                val line = raw.trim().trimStart('\uFEFF')
                if (line.isEmpty() || line.startsWith("#")) continue
                val p = File(line)
                val file = if (p.isAbsolute) p else File(base, line)
                if (file.isFile && file.extension.lowercase() in MidiPlaylist.EXTENSIONS) out.add(file) else skipped++
            }
            return out to skipped
        }

        fun writeM3u(f: File, items: List<File>) {
            f.parentFile?.mkdirs()
            val text = buildString {
                append("#EXTM3U\n")
                for (song in items) append(song.absolutePath).append('\n')
            }
            f.writeText(text, Charsets.UTF_8)
        }

        /** 저장된 플레이리스트 파일 목록 (이름순). */
        fun listSaved(dir: File): List<File> =
            dir.listFiles { x -> x.isFile && x.extension.lowercase() in M3U_EXT }
                ?.sortedWith(Comparator { a, b -> MidiPlaylist.compareNatural(a.name.lowercase(), b.name.lowercase()) })
                ?: emptyList()

        /** 파일 이름으로 쓸 수 없는 글자를 걷어낸다. 비면 null. */
        fun safeName(raw: String): String? =
            raw.trim().replace(Regex("[\\\\/:*?\"<>|\\p{Cntrl}]"), "_").trim('.', ' ').ifEmpty { null }
    }

    /** 현재 플레이리스트 (읽기 전용 스냅샷 — 편집은 아래 메서드로만). */
    @Volatile var items: List<File> = load()
        private set

    private fun load(): List<File> = runCatching {
        // 존재 여부로 걸러내지 않는다: 저장소 권한을 허용하기 전이나 SD카드가 빠진 상태에서 앱이 시작되면
        // File.isFile()이 전부 false라서, 걸러낸 목록이 다음 저장 때 덮어써져 플레이리스트가 날아간다.
        // (없어진 파일은 목록에 그대로 두고 UI에서 ⚠️로 표시한다.)
        val arr = JSONArray(prefs.getString(KEY_ITEMS, "[]"))
        (0 until arr.length()).map { File(arr.getString(it)) }
    }.getOrDefault(emptyList())

    private fun persist() {
        prefs.edit().putString(KEY_ITEMS, JSONArray(items.map { it.absolutePath }).toString()).apply()
    }

    /** 뒤에 추가한다. 이미 들어 있는 곡은 건너뛰고, 실제로 추가된 곡 수를 돌려준다. */
    fun add(files: List<File>): Int {
        val have = items.mapTo(HashSet()) { it.absolutePath }
        val fresh = files.filter { have.add(it.absolutePath) }
        if (fresh.isNotEmpty()) {
            items = items + fresh
            persist()
        }
        return fresh.size
    }

    fun removeAt(i: Int) {
        if (i !in items.indices) return
        items = items.toMutableList().also { it.removeAt(i) }
        persist()
    }

    fun move(from: Int, to: Int) {
        if (from !in items.indices || to !in items.indices || from == to) return
        val l = items.toMutableList()
        l.add(to, l.removeAt(from))
        items = l
        persist()
    }

    fun clear() {
        items = emptyList()
        persist()
    }

    fun replaceAll(list: List<File>) {
        items = list.distinctBy { it.absolutePath }
        persist()
    }
}
