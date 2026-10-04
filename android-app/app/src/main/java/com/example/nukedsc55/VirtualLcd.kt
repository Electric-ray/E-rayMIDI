package com.example.nukedsc55

import android.graphics.Bitmap

/**
 * 가상 LCD (네이티브 vlcd-jni).
 *
 * LCD 하드웨어가 없는 엔진(SoundFont/FluidSynth, SC-8820)과 MT-32 문자 LCD(Munt)를 앱의 다른
 * LCD(88emu, S-MU2000)와 같은 모양으로 그린다 — 두 렌더러(Lcd2000Renderer, Lcd88Renderer)를 그대로
 * 재사용하므로 모든 모드의 화면이 한 가족처럼 보인다.
 */
object VirtualLcd {
    init { System.loadLibrary("vlcd-jni") }

    /** MU2000 스타일 가상 패널의 비트맵 크기: (width shl 16) or height */
    external fun panelSize(): Int
    /** params는 [VirtualPanelState]가 만든 32개 정수, name은 ASCII 최대 8바이트 */
    external fun renderPanel(bitmap: Bitmap, params: IntArray, name: ByteArray): Boolean

    /** 1줄 20문자 문자 LCD(MT-32/CM 연두색 유리)의 비트맵 크기 */
    external fun textSize(): Int
    external fun renderText(bitmap: Bitmap, codes: IntArray): Boolean
}

/** 문자열 하나를 문자 LCD 프레임으로 공급하는 소스 (LcdFramePump의 size/seq/fill 계약). */
class TextLcdSource(private val provider: () -> String) {
    private var last = ""
    private var seq = 0L

    fun size(): Int = VirtualLcd.textSize()

    /** 문자열이 바뀐 경우에만 카운터가 증가한다 */
    fun tick(): Long {
        val s = runCatching(provider).getOrDefault("")
        if (s != last) { last = s; seq++ }
        return seq
    }

    fun render(bitmap: Bitmap): Boolean {
        val codes = IntArray(20) { i -> if (i < last.length) last[i].code else 0x20 }
        return VirtualLcd.renderText(bitmap, codes)
    }
}
