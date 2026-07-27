package com.example.nukedsc55

import android.media.midi.MidiDeviceService
import android.media.midi.MidiReceiver
import android.util.Log

/**
 * UsbMidiDeviceService — 안드로이드 기기를 USB MIDI 주변장치(peripheral)로 노출한다.
 *
 * 이렇게 하면 USB 케이블로 연결된 Windows PC(또는 다른 MIDI 호스트)에서 이 안드로이드
 * 기기를 표준 클래스 컴플라이언트 MIDI 입력 장치로 인식할 수 있다 — 예를 들어
 * Windows의 DAW나 SoftMPU 같은 DOS MIDI 드라이버가 "이 안드로이드 폰"을 MIDI 출력
 * 대상으로 직접 선택할 수 있게 된다.
 *
 * (munt-android 프로젝트의 MuntMidiDeviceService.kt를 이식 — 원본은 MuntEngine
 * 하나에 고정돼 있었으나, 이 프로젝트는 SC55/SoundFont/MT-32 세 엔진을 갖고 있으므로
 * EngineRegistry.active(현재 연결된 엔진)로 라우팅하도록 일반화했다. MainActivity가
 * 엔진을 연결/해제할 때 EngineRegistry.active를 갱신해줘야 이 서비스가 정상 동작한다.)
 *
 * 실제로 동작하려면:
 *  1. AndroidManifest.xml에 <uses-feature android:name="android.software.midi"/> 와
 *     이 서비스의 <service> 선언(intent-filter + meta-data)이 있어야 한다.
 *  2. res/xml/midi_device_info.xml에 입력 포트 정의가 있어야 한다.
 *  3. 기기가 USB 주변장치(peripheral) 모드의 MIDI를 지원해야 한다(기기/OS 의존적 —
 *     LG Velvet 포함 대부분의 폰은 OTG 케이블로 호스트에 연결 시 이 모드를 지원).
 */
class UsbMidiDeviceService : MidiDeviceService() {

    companion object {
        private const val TAG = "UsbMidiDeviceService"
    }

    // MidiStreamParser가 완성된 메시지를 만들면 현재 활성 엔진으로 그대로 넘긴다.
    private val parser = MidiStreamParser { bytes ->
        EngineRegistry.active?.dispatchMidi(bytes)
            ?: Log.w(TAG, "MIDI 수신했지만 활성 엔진 없음 — 드롭 (${bytes.size}B)")
    }

    override fun onGetInputPortReceivers(): Array<MidiReceiver> {
        Log.i(TAG, "USB MIDI 입력 포트 열림 (호스트가 연결됨)")
        return arrayOf(object : MidiReceiver() {
            override fun onSend(data: ByteArray, offset: Int, count: Int, timestamp: Long) {
                if (count <= 0) return
                parser.feed(if (offset == 0 && count == data.size) data else data.copyOfRange(offset, offset + count))
            }
        })
    }

    override fun onDeviceStatusChanged(status: android.media.midi.MidiDeviceStatus) {
        Log.i(TAG, "MIDI 장치 상태 변경: ${status}")
    }
}
