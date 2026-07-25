package com.example.nukedsc55

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.util.AttributeSet
import android.view.View

/**
 * SC-55 LCD를 표시하는 커스텀 View.
 *
 * ImageView.setImageBitmap()을 매 프레임(30fps) 호출하면 내부적으로 새
 * Drawable을 생성하고 레이아웃을 재계산하는 오버헤드가 있어 깜빡임의
 * 원인이 될 수 있다. 이 View는 표시할 Bitmap 참조만 갈아끼우고
 * invalidate()만 호출해서, onDraw()에서 Canvas.drawBitmap()으로 직접
 * 그린다 — 훨씬 가볍고 안정적이다.
 */
class LcdView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : View(context, attrs) {

    @Volatile
    private var currentBitmap: Bitmap? = null

    /** 백그라운드 스레드에서 완성된 프레임을 세팅. UI 스레드에서 invalidate 필요. */
    fun setFrame(bitmap: Bitmap) {
        currentBitmap = bitmap
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val bmp = currentBitmap ?: return
        val viewW = width.toFloat()
        val viewH = height.toFloat()
        if (viewW <= 0f || viewH <= 0f) return

        val bmpW = bmp.width.toFloat()
        val bmpH = bmp.height.toFloat()
        val scale = minOf(viewW / bmpW, viewH / bmpH)
        val drawW = bmpW * scale
        val drawH = bmpH * scale
        val left = (viewW - drawW) / 2f
        val top  = (viewH - drawH) / 2f

        val srcRect = android.graphics.Rect(0, 0, bmp.width, bmp.height)
        val dstRect = android.graphics.RectF(left, top, left + drawW, top + drawH)
        canvas.drawBitmap(bmp, srcRect, dstRect, null)
    }
}
