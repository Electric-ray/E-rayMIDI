package com.example.nukedsc55

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.provider.Settings
import android.util.Log
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.io.File

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "SC55-Main"
        private const val REQ_LEGACY_STORAGE = 1001
        private const val REQ_MANAGE_STORAGE = 1002
        // 30fps(33ms)에서 20fps(50ms)로 완화: LCD_Render()가 lcd.mutex.try_lock()을
// 쓰고, 실패하면 조용히 프레임을 드롭한다(lcd.cpp 확인됨). 렌더 스레드와
// 폴링 스레드 사이의 위상이 자주 어긋나면서 프레임이 반복적으로 스킵되어
// 화면이 깜빡이는 것으로 보이는 현상 완화를 위해 폴링 간격을 넉넉하게.
private const val LCD_FPS_INTERVAL_MS = 50L // ~20fps
        // munt-android 원본 GUI의 LED 색상 그대로 재사용
        private const val LED_OFF     = 0xFF333355.toInt()
        private const val LED_ON      = 0xFF00EE44.toInt()
        private const val LED_RHY_OFF = 0xFF332222.toInt()
        private const val LED_RHY_ON  = 0xFFEE2222.toInt()
    }

    private lateinit var rgConnection: RadioGroup
    private lateinit var rgEngine:     RadioGroup
    private lateinit var rbEngineSoundfont: RadioButton
    private lateinit var rbEngineMunt: RadioButton
    private lateinit var layoutSoundFontPicker: LinearLayout
    private lateinit var tvSoundFontName: TextView
    private lateinit var btnPickSoundFont: Button
    private lateinit var btnConnectToggle: Button
    private lateinit var btnResetEngine:  Button
    private lateinit var btnRomHelp:  Button
    private lateinit var tvStatus:    TextView
    private lateinit var tvRomStatus: TextView
    private lateinit var romStatusRow: LinearLayout
    private lateinit var lcdFrame:    android.view.View
    private lateinit var ivLcd:       LcdView
    private lateinit var tvInstrumentPanel: TextView
    private lateinit var llMuntPanel: LinearLayout
    private val muntLeds = arrayOfNulls<android.view.View>(9)
    private val muntPatchNames = arrayOfNulls<TextView>(9)

    private lateinit var sc55Engine: SC55Engine
    private lateinit var sfEngine:   SoundFontEngine
    private lateinit var muntEngine: MuntEngine

    // ── USB MIDI 주변장치(peripheral) 실제 연결 ──────────────────────
    // (munt-android 참고: UsbMidiDeviceService를 매니페스트에 등록해놓는 것만으로는
    //  실제 물리 USB 케이블로 연결된 PC의 MIDI가 안 들어온다 — 그건 다른 앱이 우리
    //  앱으로 MIDI를 보낼 때 쓰는 가상장치 경로임. 실제 물리 USB 주변장치 포트는 안드로이드가
    //  시스템적으로 만들어주는 별도의 MidiDevice로, MidiManager.getDevices()로 찾아서
    //  직접 열고 그 출력포트(=PC에서 보낸 데이터)에 리시버를 붙여야만 실제로 데이터가 온다.
    private var usbMidiDeviceCallback: android.media.midi.MidiManager.DeviceCallback? = null
    private var usbMidiOpenDevice: android.media.midi.MidiDevice? = null
    private var usbMidiOutputPort: android.media.midi.MidiOutputPort? = null
    private val usbMidiThread by lazy {
        HandlerThread("UsbMidiPeripheralThread", android.os.Process.THREAD_PRIORITY_URGENT_AUDIO).apply { start() }
    }
    private val usbMidiParser = MidiStreamParser { bytes -> EngineRegistry.active?.dispatchMidi(bytes) }

    private fun startUsbMidiPeripheral() {
        val midiManager = getSystemService(Context.MIDI_SERVICE) as android.media.midi.MidiManager
        val midiHandler = Handler(usbMidiThread.looper)

        fun tryOpen(info: android.media.midi.MidiDeviceInfo) {
            midiManager.openDevice(info, { device ->
                if (device == null) return@openDevice
                usbMidiOpenDevice = device
                usbMidiOutputPort = device.openOutputPort(0)
                usbMidiOutputPort?.connect(object : android.media.midi.MidiReceiver() {
                    override fun onSend(data: ByteArray, offset: Int, count: Int, timestamp: Long) {
                        usbMidiParser.feed(if (offset == 0 && count == data.size) data else data.copyOfRange(offset, offset + count))
                    }
                })
                status("✅ USB MIDI 장치 연결됨")
            }, midiHandler)
        }

        // 연결 시점에 이미 보이는 장치 다 시도 (케이블이 이미 꽂혀 있는 경우)
        midiManager.devices.forEach { tryOpen(it) }

        // 케이블을 연결하는 시점이 앱 실행 이후일 수도 있으므로, 새 장치가 나타나는 것도 감지
        usbMidiDeviceCallback = object : android.media.midi.MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: android.media.midi.MidiDeviceInfo) { tryOpen(info) }
        }
        midiManager.registerDeviceCallback(usbMidiDeviceCallback!!, midiHandler)
    }

    private fun stopUsbMidiPeripheral() {
        usbMidiDeviceCallback?.let {
            val midiManager = getSystemService(Context.MIDI_SERVICE) as android.media.midi.MidiManager
            runCatching { midiManager.unregisterDeviceCallback(it) }
        }
        usbMidiDeviceCallback = null
        runCatching { usbMidiOutputPort?.close() }
        runCatching { usbMidiOpenDevice?.close() }
        usbMidiOutputPort = null
        usbMidiOpenDevice = null
    }

    // 현재 연결을 시작한 엔진 (셋 중 하나만 동시에 돌릴 수 있음)
    private enum class EngineType { SC55, SOUNDFONT, MUNT }
    private var activeEngineType: EngineType? = null

    // 기기초기화 버튼이 지금 어느 엔진에 resetEngine()을 호출해야 하는지 공통적으로 찾기 위함
    private fun currentEngine(): IEngine? = when (activeEngineType) {
        EngineType.SC55 -> sc55Engine
        EngineType.SOUNDFONT -> sfEngine
        EngineType.MUNT -> muntEngine
        null -> null
    }

    private val prefs by lazy { getSharedPreferences("nukedsc55_prefs", MODE_PRIVATE) }
    private var selectedSoundFontPath: String? = null

    // ── LCD 렌더링 상태 ──────────────────────────────────────────────────────
    // 트리플 버퍼링(2→3버퍼로 확장): 화면에 표시 중인 비트맵을 백그라운드
    // 스레드가 "바로 다음 프레임"에 다시 덮어쓰지 않도록 한 프레임 더
    // 여유를 준다. postInvalidate()는 비동기라 실제 GPU 업로드/드로우 완료
    // 시점을 보장하지 않는데, 2버퍼만 쓰면 방금 표시를 시작한 비트맵을
    // 곧바로(다음 루프, ~50ms 뒤) 재사용해 버려 그리기 도중 픽셀이 바뀌는
    // 레이스가 생길 수 있다(잔여 깜빡임의 원인으로 추정). 3버퍼로 늘리면
    // 같은 버퍼가 다시 쓰기 대상이 되기까지 최소 한 프레임(~50ms)의
    // 추가 여유가 생겨 이 레이스 창을 크게 줄인다.
    private val NUM_LCD_BUFFERS = 3
    private var lcdBitmaps: Array<Bitmap?> = arrayOfNulls(NUM_LCD_BUFFERS)
    private var writeIdx = 0   // 다음에 native 프레임을 채워 넣을 버퍼 인덱스

    private val lcdThread = HandlerThread("LcdRenderThread").apply { start() }
    private val lcdBgHandler = Handler(lcdThread.looper)
    private val uiHandler = Handler(Looper.getMainLooper())
    private var lcdRunning = false

    private val lcdBgRunnable = object : Runnable {
        override fun run() {
            renderLcdFrameOnBgThread()
            if (lcdRunning) lcdBgHandler.postDelayed(this, LCD_FPS_INTERVAL_MS)
        }
    }

    // ── 악기명 패널 (MT-32/SoundFont용, SC-55 LCD보다 훨씬 가벼운 텍스트 폴링) ────
    private var instrumentPanelRunning = false
    private val INSTRUMENT_PANEL_INTERVAL_MS = 400L
    private val instrumentPanelRunnable = object : Runnable {
        override fun run() {
            when (activeEngineType) {
                EngineType.MUNT -> if (muntEngine.engineRunning) updateMuntPanel()
                EngineType.SOUNDFONT -> if (sfEngine.engineRunning) tvInstrumentPanel.text = sfEngine.getChannelPanelText()
                else -> {}
            }
            if (instrumentPanelRunning) uiHandler.postDelayed(this, INSTRUMENT_PANEL_INTERVAL_MS)
        }
    }

    private fun startInstrumentPanel() {
        if (instrumentPanelRunning) return
        instrumentPanelRunning = true
        uiHandler.post(instrumentPanelRunnable)
    }

    private fun stopInstrumentPanel() {
        instrumentPanelRunning = false
        uiHandler.removeCallbacks(instrumentPanelRunnable)
    }

    // ── MT-32 LED 패널 (munt-android 원본 GUI 재현) ────────────────────
    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()

    private fun buildMuntPanelViews() {
        if (llMuntPanel.childCount > 0) return
        for (i in 0..8) {
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
                ).also { it.topMargin = dp(3) }
            }
            val led = android.view.View(this).apply {
                layoutParams = LinearLayout.LayoutParams(dp(14), dp(14)).also { it.marginEnd = dp(8) }
                background = android.graphics.drawable.GradientDrawable().apply {
                    shape = android.graphics.drawable.GradientDrawable.OVAL
                    setColor(LED_OFF)
                }
            }
            val label = TextView(this).apply {
                text = if (i < 8) "CH${i + 2}" else "CH10"
                setTextColor(0xFF888888.toInt())
                textSize = 11f
                layoutParams = LinearLayout.LayoutParams(dp(48), LinearLayout.LayoutParams.WRAP_CONTENT)
            }
            val patch = TextView(this).apply {
                text = "---"
                setTextColor(0xFF00FF88.toInt())
                textSize = 12f
                typeface = android.graphics.Typeface.MONOSPACE
            }
            row.addView(led); row.addView(label); row.addView(patch)
            llMuntPanel.addView(row)
            muntLeds[i] = led
            muntPatchNames[i] = patch
        }
    }

    private fun updateMuntPanel() {
        val info = muntEngine.getPartInfo()
        for (i in 0..8) {
            val on = (info.states shr i) and 1L != 0L
            val rhy = i == 8
            val color = when {
                on && rhy -> LED_RHY_ON
                on        -> LED_ON
                rhy       -> LED_RHY_OFF
                else      -> LED_OFF
            }
            (muntLeds[i]?.background as? android.graphics.drawable.GradientDrawable)?.setColor(color)
            muntPatchNames[i]?.text = info.names.getOrElse(i) { "---" }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        sc55Engine = SC55Engine(this)
        sc55Engine.onStatus = { msg -> status(msg) }
        sfEngine = SoundFontEngine(this)
        sfEngine.onStatus = { msg -> status(msg) }
        muntEngine = MuntEngine(this)
        muntEngine.onStatus = { msg -> status(msg) }

        rgConnection = findViewById(R.id.rgConnection)
        rgEngine     = findViewById(R.id.rgEngine)
        rbEngineSoundfont = findViewById(R.id.rbEngineSoundfont)
        rbEngineMunt = findViewById(R.id.rbEngineMunt)
        layoutSoundFontPicker = findViewById(R.id.layoutSoundFontPicker)
        tvSoundFontName = findViewById(R.id.tvSoundFontName)
        btnPickSoundFont = findViewById(R.id.btnPickSoundFont)
        btnConnectToggle = findViewById(R.id.btnConnectToggle)
        btnResetEngine   = findViewById(R.id.btnResetEngine)
        btnRomHelp  = findViewById(R.id.btnRomHelp)
        tvStatus    = findViewById(R.id.tvStatus)
        tvRomStatus = findViewById(R.id.tvRomStatus)
        romStatusRow = findViewById(R.id.romStatusRow)
        lcdFrame    = findViewById(R.id.lcdFrame)
        ivLcd       = findViewById(R.id.ivLcd)
        tvInstrumentPanel = findViewById(R.id.tvInstrumentPanel)
        llMuntPanel = findViewById(R.id.llMuntPanel)
        buildMuntPanelViews()
        // FIX (깜빡임): nativeGetLcdFrame()이 JNI AndroidBitmap_lockPixels/unlockPixels로
        // 비트맵 픽셀을 직접 쓰는데, 이런 native 측 픽셀 변경은 HWUI가
        // 텍스처 재업로드 여부를 판단하는 generation 카운터를 거치지 않아,
        // 하드웨어 가속 Canvas가 이전 GPU 텍스처를 계속 재사용해버릴 수 있다
        // (잔여 깜빡임의 유력한 원인으로 추정). 이 View만 소프트웨어 레이어로
        // 강제하면 매 프레임 Skia가 직접 픽셀을 베난바스에 blit하기 때문에
        // 이 문제가 생기지 않는다. 크기가 작아(741x268) 성능 부담도 미미하다.
        ivLcd.setLayerType(android.view.View.LAYER_TYPE_SOFTWARE, null)

        // 모델 선택 UI 제거됨 — rom_sc55 폴더의 ROM으로 바로 실행
        findViewById<Spinner>(R.id.spinnerModel)?.visibility = android.view.View.GONE

        btnResetEngine.isEnabled = false

        rgEngine.setOnCheckedChangeListener { _, _ -> onEngineSelectionChanged() }
        btnPickSoundFont.setOnClickListener { showSoundFontPicker() }
        btnConnectToggle.setOnClickListener {
            if (activeEngineType == null) onConnectClicked() else stopAll()
        }
        btnResetEngine.setOnClickListener {
            currentEngine()?.let {
                it.resetEngine(true)
                status("🔄 기기 초기화됨")
            }
        }
        btnRomHelp.setOnClickListener { showRomHelp() }

        onEngineSelectionChanged()
        checkAndRequestStoragePermission()
    }

    override fun onResume() {
        super.onResume()
        updateRomStatus()
    }

    // ── 재생 엔진 선택 UI 반영 ───────────────────────────────────────────
    private fun onEngineSelectionChanged() {
        val isSoundFont = rbEngineSoundfont.isChecked
        val isMunt = rbEngineMunt.isChecked
        layoutSoundFontPicker.visibility = if (isSoundFont) android.view.View.VISIBLE else android.view.View.GONE
        // LCD는 SC-55 전용 (munt/SoundFont는 실제 LCD 컨트롤러 에뮬레이션이 없음)
        lcdFrame.visibility = if (!isSoundFont && !isMunt) android.view.View.VISIBLE else android.view.View.GONE
        // 악기명 패널은 각 엔진별로 분리 (MUNT는 LED패널, SoundFont는 텍스트패널)
        llMuntPanel.visibility = if (isMunt) android.view.View.VISIBLE else android.view.View.GONE
        tvInstrumentPanel.visibility = if (isSoundFont) android.view.View.VISIBLE else android.view.View.GONE
        // ROM 상태 표시는 ROM 파일이 필요한 SC-55/munt에서만 (SoundFont는 .sf2 파일 선택 UI로 대체)
        romStatusRow.visibility = if (isSoundFont) android.view.View.GONE else android.view.View.VISIBLE
        if (isSoundFont) refreshSoundFontSelection()
        else updateRomStatus()
    }

    // ── 사운드폰트 선택 ──────────────────────────────────────────────────
    private fun refreshSoundFontSelection() {
        val lastPath = prefs.getString("last_soundfont", null)
        val files = sfEngine.getSoundFontFileList()
        val preselect = when {
            lastPath != null && File(lastPath).exists() -> File(lastPath)
            files.isNotEmpty() -> files.first()
            else -> null
        }
        if (preselect != null) {
            selectedSoundFontPath = preselect.absolutePath
            tvSoundFontName.text = preselect.name
        } else {
            selectedSoundFontPath = null
            tvSoundFontName.text = "선택된 사운드폰트 없음 (${sfEngine.SOUNDFONT_DIR})"
        }
    }

    private fun showSoundFontPicker() {
        val files = sfEngine.getSoundFontFileList()
        if (files.isEmpty()) {
            AlertDialog.Builder(this)
                .setTitle("사운드폰트 없음")
                .setMessage("다음 폴더에 .sf2 파일을 넣어주세요:\n\n${sfEngine.SOUNDFONT_DIR}")
                .setPositiveButton("확인", null)
                .show()
            return
        }
        val names = files.map { it.name }.toTypedArray()
        val currentIdx = files.indexOfFirst { it.absolutePath == selectedSoundFontPath }.coerceAtLeast(0)
        AlertDialog.Builder(this)
            .setTitle("사운드폰트 선택")
            .setSingleChoiceItems(names, currentIdx) { dialog, which ->
                val picked = files[which]
                selectedSoundFontPath = picked.absolutePath
                tvSoundFontName.text = picked.name
                prefs.edit().putString("last_soundfont", picked.absolutePath).apply()
                dialog.dismiss()
            }
            .setNegativeButton("취소", null)
            .show()
    }

    // ── LCD (실제 SC-55 LCD 컨트롤러 에뮬레이션을 그대로 렌더링) ────────────
    private fun ensureBitmaps() {
        if (lcdBitmaps[0] != null) return
        val w = sc55Engine.nativeGetLcdWidth().coerceAtLeast(1)
        val h = sc55Engine.nativeGetLcdHeight().coerceAtLeast(1)
        for (i in 0 until NUM_LCD_BUFFERS) {
            lcdBitmaps[i] = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        }
    }

    private fun startLcdUpdates() {
        if (lcdRunning) return
        ensureBitmaps()
        lcdRunning = true
        lcdBgHandler.post(lcdBgRunnable)
    }

    private fun stopLcdUpdates() {
        lcdRunning = false
        lcdBgHandler.removeCallbacks(lcdBgRunnable)
    }

    // 백그라운드 스레드에서 실행: native 프레임을 "쓰기용" 비트맵에 채우고,
    // 완료되면 UI 스레드에 "이제 이 비트맵을 보여줘"라고 가볍게 알린다.
    private fun renderLcdFrameOnBgThread() {
        if (!sc55Engine.engineRunning) return
        val target = lcdBitmaps[writeIdx] ?: return
        val ok = sc55Engine.nativeGetLcdFrame(target)
        if (!ok) return
        ivLcd.setFrame(target)
        writeIdx = (writeIdx + 1) % NUM_LCD_BUFFERS
        ivLcd.postInvalidate()
    }

    // ── 권한 ─────────────────────────────────────────────────────────────
    private fun hasStoragePermission(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            Environment.isExternalStorageManager()
        else
            ContextCompat.checkSelfPermission(
                this, Manifest.permission.READ_EXTERNAL_STORAGE
            ) == PackageManager.PERMISSION_GRANTED

    private fun checkAndRequestStoragePermission() {
        if (hasStoragePermission()) { onPermissionsReady(); return }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            AlertDialog.Builder(this)
                .setTitle("파일 접근 권한 필요")
                .setMessage("ROM/사운드폰트 파일을 읽으려면 \"모든 파일 접근\" 권한이 필요합니다.")
                .setPositiveButton("설정 열기") { _, _ ->
                    @Suppress("DEPRECATION")
                    startActivityForResult(
                        Intent(
                            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                            Uri.parse("package:$packageName")
                        ),
                        REQ_MANAGE_STORAGE
                    )
                }
                .setNegativeButton("취소") { _, _ -> status("권한 없음 — 파일을 읽을 수 없습니다") }
                .setCancelable(false)
                .show()
        } else {
            ActivityCompat.requestPermissions(
                this, arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE), REQ_LEGACY_STORAGE
            )
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_MANAGE_STORAGE) {
            if (Environment.isExternalStorageManager()) onPermissionsReady()
            else status("권한 거부됨 — 설정에서 허용해주세요")
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, results: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, results)
        if (requestCode == REQ_LEGACY_STORAGE &&
            results.isNotEmpty() && results[0] == PackageManager.PERMISSION_GRANTED)
            onPermissionsReady()
        else
            status("저장소 권한 거부됨")
    }

    private fun onPermissionsReady() {
        updateRomStatus()
        if (rbEngineSoundfont.isChecked) refreshSoundFontSelection()
        status("준비됨 — ${sc55Engine.version()}")
    }

    // ── ROM 상태 ──────────────────────────────────────────────────────────
    private fun updateRomStatus() {
        if (!hasStoragePermission()) {
            tvRomStatus.text = "파일 접근 권한 없음"
            return
        }
        val romDirPath = if (rbEngineMunt.isChecked) muntEngine.ROM_DIR else sc55Engine.ROM_DIR
        val needed = if (rbEngineMunt.isChecked) muntEngine.getRomFileList() else sc55Engine.getRomFileList()
        val romDir = File(romDirPath)
        if (needed.isEmpty()) { tvRomStatus.text = "ROM 목록 조회 중…"; return }
        val found   = needed.count { File(romDir, it).exists() }
        val missing = needed.filter { !File(romDir, it).exists() }
        tvRomStatus.text = if (found == needed.size)
            "✅ ROM ${found}/${needed.size}개 확인\n$romDirPath"
        else
            "⚠️ ROM ${found}/${needed.size}개\n없음: ${missing.joinToString(", ")}"
    }

    private fun showRomHelp() {
        val helpText = if (rbEngineMunt.isChecked) muntEngine.getRomHelpText() else sc55Engine.getRomHelpText()
        AlertDialog.Builder(this)
            .setTitle("ROM 파일 안내")
            .setMessage(helpText)
            .setPositiveButton("확인", null)
            .show()
    }

    // ── 연결 시작 (①연결방식 + ②재생엔진 조합) ──────────────────────────
    private fun onConnectClicked() {
        val useSoundFont = rbEngineSoundfont.isChecked
        val useMunt = rbEngineMunt.isChecked
        val useRtp = findViewById<RadioButton>(R.id.rbConnRtp).isChecked
        val useUsbMidiDevice = findViewById<RadioButton>(R.id.rbConnUsbMidi).isChecked

        // 연결방식에 따라 실제 입력 경로를 열어주는 공통 함수.
        // USB MIDI기기 모드에서는 RTP/USB-Serial을 전혀 시작하지 않고,
        // EngineRegistry.active만 설정해서 UsbMidiDeviceService(안드로이드가 USB
        // MIDI 주변장치로 노출된 동안 시스템이 넣어주는 MIDI)이 이 엔진으로 바로
        // 전달되도록만 한다.
        fun startInputPath(engine: IEngine): Boolean {
            return when {
                useUsbMidiDevice -> {
                    EngineRegistry.active = engine
                    startUsbMidiPeripheral()
                    status("🎹 USB MIDI기기 모드 — Windows 등 PC에서 이 폰을 MIDI 입력장치로 선택하세요")
                    true
                }
                useRtp -> { engine.startRtp(); true }
                else -> engine.startUsb()
            }
        }

        if (useSoundFont) {
            val path = selectedSoundFontPath
            if (path == null) {
                status("⚠️ 먼저 사운드폰트 파일을 선택하세요")
                showSoundFontPicker()
                return
            }
            if (!sfEngine.initEngine(path)) return
            if (!startInputPath(sfEngine)) status("⚠️ USB 연결 대기 중 (권한 확인)")
            activeEngineType = EngineType.SOUNDFONT
            if (!useUsbMidiDevice) EngineRegistry.active = sfEngine
            startInstrumentPanel()
        } else if (useMunt) {
            if (!muntEngine.initEngine()) return
            if (!startInputPath(muntEngine)) status("⚠️ USB 연결 대기 중 (권한 확인)")
            activeEngineType = EngineType.MUNT
            if (!useUsbMidiDevice) EngineRegistry.active = muntEngine
            startInstrumentPanel()
        } else {
            if (!sc55Engine.initEngine()) return
            if (!startInputPath(sc55Engine)) status("⚠️ USB 연결 대기 중 (권한 확인)")
            startLcdUpdates()
            activeEngineType = EngineType.SC55
            if (!useUsbMidiDevice) EngineRegistry.active = sc55Engine
        }

        rgConnection.isEnabled = false
        for (i in 0 until rgConnection.childCount) rgConnection.getChildAt(i).isEnabled = false
        rgEngine.isEnabled = false
        for (i in 0 until rgEngine.childCount) rgEngine.getChildAt(i).isEnabled = false
        btnPickSoundFont.isEnabled = false
        btnConnectToggle.text = "⏹ 정지"
        btnConnectToggle.backgroundTintList = android.content.res.ColorStateList.valueOf(0xFF6B0000.toInt())
        btnResetEngine.isEnabled = true
    }

    private fun stopAll() {
        stopLcdUpdates()
        stopInstrumentPanel()
        when (activeEngineType) {
            EngineType.SC55 -> { sc55Engine.allNotesOff(); sc55Engine.stop() }
            EngineType.SOUNDFONT -> { sfEngine.allNotesOff(); sfEngine.stop() }
            EngineType.MUNT -> { muntEngine.allNotesOff(); muntEngine.stop() }
            null -> {}
        }
        activeEngineType = null
        EngineRegistry.active = null
        stopUsbMidiPeripheral()

        rgConnection.isEnabled = true
        for (i in 0 until rgConnection.childCount) rgConnection.getChildAt(i).isEnabled = true
        rgEngine.isEnabled = true
        for (i in 0 until rgEngine.childCount) rgEngine.getChildAt(i).isEnabled = true
        btnPickSoundFont.isEnabled = true
        btnConnectToggle.text = "▶ 연결 시작"
        btnConnectToggle.backgroundTintList = android.content.res.ColorStateList.valueOf(0xFF0F3460.toInt())
        btnResetEngine.isEnabled = false
        status("⏹ 정지됨")
        updateRomStatus()
    }

    fun status(msg: String) {
        Log.i(TAG, msg)
        runOnUiThread { tvStatus.text = msg }
    }

    override fun onDestroy() {
        super.onDestroy()
        stopLcdUpdates()
        stopInstrumentPanel()
        lcdThread.quitSafely()
        if (activeEngineType != null) stopAll()
    }
}
