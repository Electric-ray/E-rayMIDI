package com.example.nukedsc55

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.util.SerialInputOutputManager
import java.util.concurrent.Executors

/**
 * UsbMidiManager.kt
 * USB OTG 시리얼 → MIDI 관리자 (munt-android에서 그대로 포팅)
 *
 * 중요: ESP32는 USB-MIDI class 기기가 아니라 USB-Serial(CH340/CP210x 등) 기기입니다.
 * Android MidiManager API로는 절대 인식되지 않습니다 — usb-serial-for-android
 * 라이브러리로 raw 시리얼 바이트를 읽어 MidiStreamParser로 직접 파싱해야 합니다.
 *
 * Baud: 38400 (dos_midi.ino RS232_BAUD)
 */
class UsbMidiManager(
    private val context: Context,
    private val onMidi: (ByteArray) -> Unit
) {
    companion object {
        private const val BAUD       = 38400
        private const val ACTION_USB = "com.example.nukedsc55.USB_PERMISSION"
    }

    var onStatus: ((String) -> Unit)? = null

    private var port: UsbSerialPort? = null
    private var ioManager: SerialInputOutputManager? = null
    private var usbRawDumpCount = 0
    private val executor = Executors.newSingleThreadExecutor()
    private val parser = MidiStreamParser(onMidi)

    val isConnected get() = port != null

    private val permReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            if (intent.action != ACTION_USB) return
            val device: UsbDevice? = if (android.os.Build.VERSION.SDK_INT >= 33) {
                intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
            } else {
                @Suppress("DEPRECATION") intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
            }
            if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                onStatus?.invoke("USB 권한 허용됨 → 연결 중...")
                device?.let { openDevice(it) }
            } else {
                onStatus?.invoke("USB 권한 거부됨")
            }
            runCatching { context.unregisterReceiver(this) }
        }
    }

    fun connect(): Boolean {
        val usbMgr = context.getSystemService(Context.USB_SERVICE) as UsbManager
        val drivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbMgr)

        if (drivers.isEmpty()) {
            onStatus?.invoke("USB 시리얼 장치 없음\n(CH340/FTDI/CP210x 어댑터 확인)")
            return false
        }
        val driver = drivers.first()
        val device = driver.device

        if (!usbMgr.hasPermission(device)) {
            val pi = PendingIntent.getBroadcast(
                context, 0,
                Intent(ACTION_USB),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            )
            context.registerReceiver(permReceiver, IntentFilter(ACTION_USB))
            usbMgr.requestPermission(device, pi)
            onStatus?.invoke("USB 권한 요청 중... (다이얼로그 확인)")
            return false
        }
        return openDevice(device)
    }

    private fun openDevice(device: UsbDevice): Boolean {
        val usbMgr = context.getSystemService(Context.USB_SERVICE) as UsbManager
        val drivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbMgr)
        val driver = drivers.firstOrNull { it.device == device } ?: run {
            onStatus?.invoke("드라이버 없음"); return false
        }
        val conn = usbMgr.openDevice(device) ?: run {
            onStatus?.invoke("장치 열기 실패"); return false
        }
        port = driver.ports.first().also { p ->
            p.open(conn)
            p.setParameters(BAUD, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
            p.dtr = true; p.rts = true
        }
        parser.reset()
        usbRawDumpCount = 0
        ioManager = SerialInputOutputManager(port,
            object : SerialInputOutputManager.Listener {
                override fun onNewData(data: ByteArray) {
                    // FIX (RTP vs USB 비교용): RTP쪽과 동일하게 원시 시리얼 바이트를 그대로 찍는다.
                    // 처음 몇 초만 찍으면 되므로 200회로 제한.
                    if (usbRawDumpCount < 200) {
                        val hex = data.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
                        android.util.Log.i("UsbMidiRaw", "USB[$usbRawDumpCount] len=${data.size} [$hex]")
                        usbRawDumpCount++
                    }
                    parser.feed(data)
                }
                override fun onRunError(e: Exception) {
                    onStatus?.invoke("USB 오류: ${e.message}")
                    disconnect()
                }
            }).also { executor.submit(it) }

        onStatus?.invoke("✅ USB 연결됨: ${device.productName} @ ${BAUD}bps")
        return true
    }

    fun disconnect() {
        ioManager?.stop(); ioManager = null
        try { port?.close() } catch (_: Exception) {}
        port = null
        runCatching { context.unregisterReceiver(permReceiver) }
        onStatus?.invoke("USB 연결 해제")
    }
}
