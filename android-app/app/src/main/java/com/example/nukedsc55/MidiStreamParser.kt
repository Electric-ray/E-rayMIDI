package com.example.nukedsc55

/**
 * ?곗냽 MIDI 諛붿씠???ㅽ듃由????꾩꽦??硫붿떆吏 肄쒕갚
 * Running Status 吏?? SysEx 吏??(F0..F7)
 * USB-serial 38400bps 濡??ㅼ뼱?ㅻ뒗 raw MIDI 諛붿씠?몄슜
 * (munt-android?먯꽌 洹몃?濡??ы똿 ??寃利앸맂 援ы쁽)
 */
class MidiStreamParser(private val onMessage: (ByteArray) -> Unit) {

    private var runningStatus = 0
    private val buf = ByteArray(512)
    private var bufPos = 0
    private var expected = 0
    private var inSysEx = false

    fun reset() { runningStatus = 0; bufPos = 0; expected = 0; inSysEx = false }

    fun feed(data: ByteArray) {
        for (b in data) feedByte(b.toInt() and 0xFF)
    }

    private fun feedByte(b: Int) {
        if (b >= 0xF8) { onMessage(byteArrayOf(b.toByte())); return }

        if (b == 0xF7) {
            if (inSysEx && bufPos > 0) {
                buf[bufPos++] = 0xF7.toByte()
                onMessage(buf.copyOf(bufPos))
            }
            inSysEx = false; bufPos = 0; expected = 0
            return
        }

        if (b == 0xF0) {
            inSysEx = true; bufPos = 0; expected = 0
            buf[bufPos++] = 0xF0.toByte()
            return
        }

        if (inSysEx) {
            if (bufPos < buf.size) buf[bufPos++] = b.toByte()
            return
        }

        if (b and 0x80 != 0) {
            // MIDI spec: only 0x80-0xEF (Channel Voice/Mode) may be stored
            // as Running Status. 0xF1-0xF6 (System Common) must CLEAR it.
            runningStatus = if (b < 0xF0) b else 0
            bufPos = 0
            buf[bufPos++] = b.toByte()
            expected = messageLength(b)
            if (expected == 1) { emit(); return }
            return
        }

        if (runningStatus == 0) return
        if (bufPos == 0) {
            buf[bufPos++] = runningStatus.toByte()
            expected = messageLength(runningStatus)
        }
        if (bufPos < buf.size) buf[bufPos++] = b.toByte()
        if (bufPos >= expected) emit()
    }

    private fun emit() {
        onMessage(buf.copyOf(bufPos))
        bufPos = 0
    }

    private fun messageLength(status: Int): Int = when (status and 0xF0) {
        0x80, 0x90, 0xA0, 0xB0, 0xE0 -> 3
        0xC0, 0xD0                    -> 2
        0xF0 -> when (status) { 0xF2 -> 3; 0xF1, 0xF3 -> 2; else -> 1 }
        else -> 1
    }
}
