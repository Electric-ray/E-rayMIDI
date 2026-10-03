package com.example.nukedsc55

import android.graphics.Bitmap
import android.os.Handler

/**
 * 엔진이 합성해 주는 LCD 프레임(크기가 기종마다 달라짐)을 [LcdView]에 공급한다.
 *
 * MainActivity의 SC-55용 트리플 버퍼 로직을 엔진 공통으로 일반화한 것이다.
 *  - 크기가 바뀌면(기종 전환) 비트맵 3장을 새로 만든다.
 *  - [seqProvider]가 같은 값이면 화면 내용이 같다는 뜻이므로 그리기를 건너뛴다.
 *  - 지금 보여지고 있거나 onDraw() 중인 비트맵에는 절대 쓰지 않는다 (LcdView 참고).
 *
 * 모든 메서드는 [handler]의 스레드(LCD 전용 백그라운드)에서만 호출한다.
 */
class LcdFramePump(
    private val view: LcdView,
    private val handler: Handler,
    private val isRunning: () -> Boolean,
    private val sizeProvider: () -> Int,      // (w shl 16) or h, 0 = LCD 없음
    private val seqProvider: () -> Long,
    private val fill: (Bitmap) -> Boolean,
    private val intervalMs: Long = 33L        // ~30fps: 레벨미터가 부드럽게 움직이도록
) {
    private val bitmaps = arrayOfNulls<Bitmap>(3)
    private var bmpW = 0
    private var bmpH = 0
    private var writeIdx = 0
    private var lastSeq = -1L
    @Volatile private var active = false

    private val tick = object : Runnable {
        override fun run() {
            if (!active) return
            renderOnce()
            if (active) handler.postDelayed(this, intervalMs)
        }
    }

    fun start() {
        if (active) return
        active = true
        lastSeq = -1L
        handler.post(tick)
    }

    fun stop() {
        active = false
        handler.removeCallbacks(tick)
    }

    private fun renderOnce() {
        if (!isRunning()) return
        val size = sizeProvider()
        if (size == 0) return
        val w = size ushr 16
        val h = size and 0xFFFF
        if (w <= 0 || h <= 0) return

        if (w != bmpW || h != bmpH || bitmaps[0] == null) {
            // 이전 비트맵은 recycle하지 않는다 — onDraw()가 아직 읽고 있을 수 있다(GC가 정리).
            for (i in bitmaps.indices) bitmaps[i] = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
            bmpW = w; bmpH = h; writeIdx = 0
            lastSeq = -1L
        }

        val seq = seqProvider()
        if (seq == lastSeq) return   // 화면 내용 변화 없음

        var target: Bitmap? = null
        for (attempt in bitmaps.indices) {
            val candidate = bitmaps[writeIdx] ?: return
            val unsafe = candidate === view.getActiveBitmap() || view.isBeingDrawn(candidate)
            if (!unsafe) { target = candidate; break }
            writeIdx = (writeIdx + 1) % bitmaps.size
        }
        if (target == null) return   // 셋 다 사용 중 — 이번 프레임은 건너뜀

        if (!fill(target)) return
        lastSeq = seq
        view.setFrame(target)
        writeIdx = (writeIdx + 1) % bitmaps.size
        view.postInvalidate()
    }
}
