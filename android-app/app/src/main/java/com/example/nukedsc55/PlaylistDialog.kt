package com.example.nukedsc55

import android.app.Dialog
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.os.Environment
import android.text.InputType
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.BaseAdapter
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import java.io.File

/** PlaylistDialog가 MidiPlayerPanel에게 묻거나 시키는 일들. */
interface PlaylistHost {
    /** 지금 재생 중인 목록이 플레이리스트일 때 현재 곡의 위치, 아니면 -1 (현재 곡 강조용). */
    fun nowPlayingIndex(): Int
    /** 플레이리스트의 index번 곡부터 재생 (재생 목록을 플레이리스트로 전환). */
    fun playFromPlaylist(index: Int)
    /** 플레이리스트가 편집됨 — 재생 중인 목록이 플레이리스트라면 소리를 끊지 않고 동기화한다. */
    fun onPlaylistEdited()
    /** [적용] 버튼: 재생 목록을 플레이리스트로 바꾼다 (재생은 시작하지 않음). 이미 플레이리스트가 재생 목록이면 아무 것도 안 한다. */
    fun applyPlaylist()
    /** "곡 추가" 탐색기를 처음 열 폴더. */
    fun pickerStartDir(): File
    /** 이름 붙여 저장하는 .m3u8 파일들의 폴더. */
    fun saveDir(): File
}

/**
 * PlaylistDialog.kt — 📋 플레이리스트 화면.
 *
 *  - 목록: 곡을 누르면 그 곡부터 재생, ▲▼로 순서 변경, × 로 삭제. 재생 중인 곡은 ▶ 와 초록색으로 강조.
 *  - ➕ 곡 추가: 폴더를 오가며 여러 곡을 체크해서 한꺼번에 담는다 (폴더를 넘나들며 고른 것도 유지됨).
 *  - 💾 저장 / 📂 불러오기: 이름을 붙여 .m3u8 파일로 저장하고 다시 불러온다 (불러오기에서 길게 누르면 삭제).
 *  - 🗑 비우기.
 * 시스템의 라이트/다크 설정과 상관없이 앱의 랙 스타일에 맞춘 어두운 다이얼로그를 직접 만들어 쓴다.
 */
class PlaylistDialog(
    private val activity: AppCompatActivity,
    private val store: PlaylistStore,
    private val host: PlaylistHost
) {
    companion object {
        private val TEXT = 0xFFE0E0FF.toInt()
        private val DIM = 0xFF8888AA.toInt()
        private val BTN_TEXT = 0xFFCCCCDD.toInt()
        private val ACCENT = 0xFF00CC88.toInt()
        private val ACCENT_BG = 0x2200CC88
        private val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
        private val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT
    }

    private class Shell(val dialog: Dialog, val root: LinearLayout, val title: TextView)
    private class RowTag(val title: TextView, val up: TextView, val down: TextView, val del: TextView)

    private var shell: Shell? = null
    private lateinit var listView: ListView

    val isShowing: Boolean get() = shell?.dialog?.isShowing == true

    private fun dp(v: Int): Int = (v * activity.resources.displayMetrics.density).toInt()

    private fun toast(msg: String) = Toast.makeText(activity, msg, Toast.LENGTH_SHORT).show()

    // ── 공통 뼈대 / 버튼 ────────────────────────────────────────────────────

    private fun textButton(label: String, size: Float = 12f, onClick: () -> Unit): TextView =
        TextView(activity).apply {
            text = label
            textSize = size
            setTextColor(BTN_TEXT)
            gravity = Gravity.CENTER
            setPadding(dp(12), 0, dp(12), 0)
            setBackgroundResource(R.drawable.btn_physical_neutral)
            isClickable = true
            setOnClickListener { onClick() }
        }

    /** 제목 줄(+ 닫기 버튼)이 있는 어두운 다이얼로그 뼈대. 내용은 root에 이어서 붙인다. */
    private fun makeShell(closeLabel: String, applyLabel: String? = null, onApply: (() -> Unit)? = null): Shell {
        val d = Dialog(activity)
        d.requestWindowFeature(Window.FEATURE_NO_TITLE)
        val root = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundResource(R.drawable.bg_module)
            setPadding(dp(10), dp(8), dp(10), dp(10))
        }
        val title = TextView(activity).apply {
            setTextColor(TEXT)
            textSize = 15f
            typeface = Typeface.DEFAULT_BOLD
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.MIDDLE
        }
        val header = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        header.addView(title, LinearLayout.LayoutParams(0, WRAP, 1f))
        if (applyLabel != null && onApply != null) {
            header.addView(
                textButton(applyLabel) { onApply() },
                LinearLayout.LayoutParams(WRAP, dp(30)).apply { marginStart = dp(8) }
            )
        }
        header.addView(
            textButton(closeLabel) { d.dismiss() },
            LinearLayout.LayoutParams(WRAP, dp(30)).apply { marginStart = dp(8) }
        )
        root.addView(header, LinearLayout.LayoutParams(MATCH, WRAP).apply { bottomMargin = dp(6) })
        d.setContentView(root)
        d.window?.let { w ->
            w.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            val dm = activity.resources.displayMetrics
            w.setLayout((dm.widthPixels * 0.92f).toInt(), (dm.heightPixels * 0.92f).toInt())
        }
        return Shell(d, root, title)
    }

    // ── 플레이리스트 화면 ───────────────────────────────────────────────────

    private val rowAdapter = object : BaseAdapter() {
        override fun getCount(): Int = store.items.size
        override fun getItem(position: Int): Any = store.items[position]
        override fun getItemId(position: Int): Long = position.toLong()

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val row = (convertView as? LinearLayout) ?: buildRow()
            val tag = row.tag as RowTag
            val items = store.items
            if (position >= items.size) return row
            val f = items[position]
            val current = position == host.nowPlayingIndex()
            val missing = !f.exists()
            tag.title.text = (if (current) "▶ " else if (missing) "⚠️ " else "") +
                "${position + 1}. ${f.nameWithoutExtension}"
            tag.title.setTextColor(if (current) ACCENT else if (missing) DIM else TEXT)
            row.setBackgroundColor(if (current) ACCENT_BG else Color.TRANSPARENT)

            tag.up.isEnabled = position > 0
            tag.up.alpha = if (position > 0) 1f else 0.3f
            tag.down.isEnabled = position < items.size - 1
            tag.down.alpha = if (position < items.size - 1) 1f else 0.3f
            tag.up.setOnClickListener { store.move(position, position - 1); edited() }
            tag.down.setOnClickListener { store.move(position, position + 1); edited() }
            tag.del.setOnClickListener { store.removeAt(position); edited() }
            row.setOnClickListener { host.playFromPlaylist(position); refresh() }
            return row
        }
    }

    private fun buildRow(): LinearLayout {
        val row = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            minimumHeight = dp(40)
            setPadding(dp(8), dp(2), dp(4), dp(2))
            isClickable = true
        }
        val title = TextView(activity).apply {
            textSize = 13f
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
        }
        row.addView(title, LinearLayout.LayoutParams(0, WRAP, 1f))
        fun iconButton(label: String, size: Float): TextView {
            val b = textButton(label, size) {}
            b.setPadding(0, 0, 0, 0)
            row.addView(b, LinearLayout.LayoutParams(dp(38), dp(32)).apply { marginStart = dp(4) })
            return b
        }
        val up = iconButton("▲", 12f)
        val down = iconButton("▼", 12f)
        val del = iconButton("×", 18f)
        row.tag = RowTag(title, up, down, del)
        return row
    }

    fun show() {
        val sh = makeShell("닫기", "✔ 적용") {
            if (store.items.isEmpty()) toast("플레이리스트가 비어 있습니다") else { host.applyPlaylist(); dismiss() }
        }
        shell = sh

        val toolbar = LinearLayout(activity).apply { orientation = LinearLayout.HORIZONTAL }
        fun tool(label: String, action: () -> Unit) {
            toolbar.addView(
                textButton(label, 11f, action),
                LinearLayout.LayoutParams(0, dp(34), 1f).apply { marginEnd = dp(4) }
            )
        }
        tool("➕ 곡 추가") { Picker(host.pickerStartDir()).show() }
        tool("💾 저장") { promptSave() }
        tool("📂 불러오기") { showLoad() }
        tool("🗑 비우기") { confirmClear() }
        sh.root.addView(toolbar, LinearLayout.LayoutParams(MATCH, WRAP).apply { bottomMargin = dp(6) })

        listView = ListView(activity).apply {
            divider = null
            selector = ColorDrawable(Color.TRANSPARENT)
            this.adapter = rowAdapter
        }
        val empty = TextView(activity).apply {
            text = "플레이리스트가 비어 있습니다\n\n[➕ 곡 추가]로 MIDI 파일을 담아보세요"
            setTextColor(DIM)
            textSize = 13f
            gravity = Gravity.CENTER
        }
        val frame = FrameLayout(activity)
        frame.addView(listView, FrameLayout.LayoutParams(MATCH, MATCH))
        frame.addView(empty, FrameLayout.LayoutParams(MATCH, MATCH))
        listView.emptyView = empty
        sh.root.addView(frame, LinearLayout.LayoutParams(MATCH, 0, 1f))

        sh.dialog.setOnDismissListener { if (shell === sh) shell = null }
        sh.dialog.show()
        refresh()
        val now = host.nowPlayingIndex()
        if (now > 1) listView.setSelection(now - 1)
    }

    fun dismiss() {
        shell?.dialog?.dismiss()
        shell = null
    }

    /** 목록/제목을 다시 그린다 (곡이 자동으로 넘어가 현재 곡 표시가 바뀔 때도 부른다). */
    fun refresh() {
        val sh = shell ?: return
        if (!sh.dialog.isShowing) return
        sh.title.text = "📋 플레이리스트 (${store.items.size}곡)"
        rowAdapter.notifyDataSetChanged()
    }

    private fun edited() {
        host.onPlaylistEdited()
        refresh()
    }

    // ── 비우기 / 저장 / 불러오기 ────────────────────────────────────────────

    private fun confirmClear() {
        val n = store.items.size
        if (n == 0) return
        AlertDialog.Builder(activity)
            .setMessage("플레이리스트의 ${n}곡을 모두 비울까요?\n(MIDI 파일은 삭제되지 않습니다)")
            .setPositiveButton("비우기") { _, _ -> store.clear(); edited() }
            .setNegativeButton("취소", null)
            .show()
    }

    private fun promptSave() {
        if (store.items.isEmpty()) { toast("저장할 곡이 없습니다"); return }
        val input = EditText(activity).apply {
            hint = "플레이리스트 이름"
            setSingleLine()
            inputType = InputType.TYPE_CLASS_TEXT
        }
        val holder = FrameLayout(activity).apply {
            setPadding(dp(20), dp(8), dp(20), 0)
            addView(input, FrameLayout.LayoutParams(MATCH, WRAP))
        }
        AlertDialog.Builder(activity)
            .setTitle("플레이리스트 저장")
            .setView(holder)
            .setPositiveButton("저장") { _, _ -> saveAs(input.text.toString()) }
            .setNegativeButton("취소", null)
            .show()
    }

    private fun saveAs(raw: String) {
        val name = PlaylistStore.safeName(raw)
        if (name == null) { toast("이름을 입력하세요"); return }
        val f = File(host.saveDir(), "$name.m3u8")
        if (!f.exists()) { writeTo(f); return }
        AlertDialog.Builder(activity)
            .setMessage("'${f.nameWithoutExtension}' 이(가) 이미 있습니다. 덮어쓸까요?")
            .setPositiveButton("덮어쓰기") { _, _ -> writeTo(f) }
            .setNegativeButton("취소", null)
            .show()
    }

    private fun writeTo(f: File) {
        try {
            PlaylistStore.writeM3u(f, store.items)
            toast("💾 저장됨: ${f.nameWithoutExtension} (${store.items.size}곡)")
        } catch (e: Exception) {
            toast("⚠️ 저장 실패: ${e.message ?: e.javaClass.simpleName}")
        }
    }

    private fun showLoad() {
        val dir = host.saveDir()
        val saved = PlaylistStore.listSaved(dir)
        if (saved.isEmpty()) {
            AlertDialog.Builder(activity)
                .setTitle("저장된 플레이리스트 없음")
                .setMessage("[💾 저장]으로 만든 플레이리스트가 여기에 나타납니다.\n\n${dir.absolutePath}\n\n(.m3u8 파일을 이 폴더에 넣어도 됩니다)")
                .setPositiveButton("확인", null)
                .show()
            return
        }
        val names = saved.map { "📄  ${it.nameWithoutExtension}" }.toTypedArray()
        val dlg = AlertDialog.Builder(activity)
            .setTitle("플레이리스트 불러오기 (길게 누르면 삭제)")
            .setItems(names) { _, which -> confirmLoad(saved[which]) }
            .setNegativeButton("닫기", null)
            .create()
        dlg.show()
        dlg.listView.setOnItemLongClickListener { _, _, pos, _ ->
            confirmDelete(saved[pos], dlg)
            true
        }
    }

    private fun confirmDelete(f: File, listDialog: AlertDialog) {
        AlertDialog.Builder(activity)
            .setMessage("'${f.nameWithoutExtension}' 플레이리스트 파일을 삭제할까요?\n(안의 MIDI 파일은 삭제되지 않습니다)")
            .setPositiveButton("삭제") { _, _ ->
                if (f.delete()) toast("🗑 삭제됨: ${f.nameWithoutExtension}") else toast("⚠️ 삭제 실패")
                listDialog.dismiss()
                showLoad()
            }
            .setNegativeButton("취소", null)
            .show()
    }

    private fun confirmLoad(f: File) {
        val (found, skipped) = try {
            PlaylistStore.readM3u(f)
        } catch (e: Exception) {
            toast("⚠️ 읽기 실패: ${e.message ?: e.javaClass.simpleName}")
            return
        }
        if (found.isEmpty()) { toast("⚠️ 불러올 수 있는 곡이 없습니다 (${skipped}줄 건너뜀)"); return }
        val note = if (skipped > 0) "\n(없는 파일 ${skipped}개는 제외됨)" else ""
        if (store.items.isEmpty()) {
            store.replaceAll(found)
            edited()
            toast("📂 ${found.size}곡 불러옴$note")
            return
        }
        AlertDialog.Builder(activity)
            .setTitle(f.nameWithoutExtension)
            .setMessage("${found.size}곡$note\n\n현재 플레이리스트(${store.items.size}곡)를 어떻게 할까요?")
            .setPositiveButton("바꾸기") { _, _ -> store.replaceAll(found); edited() }
            .setNeutralButton("뒤에 추가") { _, _ -> store.add(found); edited() }
            .setNegativeButton("취소", null)
            .show()
    }

    // ── ➕ 곡 추가 (폴더를 오가며 여러 곡을 체크) ─────────────────────────────

    private class PickRow(val file: File, val kind: Int) // 0 = 상위 폴더, 1 = 폴더, 2 = MIDI 파일

    private inner class Picker(startDir: File) {
        private val root = Environment.getExternalStorageDirectory()
        private val rows = ArrayList<PickRow>()
        private val picked = LinkedHashSet<String>()   // 폴더를 넘나들며 고른 곡 (고른 순서 유지)
        private val already = store.items.mapTo(HashSet()) { it.absolutePath }
        private var dir: File = startDir
        private var midis: List<File> = emptyList()
        private lateinit var sh: Shell
        private lateinit var lv: ListView
        private lateinit var btnAdd: TextView
        private lateinit var btnAll: TextView

        private val ad = object : BaseAdapter() {
            override fun getCount(): Int = rows.size
            override fun getItem(position: Int): Any = rows[position]
            override fun getItemId(position: Int): Long = position.toLong()
            override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
                val tv = (convertView as? TextView) ?: TextView(activity).apply {
                    textSize = 14f
                    maxLines = 1
                    ellipsize = TextUtils.TruncateAt.END
                    setPadding(dp(10), dp(9), dp(10), dp(9))
                }
                val r = rows[position]
                val path = r.file.absolutePath
                when (r.kind) {
                    0 -> {
                        tv.text = "⬆  .."
                        tv.setTextColor(DIM)
                        tv.setBackgroundColor(Color.TRANSPARENT)
                    }
                    1 -> {
                        tv.text = "📁  ${r.file.name}"
                        tv.setTextColor(TEXT)
                        tv.setBackgroundColor(Color.TRANSPARENT)
                    }
                    else -> {
                        val inList = path in already
                        val on = path in picked
                        tv.text = (if (inList) "📋  " else if (on) "✅  " else "🎵  ") + r.file.name
                        tv.setTextColor(if (inList) DIM else if (on) ACCENT else TEXT)
                        tv.setBackgroundColor(if (on) ACCENT_BG else Color.TRANSPARENT)
                    }
                }
                return tv
            }
        }

        fun show() {
            sh = makeShell("취소")
            lv = ListView(activity).apply {
                divider = null
                this.adapter = ad
                setOnItemClickListener { _, _, p, _ -> onRow(rows[p]) }
            }
            sh.root.addView(lv, LinearLayout.LayoutParams(MATCH, 0, 1f))

            btnAll = textButton("이 폴더 전체 선택", 12f) { toggleAll() }
            btnAdd = textButton("추가 (0)", 12f) { commit() }
            val bottom = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.END
            }
            bottom.addView(btnAll, LinearLayout.LayoutParams(WRAP, dp(34)).apply { marginEnd = dp(8) })
            bottom.addView(btnAdd, LinearLayout.LayoutParams(WRAP, dp(34)))
            sh.root.addView(bottom, LinearLayout.LayoutParams(MATCH, WRAP).apply { topMargin = dp(6) })

            load(dir)
            sh.dialog.show()
        }

        private fun load(d: File) {
            dir = d
            rows.clear()
            val parent = d.parentFile
            if (d.absolutePath != root.absolutePath && parent != null) rows.add(PickRow(parent, 0))
            (d.listFiles()?.toList() ?: emptyList())
                .filter { it.isDirectory && !it.name.startsWith(".") }
                .sortedWith(Comparator { a, b -> MidiPlaylist.compareNatural(a.name.lowercase(), b.name.lowercase()) })
                .forEach { rows.add(PickRow(it, 1)) }
            midis = MidiPlaylist.scan(d)
            midis.forEach { rows.add(PickRow(it, 2)) }
            ad.notifyDataSetChanged()
            lv.setSelection(0)
            val shown = d.absolutePath.removePrefix(root.absolutePath).ifEmpty { "/" }
            sh.title.text = "$shown  (${midis.size}곡)"
            updateButtons()
        }

        private fun onRow(r: PickRow) {
            if (r.kind != 2) { load(r.file); return }
            val p = r.file.absolutePath
            if (p in already) { toast("이미 플레이리스트에 있는 곡입니다"); return }
            if (!picked.remove(p)) picked.add(p)
            ad.notifyDataSetChanged()
            updateButtons()
        }

        private fun candidates(): List<String> =
            midis.map { it.absolutePath }.filter { it !in already }

        private fun toggleAll() {
            val cand = candidates()
            if (cand.isEmpty()) return
            if (cand.all { it in picked }) picked.removeAll(cand.toSet()) else picked.addAll(cand)
            ad.notifyDataSetChanged()
            updateButtons()
        }

        private fun updateButtons() {
            btnAdd.text = "추가 (${picked.size})"
            btnAdd.alpha = if (picked.isEmpty()) 0.4f else 1f
            val cand = candidates()
            val allOn = cand.isNotEmpty() && cand.all { it in picked }
            btnAll.text = if (allOn) "이 폴더 선택 해제" else "이 폴더 전체 선택"
            btnAll.alpha = if (cand.isEmpty()) 0.4f else 1f
        }

        private fun commit() {
            if (picked.isEmpty()) return
            val added = store.add(picked.map { File(it) })
            sh.dialog.dismiss()
            toast("➕ ${added}곡 추가됨")
            edited()
        }
    }
}
