package com.example.nukedsc55

/**
 * IEngine — SC55Engine / MuntEngine / SoundFontEngine이 공유하는 공통 계약.
 *
 * (통합작업순서.md Phase 1 참고)
 *
 * 설계 메모: 초기화(initEngine)는 엔진마다 필요한 리소스가 근본적으로 달라서
 * (SC55Engine은 ROM 폴더 하나, MuntEngine은 MT-32 ROM 2개, SoundFontEngine은
 * .sf2 파일 경로 하나) 이 인터페이스에 억지로 통일된 init() 시그니처를 넣지
 * 않았다. 각 엔진은 자신만의 init 함수(initEngine(model), initEngine(sf2Path)
 * 등)로 리소스를 준비한 뒤, MainActivity는 그 다음부터는 IEngine 참조 하나로
 * 재생/정지/엔진 전환을 동일하게 다룬다.
 *
 * RtpMidiSession / UsbMidiManager는 완전히 엔진 독립적(콜백 기반)이라 세 엔진
 * 모두 동일한 인스턴스를 그대로 재사용한다 — 이 인터페이스는 그 재사용 구조를
 * 바꾸지 않고, 단지 MainActivity 쪽 엔진 스위칭 코드를 다형적으로 만든다.
 */
interface IEngine {

    /** 상태 메시지 콜백 (UI 표시용) */
    var onStatus: ((String) -> Unit)?

    /** 엔진이 현재 초기화되어 재생 가능한 상태인지 */
    val engineRunning: Boolean

    /** 이 엔진의 실제 네이티브 오디오 출력 레이트(Hz).
     *  SC-55: 66207Hz 고정 (리샘플링 금지 — 절대 하드코딩 32000Hz로 되돌리지 않기)
     *  munt : 32000Hz 고정
     *  SoundFont: 디바이스가 부여한 값 그대로(고정 레이트 제약 없음) */
    fun getNativeSampleRate(): Int

    // ── MIDI 입력 경로 (RTP-MIDI / USB-Serial) ──────────────────────────
    // 엔진 전환 시 반드시 이전 엔진의 stop()을 먼저 호출한 뒤 다음 엔진의
    // startRtp()/startUsb()를 호출할 것 — 두 엔진이 동시에 같은 네트워크/USB
    // 리소스를 잡으면 충돌한다.
    fun startRtp()
    fun stopRtp()
    fun startUsb(): Boolean
    fun stopUsb()

    /** 원시 MIDI 바이트 수신 (RtpMidiSession/UsbMidiManager 콜백에서 호출) */
    fun dispatchMidi(bytes: ByteArray)

    /** stuck note 방지용 강제 All Notes Off / All Sound Off */
    fun allNotesOff()

    /**
     * 엔진 리셋.
     * hard=true → 엔진 내부 상태를 완전히 초기 패치 상태로 되돌림
     *   (munt: MT-32 Master Reset SysEx 재생)
     *   (SC-55: 절대 자동 GS Reset을 걸지 않기로 확정됨 — SC55Engine 구현체는
     *    이 플래그와 무관하게 All Notes/Sound Off만 수행하고 GS Reset은
     *    사용자가 명시적으로 누르는 수동 리셋 버튼 경로에서만 별도 처리한다.
     *    자동 전환 로직에서 이 함수에 hard=true를 넘기더라도 SC55Engine은
     *    GS Reset을 걸지 않는다 — 정책 위반 방지용 안전장치.)
     */
    fun resetEngine(hard: Boolean)

    /** 엔진 완전 종료 (RTP/USB 정지 + 네이티브 리소스 해제) */
    fun stop()
}
