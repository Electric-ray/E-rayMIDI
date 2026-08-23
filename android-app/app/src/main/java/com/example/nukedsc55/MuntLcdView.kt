package com.example.nukedsc55

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.util.AttributeSet
import android.view.View

/**
 * MuntLcdView — MT-32/CM-32L의 실제 텍스트 LCD(mt32emu Display.cpp가 그대로
 * 재현하는 문자열, munt-qt/PC용 munt와 동일)를 SC-55 LcdView처럼 "그래픽
 * LCD"로 보이게 그린다.
 *
 * v2 (사용자 피드백 반영):
 *  - 문자를 통짜 문자열로 한 번에 그리면, 폰트에 따라 특수문자(가득찬
 *    블록 등)의 자연 폭이 숫자와 달라 위치가 흔들렸다(늘었다 줄었다).
 *    → 각 칸을 고정폭 셀로 만들어 글자 중심에 정렬해서 그린다.
 *  - 가로/세로를 따로 늘려서(뷰 크기에 맞춰 stretch) 도트가 옆으로 길쭉하게
 *    늘어나며 전체적으로 너무 커 보였다.
 *    → 가로세로 비율을 유지하는 단일 배율로 계산하고, 남는 공간은 중앙 정렬.
 *  - 실제 LCD는 도트 하나하나가 살짝 떨어져 있는데 지금은 그냥 뭉쳐 있었다.
 *    → 비트맵을 그대로 확대해 붙여넣는 대신, 픽셀 하나하나를 개별 사각형으로
 *      그리면서 아주 약간의 간격(gap)을 둬서 실제 도트매트릭스 느낌을 낸다.
 */
class MuntLcdView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {

    private var text: String = ""

    private val bgColor = 0xFF1c2e12.toInt()
    private val fgColor = 0xFFc8e020.toInt()

    private val bgPaint = Paint().apply { color = bgColor }
    private val dotPaint = Paint().apply { color = fgColor; isAntiAlias = false }
    private val textPaint = Paint().apply {
        color = fgColor
        isAntiAlias = false
        typeface = Typeface.MONOSPACE
        textAlign = Paint.Align.CENTER // 칸 중앙에 그려서 고정폭 셀을 만든다
    }

    private val charsPerLine = 20
    private val tinyCharW = 8
    private val tinyCharH = 12
    private val tinyW = charsPerLine * tinyCharW
    private val tinyH = tinyCharH
    private val tinyBitmap = Bitmap.createBitmap(tinyW, tinyH, Bitmap.Config.ARGB_8888)
    private val tinyCanvas = Canvas(tinyBitmap)
    private val pixels = IntArray(tinyW * tinyH)

    // 도트 사이 간격 비율 (도트 한 칸 크기 대비). 너무 크면 글자가 흐려 보이고
    // 너무 작으면 뭉쳐 보인다 — 실제 LCD 느낌에 맞춘 값.
    private val gapFraction = 0.16f

    init {
        textPaint.textSize = tinyCharH * 0.92f
    }

    fun setText(newText: String) {
        val padded = newText.padEnd(charsPerLine).take(charsPerLine)
        if (padded == text) return
        text = padded
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width
        val h = height
        if (w <= 0 || h <= 0) return

        canvas.drawRect(0f, 0f, w.toFloat(), h.toFloat(), bgPaint)
        if (text.isBlank()) return

        // 고정폭 셀: 문자마다 자기 칸의 중앙에 그린다 (폭이 흔들리지 않음)
        tinyCanvas.drawColor(bgColor)
        val baseline = tinyCharH * 0.82f
        for (i in text.indices) {
            val cx = i * tinyCharW + tinyCharW / 2f
            tinyCanvas.drawText(text[i].toString(), cx, baseline, textPaint)
        }
        tinyBitmap.getPixels(pixels, 0, tinyW, 0, 0, tinyW, tinyH)

        // 종횡비를 유지하는 단일 배율 + 중앙 정렬
        val scale = minOf(w / tinyW.toFloat(), h / tinyH.toFloat())
        val drawW = tinyW * scale
        val drawH = tinyH * scale
        val offsetX = (w - drawW) / 2f
        val offsetY = (h - drawH) / 2f
        val gap = scale * gapFraction

        for (y in 0 until tinyH) {
            for (x in 0 until tinyW) {
                if (pixels[y * tinyW + x] == fgColor) {
                    val left = offsetX + x * scale + gap / 2f
                    val top = offsetY + y * scale + gap / 2f
                    canvas.drawRect(
                        left, top,
                        left + scale - gap, top + scale - gap,
                        dotPaint
                    )
                }
            }
        }
    }
}
