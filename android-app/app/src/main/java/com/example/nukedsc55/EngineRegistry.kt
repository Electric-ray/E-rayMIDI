package com.example.nukedsc55

/**
 * EngineRegistry — 현재 재생 중인 엔진(IEngine)에 대한 전역 참조.
 *
 * UsbMidiDeviceService(안드로이드를 USB MIDI 주변장치로 노출 — Windows PC 등에서
 * 이 기기를 표준 MIDI 입력 장치로 인식)는 MainActivity와 완전히 독립된 시스템
 * 서비스 컴포넌트로 실행되므로, 지금 어떤 엔진이 활성 상태인지 알 방법이 없다.
 * MainActivity가 엔진을 연결/해제할 때마다 이 레지스트리를 갱신해주면,
 * UsbMidiDeviceService는 그저 EngineRegistry.active로 들어오는 MIDI를 넘기기만
 * 하면 된다 — SC55Engine이든 MuntEngine이든 SoundFontEngine이든 무관하게 동작한다.
 *
 * (munt-android의 MuntEngine.getInstance() 싱글톤 패턴을 참고했으나, 이 프로젝트는
 * 엔진이 3개라서 "고정된 싱글톤 엔진" 대신 "현재 활성 엔진에 대한 교체 가능한 참조"
 * 방식으로 확장했다.)
 */
object EngineRegistry {
    @Volatile
    var active: IEngine? = null
}
