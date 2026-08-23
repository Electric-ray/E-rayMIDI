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

    // BUGFIX (버그 제보 대응 — "LCD 화면이 여러 개 겹쳐 보인다"/체크무늬 노이즈):
    // 원래는 백그라운드 스레드가 "타이머상 N프레임(버퍼 개수만큼) 지났으니 안전하겠지"
    // 라는 추측만으로 비트맵을 재사용해 픽셀을 덮어썼다. 빠른 기기에서는 onDraw()가
    // 항상 그 안에 끝나서 문제가 안 보였지만, 느린 기기(구형 Android 10 기기,
  // Galaxy A50 등)에서 onDraw()가 그 여유 시간보다 오래 걸리면, 화면에 그려지고
    // 있는 바로 그 비트맵의 픽셀 메모리를 네이티브 스레드가 동시에 write하게 되어
    // 절반은 이전 프레임 절반은 새 프레임이 섞인 "찢어진" 이미지(체크무늬/겹침처럼
    // 보임)가 나왔다. 진짜 동기화로 고친다: onDraw()가 지금 실제로 읽고 있는 Bitmap
    // 인스턴스를 별도로 추적해서, 그 인스턴스에는 렌더 스레드가 절대 쓰지 못하게 한다.
    @Volatile
    private var drawingBitmap: Bitmap? = null

    /** 백그라운드 스레드에서 완성된 프레임을 세팅. UI 스레드에서 invalidate 필요. */
    fun setFrame(bitmap: Bitmap) {
        currentBitmap = bitmap
    }

    /**
     * 지금 이 View에 세팅되어 다음 onDraw()에서 그려질 예정인 Bitmap 인스턴스.
     * 렌더 스레드가 "이 인스턴스는 아직 쓰면 안 됨"을 판단하는 데 씀.
     */
    fun getActiveBitmap(): Bitmap? = currentBitmap

    /**
     * 지금 이 순간 onDraw()가 실제로 픽셀을 읽고 있는 중인 Bitmap 인스턴스.
     * currentBitmap이 이미 다음 프레임으로 넘어갔더라도, 느린 기기에서는
     * 이전 프레임을 그리던 onDraw()가 아직 끝나지 않았을 수 있어서 별도로 필요.
     */
    fun isBeingDrawn(bitmap: Bitmap): Boolean = drawingBitmap === bitmap

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val bmp = currentBitmap ?: return
        drawingBitmap = bmp
        try {
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
        } finally {
            // onDraw()가 이 인스턴스의 픽셀을 다 읽었으니(Canvas.drawBitmap은 동기
            // 호출 — 리턴 시점엔 CPU 쪽 픽셀 읽기가 끝나있음), 이제부터 렌더
            // 스레드가 이 인스턴스를 다시 써도 안전하다.
            drawingBitmap = null
        }
    }
}
