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
import android.view.View
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.activity.OnBackPressedCallback
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.io.File

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "SC55-Main"
        private const val REQ_LEGACY_STORAGE = 1001
        private const val REQ_MANAGE_STORAGE = 1002
        private const val STATE_LCD_FULLSCREEN = "lcd_fullscreen"
        // 30fps(33ms)에서 20fps(50ms)로 완화: LCD_Render()가 lcd.mutex.try_lock()을
// 쓰고, 실패하면 조용히 프레임을 드롭한다(lcd.cpp 확인됨). 렌더 스레드와
// 폴링 스레드 사이의 위상이 자주 어긋나면서 프레임이 반복적으로 스킵되어
// 화면이 깜빡이는 것으로 보이는 현상 완화를 위해 폴링 간격을 넉넉하게.
private const val LCD_FPS_INTERVAL_MS = 50L // ~20fps
        // munt-android 원본 GUI의 LED 색상 그대로 재사용
    
    }

    private lateinit var rgConnection: RadioGroup
    private lateinit var rgEngine:     RadioGroup
    private lateinit var rbEngineSoundfont: RadioButton
    private lateinit var rbEngineMunt: RadioButton
    private lateinit var rbEngineGearmulator: RadioButton
    private lateinit var btnGearmulatorModel: Button
    private lateinit var rbEngineMu2000: RadioButton
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
    // 모든 모드가 베젤 안을 LCD 하나로 채운다 (채널별 LED 인디케이터 패널은 제거됨). 각 LCD는 LcdFramePump가
    // ~30fps로 갱신한다.
    //  - Munt(MT-32): 1줄 문자 LCD를 연두색 도트 LCD로 (VirtualLcd.renderText)
    //  - SoundFont(FluidSynth): MU2000 스타일 가상 LCD (MIDI에서 추적한 값, VirtualPanelState)
    //  - 88emu: 실제 LCD (SC-55/88 유리 LCD, SC-8850, MT-32/CM 도트). LCD가 없는 SC-8820은 가상 LCD
    //  - S-MU2000: 실제 LCD
    private lateinit var muntLcdView: LcdView
    private lateinit var muntLcdPump: LcdFramePump
    private lateinit var sfLcdView: LcdView
    private lateinit var sfLcdPump: LcdFramePump
    private lateinit var gearLcdView: LcdView
    private lateinit var gearLcdPump: LcdFramePump
    private lateinit var mu2000LcdView: LcdView
    private lateinit var mu2000LcdPump: LcdFramePump

    private lateinit var sc55Engine: SC55Engine
    private lateinit var sfEngine:   SoundFontEngine
    private lateinit var muntEngine: MuntEngine
    private lateinit var gearmulatorEngine: GearmulatorEngine
    private lateinit var mu2000Engine: MU2000Engine
    private lateinit var midiPlayerPanel: MidiPlayerPanel

    // ── LCD 전체화면 ─────────────────────────────────────────────────────
    // 베젤 위의 전체화면 버튼을 누르면 LCD 외의 GUI(타이틀/MODE/LINK/상태줄/플레이어 바)를 숨기고
    // 시스템 바까지 감춰서(몰입 모드) LCD가 화면을 꽉 채운다. 다시 누르면(또는 뒤로가기) 원래대로 복귀.
    private lateinit var btnLcdFullscreen: ImageButton
    private var lcdFullscreen = false
    private val fullscreenBackCallback = object : OnBackPressedCallback(false) {
        override fun handleOnBackPressed() { setLcdFullscreen(false) }
    }

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

    // BUGFIX(버그 제보 대응 — 빠른 곡에서 USB MIDI만 끊김+노이즈): USB는
    // RTP-MIDI(WiFi)보다 훨씬 빨라서, 화음/아르페지오가 거의 동시에 onSend()로
    // 몰려 들어올 수 있다. 원래는 onSend() 안에서(=THREAD_PRIORITY_URGENT_AUDIO로
    // 도는 usbMidiThread에서 그대로) 파싱+디스패치까지 다 처리했는데, RTP-MIDI 쪽
    // 스레드도 같은 우선순위이긴 하지만 WiFi 특성상 이벤트가 자연히 퍼져서
    // 들어오는 반면 USB는 짧은 시간에 몰아치기 때문에, 그 순간 급한 우선순위
    // 스레드가 CPU를 오래 붙잡으며 오디오 렌더 스레드와 경합해 끊김을 유발하는
    // 것으로 추정된다. onSend()는 큐에 넣기만 하고 즉시 리턴하게 하고, 실제
    // 파싱/디스패치는 한 단계 낮은 우선순위의 별도 드레인 스레드가 처리하도록
    // 분리했다 — 다른 엔진 브리지들(SC55/SoundFont/S-YXG50)이 이미 쓰고 있는
    // "큐 + 전용 처리 스레드" 패턴과 동일한 원리.
    private val usbMidiQueue = java.util.concurrent.LinkedBlockingQueue<ByteArray>()
    private var usbMidiDrainThread: Thread? = null
    @Volatile private var usbMidiDrainRunning = false

    private fun startUsbMidiDrainThread() {
        if (usbMidiDrainRunning) return
        usbMidiDrainRunning = true
        usbMidiDrainThread = Thread({
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO)
            while (usbMidiDrainRunning) {
                val chunk = try {
                    usbMidiQueue.poll(200, java.util.concurrent.TimeUnit.MILLISECONDS)
                } catch (e: InterruptedException) { null }
                if (chunk != null) usbMidiParser.feed(chunk)
            }
        }, "UsbMidiDrainThread").apply { isDaemon = true; start() }
    }

    private fun stopUsbMidiDrainThread() {
        usbMidiDrainRunning = false
        usbMidiDrainThread?.interrupt()
        usbMidiDrainThread = null
        usbMidiQueue.clear()
    }

    private fun startUsbMidiPeripheral() {
        val midiManager = getSystemService(Context.MIDI_SERVICE) as android.media.midi.MidiManager
        val midiHandler = Handler(usbMidiThread.looper)

        // BUGFIX(버그 제보 대응): 리눅스/MiSTer 같은 호스트에서는 문제없이 붙는데,
        // 일부 안드로이드 기기는 USB MIDI 주변장치(peripheral) 게이트웨이가 시간이
        // 지나면 스스로 재협상되면서(원인 불명 — 안드로이드 플랫폼 쪽 USB 게이트웨이
        // 이슈로 추정) 겉보기엔 "연결됨" 상태인데 실제로는 죽은 포트를 붙잡고 있게
        // 되는 증상이 보고됨. 개발자설정에서 USB 연결모드를 MIDI가 아닌 걸로
        // 바꿨다가 다시 MIDI로 바꾸면 살아나는데, 이는 시스템이 강제로 장치를
        // 제거→재추가하기 때문 — 그런데 원래 코드는 onDeviceRemoved를 아예 처리
        // 안 해서, 새 장치가 추가돼도 죽은 포트/디바이스 참조가 안 정리되고
        // 남아있을 수 있었다. 아래에서 (a) 이미 연 장치를 다시 열려고 하면 무시,
        // (b) 다른 장치가 새로 열리면 이전 것부터 확실히 닫고, (c) 열려있던 장치가
        // 제거되면 참조를 정리하도록 고쳤다. 그래도 시스템이 이벤트 자체를 못 쏴주는
        // "좀비" 상태까지는 소프트웨어로 막을 수 없어서, "초기화" 버튼을 누르면
        // USB MIDI 연결을 강제로 닫았다 다시 여는 수동 복구 경로도 추가함(아래
        // btnResetEngine 리스너 참고) — 개발자설정을 직접 안 건드려도 앱 안에서
        // 복구할 수 있게.
        fun tryOpen(info: android.media.midi.MidiDeviceInfo) {
            if (usbMidiOpenDevice?.info == info) return // 이미 이 장치로 연결되어 있음
            // 다른 장치가 열려 있었다면 새로 열기 전에 확실히 정리
            runCatching { usbMidiOutputPort?.close() }
            runCatching { usbMidiOpenDevice?.close() }
            usbMidiOutputPort = null
            usbMidiOpenDevice = null

            midiManager.openDevice(info, { device ->
                if (device == null) return@openDevice
                usbMidiOpenDevice = device
                usbMidiOutputPort = device.openOutputPort(0)
                usbMidiOutputPort?.connect(object : android.media.midi.MidiReceiver() {
                    override fun onSend(data: ByteArray, offset: Int, count: Int, timestamp: Long) {
                        // 파싱/디스패치는 여기서 하지 않는다 — 이 콜백은 URGENT_AUDIO
                        // 우선순위 스레드에서 실행되므로, 큐에 넣고 즉시 리턴만 한다
                        // (실제 처리는 usbMidiDrainThread가 함, 위 주석 참고).
                        usbMidiQueue.offer(if (offset == 0 && count == data.size) data else data.copyOfRange(offset, offset + count))
                    }
                })
                status("✅ USB MIDI 장치 연결됨")
            }, midiHandler)
        }

        startUsbMidiDrainThread()

        // 연결 시점에 이미 보이는 장치 다 시도 (케이블이 이미 꽂혀 있는 경우)
        midiManager.devices.forEach { tryOpen(it) }

        // 케이블을 연결하는 시점이 앱 실행 이후일 수도 있으므로, 새 장치가 나타나는 것도 감지.
        // 장치가 사라지면(재협상/케이블 뽑힘 등) 참조도 같이 정리해서, 다음
        // onDeviceAdded가 정상적으로 새 연결을 맺을 수 있게 한다.
        usbMidiDeviceCallback = object : android.media.midi.MidiManager.DeviceCallback() {
            override fun onDeviceAdded(info: android.media.midi.MidiDeviceInfo) { tryOpen(info) }
            override fun onDeviceRemoved(info: android.media.midi.MidiDeviceInfo) {
                if (usbMidiOpenDevice?.info != info) return
                runCatching { usbMidiOutputPort?.close() }
                runCatching { usbMidiOpenDevice?.close() }
                usbMidiOutputPort = null
                usbMidiOpenDevice = null
                status("⚠️ USB MIDI 장치 연결 끊김 — 재연결 대기 중")
            }
        }
        midiManager.registerDeviceCallback(usbMidiDeviceCallback!!, midiHandler)
    }

    private fun stopUsbMidiPeripheral() {
        stopUsbMidiDrainThread()
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
    private enum class EngineType { SC55, SOUNDFONT, MUNT, GEARMULATOR, MU2000 }
    private var activeEngineType: EngineType? = null

    // 기기초기화 버튼이 지금 어느 엔진에 resetEngine()을 호출해야 하는지 공통적으로 찾기 위함
    private fun currentEngine(): IEngine? = when (activeEngineType) {
        EngineType.SC55 -> sc55Engine
        EngineType.SOUNDFONT -> sfEngine
        EngineType.MUNT -> muntEngine
        EngineType.GEARMULATOR -> gearmulatorEngine
        EngineType.MU2000 -> mu2000Engine
        null -> null
    }

    private val prefs by lazy { getSharedPreferences("nukedsc55_prefs", MODE_PRIVATE) }
    private var selectedSoundFontPath: String? = null

    // ── 재생 중 화면보호기/CPU 절전 방지 ────────────────────────────
    // 화면이 자동으로 꺼지면(화면보호기 진입) Android가 CPU를 저전력 상태로 내려
    // MCU/오디오 콜백 스레드가 밀려 음이 끊기거나 불안정해지는 현상이 있었다. 연결 중에는:
    //  1) FLAG_KEEP_SCREEN_ON 으로 화면보호기 자체가 안 켜지도록 하고,
    //  2) 혹시라도(사용자가 직접 잠금하는 등) 화면이 꺼져도 CPU는 계속 깨어있도록
    //     PARTIAL_WAKE_LOCK을 함께 잡아둔다 (화면/터치 입력은 여전히 꺼진 상태 유지).
    private var wakeLock: android.os.PowerManager.WakeLock? = null

    private fun acquirePlaybackWakeLock() {
        if (wakeLock?.isHeld == true) return
        val pm = getSystemService(Context.POWER_SERVICE) as android.os.PowerManager
        wakeLock = pm.newWakeLock(
            android.os.PowerManager.PARTIAL_WAKE_LOCK, "E-rayMIDI:playback"
        ).apply { setReferenceCounted(false); acquire() }
        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private fun releasePlaybackWakeLock() {
        runCatching { if (wakeLock?.isHeld == true) wakeLock?.release() }
        wakeLock = null
        window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

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

    // ── 엔진별 LCD 갱신 (LcdFramePump) ───────────────────────────────────
    // 예전 이름(startInstrumentPanel/stopInstrumentPanel)을 그대로 두고 내용만 바꿨다 — 호출하는 곳이 많다.
    // 선택된 엔진의 LCD만 돌리고, 화면이 꺼지면(onPause) 전부 멈춘다.
    private fun startInstrumentPanel() {
        when (activeEngineType) {
            EngineType.MUNT -> muntLcdPump.start()
            EngineType.SOUNDFONT -> sfLcdPump.start()
            EngineType.GEARMULATOR -> gearLcdPump.start()
            EngineType.MU2000 -> mu2000LcdPump.start()
            else -> {}
        }
    }

    private fun stopInstrumentPanel() {
        if (!::muntLcdPump.isInitialized) return
        muntLcdPump.stop(); sfLcdPump.stop(); gearLcdPump.stop(); mu2000LcdPump.stop()
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        sc55Engine = SC55Engine(this)
        sc55Engine.onStatus = { msg -> status(msg) }
        sfEngine = SoundFontEngine(this)
        sfEngine.onStatus = { msg -> status(msg) }
        muntEngine = MuntEngine(this)
        muntEngine.onStatus = { msg -> status(msg) }
        gearmulatorEngine = GearmulatorEngine(this)
        gearmulatorEngine.onStatus = { msg -> status(msg) }
        mu2000Engine = MU2000Engine(this)
        mu2000Engine.onStatus = { msg -> status(msg) }

        rgConnection = findViewById(R.id.rgConnection)
        rgEngine     = findViewById(R.id.rgEngine)
        rbEngineSoundfont = findViewById(R.id.rbEngineSoundfont)
        rbEngineMunt = findViewById(R.id.rbEngineMunt)
        rbEngineGearmulator = findViewById(R.id.rbEngineGearmulator)
        btnGearmulatorModel = findViewById(R.id.btnGearmulatorModel)
        rbEngineMu2000 = findViewById(R.id.rbEngineMu2000)
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
        // ── LCD 뷰와 프레임 공급 (모든 모드가 같은 구조) ──
        muntLcdView = findViewById(R.id.muntLcdView)
        sfLcdView = findViewById(R.id.sfLcdView)
        gearLcdView = findViewById(R.id.gearLcdView)
        mu2000LcdView = findViewById(R.id.mu2000LcdView)
        // SC-55용 ivLcd와 같은 이유(네이티브가 비트맵 픽셀을 직접 씀)로 소프트웨어 레이어 사용
        for (v in listOf(muntLcdView, sfLcdView, gearLcdView, mu2000LcdView))
            v.setLayerType(android.view.View.LAYER_TYPE_SOFTWARE, null)

        // Munt(MT-32): 1줄 20문자 LCD -> 연두색 도트 LCD. 문자열이 바뀐 경우에만 다시 그린다 (100ms 폴링)
        val muntText = TextLcdSource { muntEngine.getLcdText() }
        muntLcdPump = LcdFramePump(
            view = muntLcdView, handler = lcdBgHandler,
            isRunning = { muntEngine.engineRunning },
            sizeProvider = { muntText.size() },
            seqProvider = { muntText.tick() },
            fill = { bmp -> muntText.render(bmp) },
            intervalMs = 100L
        )

        // SoundFont(FluidSynth): MU2000 스타일 가상 LCD. 탭 = 다음 파트 선택, 길게 누르기 = 자동 추적
        sfLcdPump = LcdFramePump(
            view = sfLcdView, handler = lcdBgHandler,
            isRunning = { sfEngine.engineRunning },
            sizeProvider = { VirtualLcd.panelSize() },
            seqProvider = { sfEngine.panel.tick() },
            fill = { bmp -> sfEngine.panel.render(bmp) }
        )
        sfLcdView.setOnClickListener { sfEngine.panel.cycleFocus() }
        sfLcdView.setOnLongClickListener { sfEngine.panel.autoFocus(); true }

        // 88emu: 실제 LCD. LCD가 없는 기종(SC-8820)은 같은 MU2000 스타일 가상 LCD로 대신한다
        gearLcdPump = LcdFramePump(
            view = gearLcdView, handler = lcdBgHandler,
            isRunning = { gearmulatorEngine.engineRunning },
            sizeProvider = {
                if (gearmulatorEngine.isLcdLessModel()) VirtualLcd.panelSize() else gearmulatorEngine.nativeGetLcdSize()
            },
            seqProvider = {
                if (gearmulatorEngine.isLcdLessModel()) gearmulatorEngine.panel.tick() else gearmulatorEngine.nativeGetLcdSeq()
            },
            fill = { bmp ->
                if (gearmulatorEngine.isLcdLessModel()) gearmulatorEngine.panel.render(bmp) else gearmulatorEngine.nativeGetLcdFrame(bmp)
            }
        )
        gearLcdView.setOnClickListener { if (gearmulatorEngine.isLcdLessModel()) gearmulatorEngine.panel.cycleFocus() }
        gearLcdView.setOnLongClickListener {
            if (gearmulatorEngine.isLcdLessModel()) { gearmulatorEngine.panel.autoFocus(); true } else false
        }

        // S-MU2000: 실제 LCD
        mu2000LcdPump = LcdFramePump(
            view = mu2000LcdView, handler = lcdBgHandler,
            isRunning = { mu2000Engine.engineRunning },
            sizeProvider = { mu2000Engine.nativeGetLcdSize() },
            seqProvider = { mu2000Engine.nativeGetLcdSeq() },
            fill = { bmp -> mu2000Engine.nativeGetLcdFrame(bmp) }
        )
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
            // USB MIDI기기 모드에서 "연결은 됐는데 소리가 안 나오는" 좀비 상태
            // 제보 대응: 시스템이 onDeviceRemoved/onDeviceAdded를 못 쏴주는
            // 경우까지는 자동 복구가 안 되니, 초기화 버튼을 누르면 USB MIDI
            // 연결 자체도 강제로 닫았다 다시 열어서 수동 복구 경로를 제공한다.
            if (findViewById<RadioButton>(R.id.rbConnUsbMidi).isChecked && activeEngineType != null) {
                stopUsbMidiPeripheral()
                startUsbMidiPeripheral()
                status("🔄 USB MIDI 연결 재시작됨")
            }
            // MIDI 파일 모드에서는 "초기화" = 재생 중인 곡 정지 (엔진 상태는 위에서 이미 정리됨)
            if (findViewById<RadioButton>(R.id.rbConnMidiFile).isChecked && activeEngineType != null) {
                midiPlayerPanel.stopSong()
            }
        }
        btnRomHelp.setOnClickListener { showRomHelp() }
        btnGearmulatorModel.setOnClickListener { showGearmulatorModelPicker() }

        midiPlayerPanel = MidiPlayerPanel(
            activity = this,
            prefs = prefs,
            status = { msg -> status(msg) },
            canPlay = { EngineRegistry.active != null }
        )

        onEngineSelectionChanged()
        checkAndRequestStoragePermission()

        btnLcdFullscreen = findViewById(R.id.btnLcdFullscreen)
        btnLcdFullscreen.setOnClickListener { setLcdFullscreen(!lcdFullscreen) }
        onBackPressedDispatcher.addCallback(this, fullscreenBackCallback)
        if (savedInstanceState?.getBoolean(STATE_LCD_FULLSCREEN) == true) setLcdFullscreen(true)
    }

    override fun onResume() {
        super.onResume()
        updateRomStatus()
        // FIX (배터리): 앱이 백그라운드로 갔다가 돌아오면, 연결 중이던 엔진에 맞는
        // 화면 갱신(LCD 또는 악기패널)만 다시 켜준다.
        when (activeEngineType) {
            EngineType.SC55 -> startLcdUpdates()
            EngineType.GEARMULATOR -> { gearLcdPump.start(); startInstrumentPanel() }
            EngineType.MU2000 -> { mu2000LcdPump.start(); startInstrumentPanel() }
            EngineType.MUNT, EngineType.SOUNDFONT -> startInstrumentPanel()
            null -> {}
        }
        midiPlayerPanel.onResume()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(STATE_LCD_FULLSCREEN, lcdFullscreen)
    }

    // 다이얼로그/알림 등으로 포커스를 잃었다 돌아오면 시스템이 바를 다시 보여줄 수 있어서 몰입 모드를 재적용한다.
    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus && lcdFullscreen) applyImmersive(true)
    }

    private fun setLcdFullscreen(on: Boolean) {
        lcdFullscreen = on
        val vis = if (on) View.GONE else View.VISIBLE
        findViewById<View>(R.id.titleBar).visibility = vis
        findViewById<View>(R.id.colMode).visibility = vis
        findViewById<View>(R.id.colLink).visibility = vis
        findViewById<View>(R.id.statusRow).visibility = vis
        // 플레이어 바는 MIDI 파일 모드로 연결 중일 때만 보이는 게 원래 상태다 — 복귀 시 그 상태로 되돌린다
        findViewById<View>(R.id.llPlayerBar).visibility =
            if (!on && midiPlayerPanel.isActive) View.VISIBLE else View.GONE
        val pad = if (on) 0 else dp(8)
        findViewById<View>(R.id.rootLayout).setPadding(pad, pad, pad, pad)
        btnLcdFullscreen.setImageResource(if (on) R.drawable.ic_fullscreen_exit else R.drawable.ic_fullscreen)
        btnLcdFullscreen.contentDescription = if (on) "LCD 전체화면 해제" else "LCD 전체화면"
        fullscreenBackCallback.isEnabled = on
        applyImmersive(on)
    }

    private fun applyImmersive(on: Boolean) {
        val c = WindowCompat.getInsetsController(window, window.decorView)
        if (on) {
            c.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            c.hide(WindowInsetsCompat.Type.systemBars())
        } else {
            c.show(WindowInsetsCompat.Type.systemBars())
        }
    }

    override fun onPause() {
        super.onPause()
        // FIX (배터리 낭비 발견): 화면이 꺼지거나(화면보호기) 앱이 백그라운드로 갔을 때
        // 지금까지는 LCD가 초당 20회, 악기패널이 0.4초마다 계속 그려지고 있었다 —
        // 보이지도 않는 화면을 위해 CPU/배터리를 계속 쓰는 것. 오디오 자체는 native
        // 스레드/AAudio 콜백으로 도는 거라 UI 정지와 무관하게 계속 재생된다.
        stopLcdUpdates()
        gearLcdPump.stop()
        mu2000LcdPump.stop()
        stopInstrumentPanel()
        midiPlayerPanel.onPause()
    }

    // ── 재생 엔진 선택 UI 반영 ───────────────────────────────────────────
    private fun onEngineSelectionChanged() {
        val isSoundFont = rbEngineSoundfont.isChecked
        val isMunt = rbEngineMunt.isChecked
        val isGearmulator = rbEngineGearmulator.isChecked
        val isMu2000 = rbEngineMu2000.isChecked
        layoutSoundFontPicker.visibility = if (isSoundFont) android.view.View.VISIBLE else android.view.View.GONE
        // LCD는 SC-55 전용 (munt/SoundFont/88emu/MU2000는 실제 LCD 컨트롤러 에뮬레이션이 없음)
        lcdFrame.visibility = if (!isSoundFont && !isMunt && !isGearmulator && !isMu2000) android.view.View.VISIBLE else android.view.View.GONE
        // 엔진마다 LCD 하나 (한 번에 하나만 보인다)
        muntLcdView.visibility = if (isMunt) android.view.View.VISIBLE else android.view.View.GONE
        sfLcdView.visibility = if (isSoundFont) android.view.View.VISIBLE else android.view.View.GONE
        gearLcdView.visibility = if (isGearmulator) android.view.View.VISIBLE else android.view.View.GONE
        mu2000LcdView.visibility = if (isMu2000) android.view.View.VISIBLE else android.view.View.GONE
        btnGearmulatorModel.visibility = if (isGearmulator) android.view.View.VISIBLE else android.view.View.GONE
        if (isGearmulator) refreshGearmulatorModelButton()
        // ROM 상태 표시는 ROM 파일이 필요한 엔진에서만 (SoundFont는 .sf2 파일 선택 UI로 대체)
        romStatusRow.visibility = if (isSoundFont) android.view.View.GONE else android.view.View.VISIBLE
        if (isSoundFont) refreshSoundFontSelection()
        else updateRomStatus()
    }

    // ── Gearmulator 기종 선택 ────────────────────────────────────────────
    private fun refreshGearmulatorModelButton() {
        btnGearmulatorModel.text = "🎹 기종: ${gearmulatorEngine.currentModel.label} ▾"
        btnGearmulatorModel.isEnabled = !gearmulatorEngine.engineRunning
    }

    private fun showGearmulatorModelPicker() {
        if (gearmulatorEngine.engineRunning) {
            status("⚠️ 기종을 바꾸려면 먼저 연결을 끊어주세요")
            return
        }
        val models = GearmulatorEngine.Model.values()
        // ROM이 있는지 미리 확인해서 항목에 표시한다 (88lib의 내용 기반 판정,
        // GearmulatorEngine.isModelAvailable 참고).
        val items = models.map { m ->
            val mark = if (gearmulatorEngine.isModelAvailable(m)) "✅" else "⚠️"
            "$mark ${m.label}"
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Gearmulator 기종 선택")
            .setItems(items) { _, which ->
                val picked = models[which]
                if (gearmulatorEngine.selectModel(picked)) {
                    refreshGearmulatorModelButton()
                    updateRomStatus()
                    status("기종 선택: ${picked.label}")
                }
            }
            .setNegativeButton("취소", null)
            .show()
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
    //
    // BUGFIX (버그 제보 대응 — "LCD 화면이 여러 개 겹쳐 보인다"/체크무늬 노이즈,
    // 느린 기기 Android 10/Galaxy A50 등에서 재현): 원래는 버퍼 개수(3개)만큼
    // 라운드로빈으로 돌리면 안전할 거라는 "타이머 기준 추측"만으로 픽셀을 덮어썼다.
    // 느린 기기에서 onDraw()가 그 여유 시간(약 150ms)보다 오래 걸리면, 화면에
    // 그려지고 있는 바로 그 비트맵을 네이티브 스레드가 동시에 write하게 되어
    // 절반은 이전 프레임 절반은 새 프레임이 섞인 "찢어진" 이미지가 나왔다.
    // 이제 LcdView가 노출하는 실제 상태(지금 세팅된 비트맵 / 지금 onDraw()가
    // 실제로 읽고 있는 비트맵)를 확인해서, 그 어느 쪽에도 해당하지 않는
    // "확실히 안전한" 버퍼를 찾을 때까지 다음 버퍼를 시도한다. 셋 다 위험하면
    // (매우 드묾 — 기기가 극도로 느린 경우) 이번 프레임은 그냥 건너뛴다
    // (픽셀이 찢어지는 것보다 프레임 하나 스킵하는 게 훨씬 낫다).
    private fun renderLcdFrameOnBgThread() {
        if (!sc55Engine.engineRunning) return

        var target: Bitmap? = null
        for (attempt in 0 until NUM_LCD_BUFFERS) {
            val candidate = lcdBitmaps[writeIdx] ?: return
            val unsafe = candidate === ivLcd.getActiveBitmap() || ivLcd.isBeingDrawn(candidate)
            if (!unsafe) { target = candidate; break }
            writeIdx = (writeIdx + 1) % NUM_LCD_BUFFERS
        }
        if (target == null) return // 셋 다 사용 중 — 이번 프레임 스킵

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
        if (rbEngineGearmulator.isChecked) {
            // 88lib는 파일명이 아니라 내용으로 ROM을 식별하므로 다른 엔진과
            // 체크 방식이 다르다 (GearmulatorEngine.isModelAvailable 참고).
            val model = gearmulatorEngine.currentModel
            tvRomStatus.text = if (gearmulatorEngine.isModelAvailable(model))
                "✅ ${model.label} ROM 확인됨\n${gearmulatorEngine.ROM_DIR}"
            else
                "⚠️ ${model.label} ROM 없음\n${gearmulatorEngine.ROM_DIR}"
            return
        }
        if (rbEngineMu2000.isChecked) {
            tvRomStatus.text = if (mu2000Engine.isRomAvailable())
                "✅ MU2000 ROM 확인됨\n${mu2000Engine.ROM_DIR}"
            else
                "⚠️ MU2000 ROM 없음(프로그램/웨이브 4개 필요)\n${mu2000Engine.ROM_DIR}"
            return
        }
        val romDirPath = when {
            rbEngineMunt.isChecked -> muntEngine.ROM_DIR
            else -> sc55Engine.ROM_DIR
        }
        val needed = when {
            rbEngineMunt.isChecked -> muntEngine.getRomFileList()
            else -> sc55Engine.getRomFileList()
        }
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
        val helpText = when {
            rbEngineMunt.isChecked -> muntEngine.getRomHelpText()
            rbEngineGearmulator.isChecked -> gearmulatorEngine.getRomHelpText()
            rbEngineMu2000.isChecked -> mu2000Engine.getRomHelpText()
            else -> sc55Engine.getRomHelpText()
        }
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
        val useGearmulator = rbEngineGearmulator.isChecked
        val useMu2000 = rbEngineMu2000.isChecked
        val useRtp = findViewById<RadioButton>(R.id.rbConnRtp).isChecked
        val useUsbMidiDevice = findViewById<RadioButton>(R.id.rbConnUsbMidi).isChecked
        val useMidiFile = findViewById<RadioButton>(R.id.rbConnMidiFile).isChecked

        // 연결방식에 따라 실제 입력 경로를 열어주는 공통 함수.
        // USB MIDI기기 모드에서는 RTP/USB-Serial을 전혀 시작하지 않고,
        // EngineRegistry.active만 설정해서 UsbMidiDeviceService(안드로이드가 USB
        // MIDI 주변장치로 노출된 동안 시스템이 넣어주는 MIDI)이 이 엔진으로 바로
        // 전달되도록만 한다. MIDI 파일 모드도 마찬가지로 외부 입력을 열지 않고,
        // MidiPlayerPanel이 파일에서 읽은 MIDI 메시지를 이 엔진으로 직접 넣는다.
        fun startInputPath(engine: IEngine): Boolean {
            return when {
                useMidiFile -> {
                    EngineRegistry.active = engine
                    midiPlayerPanel.start(engine)
                    true
                }
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
        } else if (useGearmulator) {
            if (!gearmulatorEngine.initEngine(gearmulatorEngine.currentModel)) return
            if (!startInputPath(gearmulatorEngine)) status("⚠️ USB 연결 대기 중 (권한 확인)")
            activeEngineType = EngineType.GEARMULATOR
            if (!useUsbMidiDevice) EngineRegistry.active = gearmulatorEngine
            refreshGearmulatorModelButton()
            gearLcdPump.start()
            startInstrumentPanel()
        } else if (useMu2000) {
            if (!mu2000Engine.initEngine()) return
            if (!startInputPath(mu2000Engine)) status("⚠️ USB 연결 대기 중 (권한 확인)")
            activeEngineType = EngineType.MU2000
            if (!useUsbMidiDevice) EngineRegistry.active = mu2000Engine
            mu2000LcdPump.start()
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
        acquirePlaybackWakeLock()
    }

    private fun stopAll() {
        midiPlayerPanel.stop() // 엔진이 살아있는 상태에서 먼저 정리해야 소리 정리가 엔진에 전달됨
        stopLcdUpdates()
        gearLcdPump.stop()
        mu2000LcdPump.stop()
        stopInstrumentPanel()
        when (activeEngineType) {
            EngineType.SC55 -> { sc55Engine.allNotesOff(); sc55Engine.stop() }
            EngineType.SOUNDFONT -> { sfEngine.allNotesOff(); sfEngine.stop() }
            EngineType.MUNT -> { muntEngine.allNotesOff(); muntEngine.stop() }
            EngineType.GEARMULATOR -> { gearmulatorEngine.allNotesOff(); gearmulatorEngine.stop(); refreshGearmulatorModelButton() }
            EngineType.MU2000 -> { mu2000Engine.allNotesOff(); mu2000Engine.stop() }
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
        releasePlaybackWakeLock()
    }

    fun status(msg: String) {
        Log.i(TAG, msg)
        runOnUiThread { tvStatus.text = msg }
    }

    override fun onDestroy() {
        super.onDestroy()
        stopLcdUpdates()
        if (::gearLcdPump.isInitialized) gearLcdPump.stop()
        if (::mu2000LcdPump.isInitialized) mu2000LcdPump.stop()
        stopInstrumentPanel()
        releasePlaybackWakeLock()
        lcdThread.quitSafely()
        if (activeEngineType != null) stopAll()
    }
}
