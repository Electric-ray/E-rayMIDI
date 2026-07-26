package com.example.nukedsc55

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.os.SystemClock
import android.util.Log
import java.net.*
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.concurrent.thread
import kotlin.random.Random

/**
 * AppleMIDI 클라이언트 — 스테이트 머신 버전 v3
 *
 * 수정 사항:
 * 1. [버그수정] ctrlThread 좀비 제거: interrupt() 대신 소켓 close + 재생성
 * 2. [버그수정] Android(Initiator)가 dataOk 후 CK0을 먼저 전송
 * 3. [버그수정] 60초 keepalive CK0 타이머 추가
 * 4. [개선] remoteSSRC 저장 및 활용
 * 5. [버그수정 v3] CK 패킷 구조 전면 수정 (50초 BY 끊김 근본 원인):
 *    - CK 패킷에 version 필드 없음 (IN/OK 패킷에만 있음) → 4바이트 밀림 버그 수정
 *    - 모든 CK 패킷 36바이트 고정 (lathoub/mt32-pi: sizeof(TAppleMIDISync)=36)
 *    - CK0: 24→36바이트, CK1: 32→36바이트, CK2: 40→36바이트
 *    - 타임스탬프 단위 100ns→100μs (Apple MIDI 스펙 준수)
 *    - buildCkPacket() 공통 빌더 함수 추출
 */
class RtpMidiSession(
    private val ctx: Context,
    private val onMidiMessage: (ByteArray) -> Unit,
    private val onStatus: (String) -> Unit = {},
    private val onAllNotesOff: (() -> Unit)? = null  // stuck note 방지
) {
    companion object {
        private const val TAG = "RtpMidi"
        private val MAGIC = byteArrayOf(0xFF.toByte(), 0xFF.toByte())
        private const val CMD_IN: Short = 0x494E.toShort()
        private const val CMD_OK: Short = 0x4F4B.toShort()
        private const val CMD_CK: Short = 0x434B.toShort()
        private const val CMD_BY: Short = 0x4259.toShort()
        private const val CMD_RS: Short = 0x5253.toShort()  // ReceiverFeedback (mt32-pi와 동일)
        private const val HOST = "192.168.4.1"
        private const val BASE = 5004
        private const val CK_KEEPALIVE_MS = 45_000L
    }

    private val ssrc  = Random.nextInt().toLong() and 0xFFFFFFFFL
    private val token = Random.nextInt().toLong() and 0xFFFFFFFFL

    @Volatile private var running = false
    @Volatile private var remoteSSRC = 0L   // OK 패킷에서 저장

    // 소켓은 재연결 시마다 재생성 (좀비 스레드 문제 해결)
    @Volatile private var ctrlSock: DatagramSocket? = null
    @Volatile private var dataSock: DatagramSocket? = null

    private var multicastLock: WifiManager.MulticastLock? = null
    private var wifiLock: WifiManager.WifiLock? = null  // ★ WiFi 절전 방지
    private var regListener: NsdManager.RegistrationListener? = null
    private var nsdMgr: NsdManager? = null

    // 크로스패킷 SysEx 누적기
    private val sysexBuf = mutableListOf<Byte>()
    private var inSysex = false

    // ★ RS(ReceiverFeedback) 전송용 — mt32-pi: 1초마다 ctrl 포트로 전송
    private var lastRsMs       = 0L
    private var lastRxSeq      = 0
    private var lastSentSeq    = -1
    private var sysexCount     = 0
    private var lastAndroidCk0Ms = 0L   // CK0 10초 keepalive 타이머

    // ★ RTP 시퀀스 번호 추적
    // SSRC별로 관리 → 재연결 시 이전/현재 세션 교차 오탐지 방지
    private val seqBySSRC = mutableMapOf<Int, Int>()
    var byWasIdle = false  // BY 수신 시 idle에 의한 것이면 true → 빠른 재연결
    @Volatile var activeNetwork: android.net.Network? = null  // WiFi 소켓 바인딩용
    // FIX (레벨미터 애니메이션 완전 불능 근본 원인): ESP32가 서로 다른 SSRC로 구성된
    // 두 개의 병렬 스트림으로 동일한 MIDI를 이중 전송한다. 데이터 소켓에서 첫 번째로 본 SSRC만
    // 그 세션의 "진짜"로 고정하고, 다른 SSRC(중복 스트림)에서 온 패킷은 통째로 무시한다.
    // 핸드쉘이크 응답의 remoteSSRC에 의존하지 않는 이유: 그것이 실제 데이터 스트림의 SSRC와
    // 일치한다는 보장이 없기 때문.
    @Volatile private var lockedDataSsrc: Long? = null
    // FIX (DT1 체크섬 에러/애니메이션 간헐적 실패, ESP32 리셋 후도 재현됨):
    // 이중 전송이 항상 다른 SSRC를 쓰는 것이 아니라, 같은 SSRC 안에서 seq만 따로
    // 증가하는 두 개의 인터리빙 스트림으로 오는 경우도 있음 (seq 값이 다르므로
    // 기존 seq 기반 중복 필터를 통과해버림 → 여러 패킷에 걸친 SysEx가 두 사본이 섞여
    // 여전히 간헐적으로 깨짐). 내용(payload byte) 자체가 방금 들어온 것과 또같고 짧은
    // 시간(200ms) 안에 다시 오면 중복 전송으로 보고 무시한다.
    private var lastPayloadHash = 0
    private var lastPayloadTimeMs = 0L

    fun start(net: android.net.Network? = null) {
        running = true
        val wm = ctx.getSystemService(Context.WIFI_SERVICE) as WifiManager
        multicastLock = wm.createMulticastLock("nukedsc55").also { it.acquire() }

        // ★ WiFi 라디오 절전 완전 비활성화
        // Android 절전 시 UDP 패킷이 간헐적으로 손실 → CK 응답 실패 → ESP32 BY 전송
        // PC는 항상 고성능 WiFi 모드 → 패킷 손실 없음 → 세션 유지
        //
        // FIX (패킷 손실, 거리와 무관하게 지속): WIFI_MODE_FULL_HIGH_PERF는 API 29
        // (Android 10)부터 공식 deprecated되었고, Android 문서에 "이 API는 비기능
        // 적일 수 있으므로 WIFI_MODE_FULL_LOW_LATENCY를 대신 쓰라"고 명시되어 있다.
        // 즉 지금까지는 "절전 방지를 시도"만 했을 뿐 실제로는 안 걸렸을 가능성이 높다.
        // API 29+ 에서는 실시간 스트리밍/VoIP용으로 설계된 LOW_LATENCY를 쓴다.
        val wifiLockMode = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            WifiManager.WIFI_MODE_FULL_LOW_LATENCY
        } else {
            @Suppress("DEPRECATION") WifiManager.WIFI_MODE_FULL_HIGH_PERF
        }
        wifiLock = wm.createWifiLock(wifiLockMode, "sc55_rtp")
            .also { it.acquire() }
        Log.i(TAG, "WifiLock 모드=$wifiLockMode (LOW_LATENCY=${WifiManager.WIFI_MODE_FULL_LOW_LATENCY}) 획득=${wifiLock?.isHeld}")

        thread(name = "rtpmidi", isDaemon = true) {
            // FIX (패킷 손실): 이 스레드가 CPU를 늘게 높넣 받지 못해 스케줄링이 밀리면,
            // 그 사이 들어온 UDP 패킷이 컮널 수신 버퍼에서 넘쳐서 버려질 수 있다.
            // 오디오 스레드와 동일한 URGENT_AUDIO 우선순위로 올려서 LCD/MCU 스레드와의
            // 경쟁에서 덜 밀리게 한다.
            runCatching {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO)
            }
            sessionLoop()
        }
    }

    fun updateNetwork(net: android.net.Network?) {
        activeNetwork = net
    }

    fun stop() {
        running = false
        // FIX (모드 전환 시 "제어 타임아웃" 재현): 이전에는 소켓만 close()하고 ESP32에게 BY(세션종료)를 보내지 않았다.
        // ESP32(lathoub AppleMIDI)는 그러면 자신이 여전히 이전 세션에 연결되어 있다고 보고,
        // 자체 타임아웃(수십초)이 지나기 전까지는 새 IN(초대) 요청을 받아주지 않아서,
        // 빠른 엔진 재전환 시 "제어 타임아웃"이 나고 연결이 끊기는 것으로 확인됨.
        // 해결: close() 전에 BY를 ctrl/data 포트 둘 다에 보내 ESP32가 즉시 세션을 해제하도록 함.
        ctrlSock?.let { sendBy(it, BASE) }
        dataSock?.let { sendBy(it, BASE + 1) }
        regListener?.let { runCatching { nsdMgr?.unregisterService(it) } }
        multicastLock?.release()
        wifiLock?.release()
        ctrlSock?.close()
        dataSock?.close()
    }

    // ── BY (End Session) 전송 ── sendInvite와 동일한 패킷 구조를 재사용
    // (수신측은 이미 IN 구조를 파싱할 수 있으므로 안전하게 동일 포맷 사용).
    // UDP 손실 대비 3회 전송.
    private fun sendBy(sock: DatagramSocket, remotePort: Int) {
        val bb = ByteBuffer.allocate(16).order(ByteOrder.BIG_ENDIAN)
        bb.put(MAGIC); bb.putShort(CMD_BY); bb.putInt(2)
        bb.putInt(token.toInt()); bb.putInt(ssrc.toInt())
        val pkt = bb.array()
        repeat(3) {
            runCatching {
                sock.send(DatagramPacket(pkt, pkt.size, InetSocketAddress(HOST, remotePort)))
            }
            if (it < 2) Thread.sleep(5)
        }
        Log.i(TAG, "BY×3 전송 (port=$remotePort) — ESP32 세션 즉시 해제 요청")
    }

    // MuntEngine이 BY 이후 재연결을 기다릴 때 사용 (현재 미사용, 향후 확장용)
    // private val endLatch = java.util.concurrent.CountDownLatch(1)
    // fun awaitEnd() { endLatch.await() }

    // ── 세션 스테이트 머신 ────────────────────────────────────────────────
    private fun sessionLoop() {
        // mDNS는 한 번만 등록 (고정 포트 불필요)
        registerMdns(BASE)

        var attempt = 0
        while (running) {
            attempt++
            onStatus("─── 연결 시도 #$attempt ───")

            // ★ 재연결 시마다 소켓 재생성 (이전 좀비 스레드가 쥐고 있던 소켓 버림)
            ctrlSock?.close()
            dataSock?.close()
            val ctrl = bindPort(BASE).also  { ctrlSock = it }
            val data = bindPort(BASE + 1).also { dataSock = it }
            onStatus("소켓 생성: ctrl=${ctrl.localPort} data=${data.localPort}")

            // STEP 1: 제어 핸드셰이크
            sendInvite(ctrl, BASE)
            onStatus("IN → ESP32:$BASE")
            val ctrlOk = waitFor(ctrl, "제어", timeoutMs = 5000) { buf, len, addr, sock ->
                parseSessionPkt(buf, len, addr, sock, "ctrl")
            }
            if (!ctrlOk) { Thread.sleep(500); continue }

            // STEP 2: 데이터 핸드셰이크
            sendInvite(data, BASE + 1)
            onStatus("IN → ESP32:${BASE + 1}")
            val dataOk = waitFor(data, "데이터", timeoutMs = 4000) { buf, len, addr, sock ->
                parseSessionPkt(buf, len, addr, sock, "data")
            }
            if (!dataOk) { Thread.sleep(500); continue }

            // STEP 3: ★ CK0 burst를 백그라운드에서 비동기 전송
            // 핵심 수정: 이전엔 6회 × 1500ms = 7.5초 동안 여기서 blocking →
            //   그 사이 게임이 보내는 MT-32 초기화 SysEx 전부 소실 → 음색 차이
            // 수정: 별도 스레드로 CK0 burst 전송, midiLoop는 즉시 시작
            rs = 0; rtpDumpCount = 0
            lastAndroidCk0Ms = System.currentTimeMillis()
            thread(isDaemon = true, name = "ck0burst") {
                repeat(6) { i ->
                    if (!running) return@thread
                    sendCk0(data)
                    onStatus("⚡ CK0 버스트 ${i+1}/6 (bg)")
                    if (i < 5) Thread.sleep(1500)
                }
                Log.d(TAG, "CK0 burst 완료")
            }
            // ★ sendMt32Reset 제거: 게임이 이미 보낸 패치를 덮어써서 음색 망가짐
            //   (mt32emu 리셋 → 게임 재전송 없음 → 기본 음색으로 재생됨)

            // STEP 4: MIDI 수신 루프
            onStatus("🎵 MIDI 수신 루프 시작")
            val byReceived = midiLoop(ctrl, data)

            if (byReceived) {
                onStatus("⚠️ BY 수신 — All Notes Off 전송 후 2초 대기")
                // ★ MT-32 채널(ch1-8, ch10)에 All Notes Off + All Sound Off
                // BY 후 노트가 끊기지 않고 울리면 MT-32 폴리 꽉 차서 새 MIDI 안 들어옴
                for (ch in 0 until 16) {
                    val st = (0xB0 or ch).toByte()
                    onMidiMessage(byteArrayOf(st, 123, 0)) // CC#123 All Notes Off
                    onMidiMessage(byteArrayOf(st, 120, 0)) // CC#120 All Sound Off
                }
                inSysex = false; sysexBuf.clear()
                rs = 0  // 러닝 스테이터스 초기화
                Thread.sleep(2000)
            }
        }
    }

    // ── MIDI 수신 루프 ────────────────────────────────────────────────────
    private fun midiLoop(ctrl: DatagramSocket, data: DatagramSocket): Boolean {
        val buf = ByteArray(1500)
        var rtpCount = 0L
        var byReceived = false

        // ★ ctrlThread: 소켓 close로만 종료 가능하게 설계
        //    (interrupt()는 DatagramSocket.receive()를 깨우지 못함)
        var ctrlThreadRunning = true
        val ctrlThread = thread(isDaemon = true, name = "rtpmidi-ctrl") {
            val b = ByteArray(256)
            while (running && ctrlThreadRunning) {
                try {
                    ctrl.soTimeout = 5000
                    val p = DatagramPacket(b, b.size)
                    ctrl.receive(p)
                    if (p.length >= 4 &&
                        b[0] == 0xFF.toByte() && b[1] == 0xFF.toByte()) {
                        val cmdInt = ((b[2].toInt() and 0xFF) shl 8) or (b[3].toInt() and 0xFF)
                        when (cmdInt) {
                            (CMD_BY.toInt() and 0xFFFF) -> {
                                Log.w(TAG, "⚠️ BY ctrl 수신 (세션 종료 신호)")
                                onStatus("BY ctrl"); byReceived = true
                            }
                            (CMD_CK.toInt() and 0xFFFF) -> { handleCk(b, p.length, p.socketAddress, ctrl) }
                        }
                    }
                } catch (_: SocketTimeoutException) {}
                catch (e: Exception) {
                    // EBADF/SocketClosed = 재연결 로직이 소켓 닫은 것 → 정상 종료
                    // running=true일 때만 예기치 않은 에러로 로그
                    if (ctrlThreadRunning && running &&
                        e.message?.contains("EBADF") == false &&
                        e.message?.contains("closed") == false)
                        Log.e(TAG, "제어: $e")
                    else if (ctrlThreadRunning)
                        Log.d(TAG, "제어 소켓 종료 (정상): ${e.javaClass.simpleName}")
                    break
                }
            }
        }

        // keepalive 타이머: 45초마다 CK0 재전송
        var lastCkTime = System.currentTimeMillis()
        var lastMidiRxMs = System.currentTimeMillis()
        var silentTimeouts = 0  // 연속 타임아웃 횟수 (10초씩 누적)

        seqBySSRC.clear(); lastRsMs = System.currentTimeMillis()
        lastSentSeq = -1; sysexCount = 0
        lockedDataSsrc = null  // 새 세션에서 다시 처음 본 SSRC를 고정하도록 리셋

        while (running && !byReceived) {
            try {
                data.soTimeout = 10000
                val pkt = DatagramPacket(buf, buf.size)
                data.receive(pkt); val len = pkt.length
                silentTimeouts = 0  // 패킷 수신 시 카운터 리셋

                if (len >= 2 && buf[0] == 0xFF.toByte() && buf[1] == 0xFF.toByte()) {
                    val bb = ByteBuffer.wrap(buf, 0, len).order(ByteOrder.BIG_ENDIAN)
                    bb.short
                    when (bb.short) {
                        CMD_CK -> handleCk(buf, len, pkt.socketAddress, data)
                        CMD_BY -> {
                            val idleMs = System.currentTimeMillis() - lastMidiRxMs
                            Log.w(TAG, "⚠️ BY data 수신 (idle=${idleMs/1000}s, rtp수신=$rtpCount)")
                            onStatus("⚠️ BY (idle=${idleMs/1000}s)")
                            byReceived = true
                            byWasIdle = (idleMs > 30_000L)
                        }
                        CMD_OK -> onStatus("OK 재수신 (무시)")
                        else   -> {}
                    }
                } else if (len >= 12 && (buf[0].toInt() and 0xC0) == 0x80) {
                    if (++rtpCount == 1L) onStatus("📦 첫 MIDI RTP 수신!")
                    lastMidiRxMs = System.currentTimeMillis()
                    parseRtpMidi(buf, len)
                }

                val nowMs = System.currentTimeMillis()
                if (nowMs - lastAndroidCk0Ms >= 8_000L) {
                    sendCk0(data); lastAndroidCk0Ms = nowMs; onStatus("CK0 8s")
                }
                if (nowMs - lastRsMs >= 1_000L && lastRxSeq != lastSentSeq) {
                    sendRS(ctrl, lastRxSeq); lastSentSeq = lastRxSeq; lastRsMs = nowMs
                }
            } catch (_: SocketTimeoutException) {
                silentTimeouts++
                val nowMs = System.currentTimeMillis()

                // 타임아웃 중에도 CK0 유지 (패킷 없어도 세션 keepalive)
                if (nowMs - lastAndroidCk0Ms >= 8_000L) {
                    sendCk0(data); lastAndroidCk0Ms = nowMs
                }

                // ★ 30초(3회×10초) 무신호 → All Notes Off
                // 10초: 메뉴·로딩 중 오인식 위험 → 30초로 보수적으로 설정
                // NoteOff UDP 손실로 음이 계속 지속되는 stuck note 방지
                if (silentTimeouts >= 3) {
                    onStatus("무신호 30초 → All Notes Off")
                    onAllNotesOff?.invoke()
                    silentTimeouts = 0
                } else {
                    onStatus("무신호 ${silentTimeouts * 10}초 (rtp=$rtpCount)")
                }
            } catch (e: Exception) {
                if (running) Log.e(TAG, "midi: $e"); break
            }
        }

        // ★ ctrlThread 종료: close()로 receive() 블로킹 해제
        ctrlThreadRunning = false
        ctrl.close()  // → ctrlThread의 receive()가 SocketException으로 즉시 탈출
        return byReceived
    }

    // ── 100μs 단위 타임스탬프 (Apple MIDI 스펙 단위) ──────────────────────
    // System.nanoTime() / 100_000 = 100마이크로초 단위
    // (이전 코드의 / 100 는 100나노초 단위였음 — 1000배 오류)
    private fun syncClock(): Long = SystemClock.elapsedRealtimeNanos() / 100_000L

    // ── CK 패킷 빌더 (36바이트 고정) ─────────────────────────────────────
    // Apple MIDI / lathoub / mt32-pi 모두 struct TAppleMIDISync = 36bytes 고정
    // struct: FF FF(2) + CK(2) + SSRC(4) + count(1) + padding(3) + ts0(8) + ts1(8) + ts2(8)
    // ※ CK 패킷에는 version 필드가 없음! (IN/OK 패킷에만 있음)
    private fun buildCkPacket(count: Int, ts0: Long, ts1: Long, ts2: Long): ByteArray {
        val r = ByteBuffer.allocate(36).order(ByteOrder.BIG_ENDIAN)
        r.put(MAGIC)
        r.putShort(CMD_CK)
        r.putInt(ssrc.toInt())                    // 자신의 SSRC
        r.put(count.toByte())                     // count: 0/1/2
        r.put(0); r.put(0); r.put(0)              // padding (3바이트)
        r.putLong(ts0); r.putLong(ts1); r.putLong(ts2)  // ts 슬롯 3개 = 24바이트
        return r.array()
    }

    // ── CK0 전송 (Initiator 역할) ─────────────────────────────────────────
    private fun sendCk0(sock: DatagramSocket) {
        val pkt = buildCkPacket(0, syncClock(), 0L, 0L)
        runCatching {
            sock.send(DatagramPacket(pkt, 36, InetSocketAddress(HOST, BASE + 1)))
        }
    }

    // ── CK 수신 처리 ─────────────────────────────────────────────────────
    // count=0(CK0): Responder로서 CK1 응답
    // count=1(CK1): Initiator로서 CK2 응답 (CK0를 우리가 보낸 경우)
    // count=2(CK2): 수신만 (교환 완료)
    private fun handleCk(buf: ByteArray, len: Int, addr: SocketAddress, sock: DatagramSocket) {
        // CK 패킷 최소 크기 = 36바이트 (struct TAppleMIDISync 고정 크기)
        // lathoub / mt32-pi 모두 sizeof(TAppleMIDISync) < len 이면 즉시 버림
        if (len < 36) {
            Log.w(TAG, "handleCk: 패킷 너무 짧음 len=$len (필요 36)")
            return
        }
        val bb = ByteBuffer.wrap(buf, 0, len).order(ByteOrder.BIG_ENDIAN)
        bb.short; bb.short                  // FF FF, "CK" 소비
        bb.getInt()                         // sender SSRC (CK엔 version 없음!)
        val count = bb.get().toInt() and 0xFF
        bb.get(); bb.get(); bb.get()        // padding 3바이트
        // 여기서 bb.position() = 12, remaining = ts0(8)+ts1(8)+ts2(8) = 24바이트

        when (count) {
            0 -> {
                // CK0 수신 (ESP32 → Android) → CK1 응답
                val ts0 = bb.getLong()           // ts0 echo
                val pkt = buildCkPacket(1, ts0, syncClock(), 0L)
                // ★ CK1 3회 재전송: WiFi 패킷 손실 대비 (50% 손실→87.5% 성공)
                repeat(3) {
                    runCatching { sock.send(DatagramPacket(pkt, 36, addr)) }
                    if (it < 2) Thread.sleep(10)
                }
                onStatus("CK1x3 (Responder) addr=$addr")
            }
            1 -> {
                // CK1 수신 (ESP32가 Android CK0에 응답) → CK2 3회 전송
                // ★ 3회 전송: WiFi 손실로 CK2 미도달 시 ESP32가 sync 실패 카운트
                //   5회 실패 × 10초 = 50초 후 BY → 1분마다 재연결 루프
                //   CK2를 3회 보내면 3연속 패킷 손실 없으면 반드시 도달
                val ts0 = bb.getLong()
                val ts1 = bb.getLong()
                val pkt = buildCkPacket(2, ts0, ts1, syncClock())
                repeat(3) {
                    runCatching { sock.send(DatagramPacket(pkt, 36, addr)) }
                    if (it < 2) Thread.sleep(5)
                }
                onStatus("⚡ CK2×3 전송 (Initiator)")
                Log.d(TAG, "CK1→CK2×3 완료")
            }
            2 -> {
                // CK2 수신 (교환 완료, Responder 역할)
                Log.d(TAG, "CK2 수신 (교환 완료)")
                onStatus("CK2 수신 ✓")
            }
            else -> Log.w(TAG, "handleCk: 알 수 없는 count=$count")
        }
    }

    // ── 소켓 수신 + 조건 대기 ────────────────────────────────────────────
    private fun waitFor(
        sock: DatagramSocket, label: String, timeoutMs: Int,
        handler: (ByteArray, Int, SocketAddress, DatagramSocket) -> Boolean
    ): Boolean {
        val buf = ByteArray(512)
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline && running) {
            val rem = (deadline - System.currentTimeMillis()).coerceAtLeast(1)
            sock.soTimeout = rem.toInt()
            try {
                val pkt = DatagramPacket(buf, buf.size)
                sock.receive(pkt)
                if (handler(buf, pkt.length, pkt.socketAddress, sock)) return true
            } catch (_: SocketTimeoutException) {}
            catch (e: Exception) { Log.e(TAG, "$label: $e"); break }
        }
        onStatus("⚠️ $label 타임아웃")
        return false
    }

    // ── 세션 패킷 처리 → OK이면 true ─────────────────────────────────────
    private fun parseSessionPkt(buf: ByteArray, len: Int,
                                addr: SocketAddress, sock: DatagramSocket,
                                src: String): Boolean {
        if (len < 4) return false
        val bb = ByteBuffer.wrap(buf, 0, len).order(ByteOrder.BIG_ENDIAN)
        val magic = bb.short; val cmd = bb.short
        if (magic != 0xFFFF.toShort()) return false
        return when (cmd) {
            CMD_OK -> {
                if (bb.remaining() < 12) return false
                bb.getInt()  // version
                bb.getInt()  // initiator token (echo)
                val rssrc = if (bb.remaining() >= 4) bb.getInt().toLong() and 0xFFFFFFFFL else 0L
                remoteSSRC = rssrc  // ★ 저장
                val ia = addr as InetSocketAddress
                onStatus("✅ OK[$src] ${ia.address.hostAddress}:${ia.port} rSSRC=$rssrc")
                true
            }
            CMD_IN -> {
                // ESP32가 먼저 IN을 보내는 경우 (역방향 invite)
                if (bb.remaining() < 8) return false
                val ver = bb.getInt(); val tok = bb.getInt()
                onStatus("ESP32→IN 수신[$src], OK 응답")
                sendOk(sock, ver, tok, addr)
                true
            }
            CMD_CK -> {
                // 핸드셰이크 중 CK0 선수신 → 처리하고 계속 OK 대기
                handleCk(buf, len, addr, sock)
                false
            }
            CMD_BY -> { onStatus("⚠️ BY 수신 (핸드셰이크 중)"); false }
            else -> false
        }
    }

    private fun sendOk(sock: DatagramSocket, ver: Int, tok: Int, addr: SocketAddress) {
        val r = ByteBuffer.allocate(16).order(ByteOrder.BIG_ENDIAN)
        r.put(MAGIC); r.putShort(CMD_OK)
        r.putInt(ver); r.putInt(tok); r.putInt(ssrc.toInt())
        runCatching { sock.send(DatagramPacket(r.array(), 16, addr)) }
    }

    // ── RS (ReceiverFeedback) 전송 ────────────────────────────────────────
    // mt32-pi: 1초마다 ctrl 포트로 전송 (마지막 수신 RTP 시퀀스 번호 보고)
    // 구조: FF FF(2) + "RS"(2) + SSRC(4) + lastSeq(4) = 12바이트
    private fun sendRS(ctrl: DatagramSocket, seq: Int) {
        val r = ByteBuffer.allocate(12).order(ByteOrder.BIG_ENDIAN)
        r.put(MAGIC); r.putShort(CMD_RS)
        r.putInt(ssrc.toInt())
        r.putInt(seq)
        runCatching {
            ctrl.send(DatagramPacket(r.array(), 12, InetSocketAddress(HOST, BASE)))
        }
    }

    private fun sendInvite(sock: DatagramSocket, remotePort: Int) {
        val name = "Android\u0000".toByteArray(Charsets.US_ASCII)
        val bb = ByteBuffer.allocate(16 + name.size).order(ByteOrder.BIG_ENDIAN)
        bb.put(MAGIC); bb.putShort(CMD_IN); bb.putInt(2)
        bb.putInt(token.toInt()); bb.putInt(ssrc.toInt()); bb.put(name)
        runCatching {
            sock.send(DatagramPacket(bb.array(), bb.position(),
                InetSocketAddress(HOST, remotePort)))
        }
    }

    private fun bindPort(port: Int): DatagramSocket {
        val sock = try { DatagramSocket(port) }
            catch (_: Exception) { onStatus("포트$port 사용중→랜덤"); DatagramSocket() }
        // FIX (패킷 손실): 기본 수신 버퍼가 작으면, 앱 스레드가 잠깜 다른 일(GC,
        // 다른 스레드에 밀림 등)을 하는 사이 들어온 UDP 패킷이 커널 단에서 조용히
        // 버려진다 (진짜 전파 손실이 아니라 로컬 버퍼 오버플로우). PC는 보통 훨씬
        // 크고 여유로운 버퍼를 쓰고 스케줄링도 덜 밀려서 이 손실이 안 보였을 가능성이 큼.
        runCatching {
            sock.receiveBufferSize = 262144  // 256KB 요청 (OS가 상한선을 둘 수 있음)
            Log.i(TAG, "소켓 포트=$port 수신버퍼=${sock.receiveBufferSize}B")
        }
        return sock
    }

    private fun registerMdns(port: Int) {
        val nm = ctx.getSystemService(Context.NSD_SERVICE) as NsdManager
        nsdMgr = nm
        val listener = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(i: NsdServiceInfo) { onStatus("📡 mDNS: ${i.serviceName}") }
            override fun onRegistrationFailed(i: NsdServiceInfo, c: Int) {}
            override fun onServiceUnregistered(i: NsdServiceInfo) {}
            override fun onUnregistrationFailed(i: NsdServiceInfo, c: Int) {}
        }
        regListener = listener
        nm.registerService(NsdServiceInfo().apply {
            serviceName = "NukedSC55Android"; serviceType = "_apple-midi._udp."; setPort(port)
        }, NsdManager.PROTOCOL_DNS_SD, listener)
    }

    // ── SysEx 누적기 ─────────────────────────────────────────────────────
    private fun sysexFeed(bytes: ByteArray) {
        for (b in bytes) {
            val bi = b.toInt() and 0xFF
            when {
                bi == 0xF0 -> { sysexBuf.clear(); sysexBuf.add(b); inSysex = true }
                bi == 0xF7 && inSysex -> {
                    sysexBuf.add(b)
                    // FIX (DT1 체크섬 에러 화면 표시 억제): ESP32/lathoub 자체가 드물게 패킷 경계에서
                    // 바이트를 하나 빠뜨리는 경우가 있음 (우리 쪽 파싱 버그가 아니라 송신 측 자체 문제로
                    // 확인됨). 체크섬이 안 맞는 DT1(Roland cmd=0x12) 메시지를 그대로 엔진에 넘기면
                    // 실제 SC-55 펌웨어가 "DT1 DATA ERROR"를 화면에 표시하는 진짜 하드웨어 동작이 그대로
                    // 재현된다. 우리가 수정할 수 없는 송신측 손실이므로, 차라리 체크섬을 미리 검증해서
                    // 안 맞으면 엔진에 전달하지 않고 조용히 버린다 (에러 텍스트도 안 띄고, 그 프레임만 이전 상태를
                    // 유지한 채 넘어간다).
                    val isRoland = sysexBuf.size >= 6 && (sysexBuf[1].toInt() and 0xFF) == 0x41
                    val isDT1 = isRoland && (sysexBuf[4].toInt() and 0xFF) == 0x12
                    var checksumOk = true
                    if (isDT1 && sysexBuf.size >= 8) {
                        var s = 0
                        for (i in 5 until sysexBuf.size - 2) s += (sysexBuf[i].toInt() and 0xFF)
                        val expected = (128 - (s % 128)) % 128
                        val actual = sysexBuf[sysexBuf.size - 2].toInt() and 0xFF
                        checksumOk = (expected == actual)
                    }
                    if (!checksumOk) {
                        Log.w(TAG, "⚠️ DT1 체크섬 불일치(${sysexBuf.size}B) — 송신측 손실로 보고 엔진에 전달하지 않음")
                    } else {
                        val fullHex = sysexBuf.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
                        Log.i(TAG, "SysEx완성#${++sysexCount} [${sysexBuf.size}B] $fullHex")
                        onMidiMessage(sysexBuf.toByteArray())
                    }
                    sysexBuf.clear(); inSysex = false
                }
                bi >= 0x80 && inSysex -> {
                    val fullHex = sysexBuf.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
                    Log.w(TAG, "⚠️ SysEx중단: 0x%02X 직전까지 누적(${sysexBuf.size}B): $fullHex".format(bi))
                    sysexBuf.clear(); inSysex = false
                }
                inSysex -> sysexBuf.add(b)
            }
        }
    }

    // ── RFC 6295 RTP-MIDI 파서 ────────────────────────────────────────────
    private var rtpDumpCount = 0

    private fun parseRtpMidi(buf: ByteArray, len: Int) {
        if (len < 13) return
        val bb = ByteBuffer.wrap(buf, 0, len).order(ByteOrder.BIG_ENDIAN)
        val b0 = bb.get().toInt() and 0xFF; bb.get()
        if ((b0 ushr 6) != 2) return
        val seq  = bb.getShort().toInt() and 0xFFFF
        bb.getInt()  // timestamp
        val ssrc = bb.getInt()

        // FIX: 이 세션에서 데이터 소켓으로 처음 본 SSRC를 고정하고, 다른 SSRC(중복
        // 스트림)는 통째로 무시한다. 여러 패킷에 걸친 SysEx는 두 스트림이 섮이면
        // sysexBuf/inSysex 공유 상태가 망가져서 매번 깨졌다 (근본 원인 확인됨).
        val ssrcLong = ssrc.toLong() and 0xFFFFFFFFL
        if (lockedDataSsrc == null) {
            lockedDataSsrc = ssrcLong
            Log.i(TAG, "데이터 SSRC 고정: ${ssrcLong.toString(16)}")
        } else if (lockedDataSsrc != ssrcLong) {
            return  // 중복 스트림 패킷 — 무시
        }

        // ★ SSRC별 시퀀스 갭 추적 + 중복 패킷 필터
        // ESP32가 MIDI 이벤트를 2회 전송하므로 동일 시퀀스가 두 번 올 수 있음
        // 같은 SSRC의 동일 seq = 중복 → MIDI 처리 건너뜀 (mt32emu 이중 재생 방지)
        val prevForSSRC = seqBySSRC[ssrc]
        val isDuplicate = (prevForSSRC == seq)
        if (!isDuplicate) {
            if (prevForSSRC != null) {
                val gap = (seq - prevForSSRC + 0x10000) and 0xFFFF
                if (gap in 2..100)
                    Log.w(TAG, "⚠️ RTP 손실(SSRC=${ssrc.and(0xFFFFFFF).toString(16)}): seq $prevForSSRC→$seq (${gap-1}패킷)")
            }
            seqBySSRC[ssrc] = seq
            lastRxSeq = seq
        } else {
            // 중복 패킷 수신 (ESP32 2회 전송의 두 번째)
            return  // MIDI 처리 없이 바로 리턴
        }
        if (!bb.hasRemaining()) return
        val flag = bb.get().toInt() and 0xFF
        val longH = (flag and 0x80) != 0
        val fz    = (flag and 0x20) != 0
        val noCmd = (flag and 0x10) != 0
        val cmdLen = if (longH) {
            if (!bb.hasRemaining()) return
            ((flag and 0x0F) shl 8) or (bb.get().toInt() and 0xFF)
        } else flag and 0x0F
        if (noCmd || cmdLen == 0) return
        val end = bb.position() + cmdLen

        // FIX v2 (벐드 이펙트 약해짐 버그): 이 중복 제거는 원래 여러 패킷에 걸친 SysEx가
        // 두 사본이 섞여 깨지는 것만 막으면 되는데, 지금은 모든 메시지(노트/CC/벤드포함)에
        // 적용되고 있어서, 같은 값이 짧은 시간 안에 정당하게 반복되는 벤드(피치벤드 등)까지
        // 중복으로 오해해 먹어버려 벤드 커브가 뜻개졌다. 노트/벤드는 중복이 와도 두 번 처리돼서
        // 사실상 무해하니까, SysEx 관련 트래픽(지금 이어쓰는 중이거나 이 범위에 0xF0이 있는 경우)에만
        // 이 필터를 적용한다.
        val sysexRelated = inSysex || run {
            var i = bb.position(); val limit = minOf(end, len); var found = false
            while (i < limit) { if ((buf[i].toInt() and 0xFF) == 0xF0) { found = true; break }; i++ }
            found
        }
        if (sysexRelated) {
            var h = 1
            var i = bb.position()
            val limit = minOf(end, len)
            while (i < limit) { h = h * 31 + buf[i]; i++ }
            val nowMs2 = System.currentTimeMillis()
            if (h == lastPayloadHash && (nowMs2 - lastPayloadTimeMs) < 200) {
                return
            }
            lastPayloadHash = h; lastPayloadTimeMs = nowMs2
        }

        // ★ 처음 5패킷 MIDI 섹션 hex dump
        // 이 SysEx 버그를 끝까지 추적하기 위해 세션 초반 300패킷은 조건 없이 무조건 덤프한다.
        if (rtpDumpCount < 300) {
            val cmdBytes = buf.slice(bb.position() until minOf(end, len))
                .joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
            Log.i(TAG, "RTP[${rtpDumpCount}] seq=$seq inSysexAtStart=$inSysex flag=0x%02X longH=$longH fz=$fz cmdLen=$cmdLen [$cmdBytes]".format(flag))
            rtpDumpCount++
        }

        var first = true
        while (bb.position() < end && bb.hasRemaining()) {
            // FIX v2 (SysEx이어쓰기 데이터 1바이트 유실, 체크섬 에러의 진짜 근본 원인):
            // 이전 수정은 "패킷의 첫 명령일 때만" inSysex를 보고 skipDelta를 막았는데,
            // 이어지는 패킷은 앞에 F7 마커가 "가짜 첫 명령"으로 소비되면서 first=false가
            // 되어버려, 진짜 이어지는 데이터인 두 번째 명령에는 이 보호가 안 걸려 델타타임으로
            // 오인되어 진짝 데이터 바이트를 하나 먹어버렸다 (USB 원본과 체크섬 비교로 확인됨).
            // 수정: "첫 명령인지"가 아니라 "지금 SysEx 이어쓰는 중인지"를 매 명령마다 다시 확인해서
            // 반영한다.
            val skipBecauseInSysex = inSysex
            if (!skipBecauseInSysex && (!first || fz)) skipDelta(bb)
            first = false
            if (bb.position() >= end || !bb.hasRemaining()) break
            extractCmd(bb, end)
        }
    }

    private fun skipDelta(bb: ByteBuffer) {
        // MIDI 가변길이 delta: 최대 4바이트 (상위비트=1이면 계속, =0이면 종료)
        // mt32-pi: while(nLength < 4) → 4바이트. 이전 코드 0..2 = 3바이트만 처리 → 버그
        for (i in 0..3) {
            if (!bb.hasRemaining()) return
            if ((bb.get().toInt() and 0x80) == 0) return
        }
    }

    private var rs = 0

    private fun extractCmd(bb: ByteBuffer, end: Int) {
        if (!bb.hasRemaining()) return
        val mark = bb.position()
        val b = bb.get().toInt() and 0xFF
        when {
            b == 0xF0 -> {
                val chunk = mutableListOf<Byte>(0xF0.toByte())
                while (bb.hasRemaining() && bb.position() < end) {
                    val x = bb.get(); chunk.add(x)
                    if (x == 0xF7.toByte()) break
                }
                val complete = chunk.last() == 0xF7.toByte()
                // FIX (레벨미터 애니메이션 진짜 근본 원인, USB raw와 바이트 단위 비교로 확인됨):
                // 이 라이브러리(lathoub)는 SysEx가 패킷 경계를 넘어가면 미완성 조각의 마지막
                // 바이트를 "이어짐" 마커(항상 0xF0)로 쓰고, 이어지는 패킷은 0xF7로 시작한다.
                // 앞쪽 0xF7 마커는 이미 버리고 있었지만, 뒷쪽 0xF0 마커는 진짜 데이터처럼 그대로
                // 붙여넣고 있어서, USB 원본과 바이트 단위로 비교해보니 정확히 그 지점부터 한 칸씩 밀렸다.
                // 미완성이면 마지막 마커 바이트를 버리고 이어붙인다.
                val payload = if (complete) chunk else chunk.dropLast(1)
                val hex = payload.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
                Log.i(TAG, "SysEx발견: ${payload.size}bytes [$hex] 완료=$complete")
                rs = 0; sysexFeed(payload.toByteArray())
            }
            b and 0x80 == 0 && inSysex -> {
                bb.position(mark)
                val chunk = mutableListOf<Byte>()
                while (bb.hasRemaining() && bb.position() < end) {
                    val x = bb.get(); val xi = x.toInt() and 0xFF
                    chunk.add(x)
                    if (xi == 0xF7 || xi >= 0x80) break
                }
                val complete = chunk.isNotEmpty() && (chunk.last().toInt() and 0xFF) == 0xF7
                // 위와 동일한 이유: 이어쓰기 조각도 또 다음 패킷으로 이어지면(미완성) 마지막
                // 바이트가 실데이터가 아닌 "이어짐" 마커이므로 버려야 한다.
                val payload = if (complete) chunk else chunk.dropLast(1)
                val hex = payload.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }
                Log.i(TAG, "SysEx이어쓰기: ${payload.size}bytes [$hex] 완료=$complete")
                sysexFeed(payload.toByteArray())
            }
            b and 0x80 != 0 -> { rs = b; buildMsg(b, bb)?.let { onMidiMessage(it) } }
            rs != 0 -> { bb.position(mark); buildMsg(rs, bb)?.let { onMidiMessage(it) } }
        }
    }

    private fun buildMsg(s: Int, bb: ByteBuffer): ByteArray? {
        val n = when (s and 0xF0) {
            0x80, 0x90, 0xA0, 0xB0, 0xE0 -> 2
            0xC0, 0xD0 -> 1
            0xF0 -> when (s) { 0xF2 -> 2; 0xF1, 0xF3 -> 1; else -> 0 }
            else -> 0
        }
        if (bb.remaining() < n) return null
        return when (n) {
            0 -> byteArrayOf(s.toByte())
            1 -> byteArrayOf(s.toByte(), bb.get())
            2 -> byteArrayOf(s.toByte(), bb.get(), bb.get())
            else -> null
        }
    }
}



