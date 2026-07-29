# E-ray MIDI

Roland SC-55 / MT-32 / SoundFont(SF2) 세 가지 재생 엔진과, RTP-MIDI(WiFi) / USB 시리얼 /
USB MIDI 주변장치(peripheral) 세 가지 연결 방식을 하나로 통합한 Android MIDI 재생 앱입니다.

원래는 [nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55)의 Android 포팅으로
시작했지만, [munt](https://github.com/munt/munt)(MT-32/CM-32L) 코어와
[TinySoundFont](https://github.com/schellingb/TinySoundFont) 기반 SF2 재생을 함께
통합하면서 단일 엔진 포팅 프로젝트를 넘어선 멀티 엔진 MIDI 모듈이 되었습니다.

---

## 무엇을 할 수 있나요

DOS PC(또는 다른 MIDI 소스)에서 나가는 MIDI 신호를 안드로이드 기기로 가져와서,
세 가지 레트로 신시사이저 중 하나로 실제 소리를 냅니다.

```
                 ┌─ RTP-MIDI (WiFi, ESP32 경유)
DOS PC/호스트 ───┼─ USB 시리얼 (ESP32 경유, OTG)
                 └─ USB MIDI 주변장치 (케이블로 PC와 직결, 안드로이드가 표준 MIDI 장치로 인식됨)
                                │
                                ▼
                     E-ray MIDI (안드로이드 앱)
                                │
              ┌─────────────────┼─────────────────┐
              ▼                 ▼                 ▼
        SC-55 엔진          MT-32 엔진        SoundFont 엔진
     (Nuked-SC55 코어)      (munt 코어)      (TinySoundFont)
       ROM 파일 필요        ROM 파일 필요      .sf2 파일 필요
              │                 │                 │
              └─────────────────┴─────────────────┘
                                ▼
                         오디오 출력 (AAudio)
```

ESP32 쪽 RTP-MIDI/USB 브리지 펌웨어는 별도 저장소
[Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)에서 관리합니다.

| 항목 | 사양 |
|---|---|
| 테스트 기기 | LG Velvet (LG-G910N), Android 10 (API 29) |
| 아키텍처 | arm64-v8a |
| 최소 안드로이드 | Android 10 (API 29) |
| SC-55 ROM | 별도 준비 필요 (저작권으로 미포함) |
| MT-32 ROM | 별도 준비 필요 (저작권으로 미포함) |
| SF2 사운드폰트 | 별도 준비 필요 (미포함) |

---

## 주요 기능

### 연결 방식 (셋 중 선택)
- **📡 RTP-MIDI**: ESP32가 WiFi AppleMIDI로 중계. 케이블 없이 무선으로 연결.
- **🔌 USB 시리얼**: DOS PC와 시리얼널모뎀 케이블로 직결.
- **🎹 USB MIDI기기**: 안드로이드 기기 자체를 USB MIDI 주변장치로 노출 — Windows 등
  PC에 케이블로 연결하면 표준 MIDI 입력 장치로 인식되어, SoftMPU 같은 DOS MIDI 드라이버나
  DAW가 직접 이 폰으로 MIDI를 보낼 수 있습니다. (`MidiManager`로 시스템이 제공하는
  USB peripheral 포트를 직접 열어 연결)

### 재생 엔진 (셋 중 선택)
- **SC-55**: Nuked-SC55 코어를 그대로 이식, MCU 사이클 단위 에뮬레이션. 실제 LCD
  컨트롤러 동작을 픽셀 단위로 재현 (파라미터 레벨미터 애니메이션 포함).
- **MT-32**: munt 코어 이식. munt-android 원본 GUI(파트별 LED + 패치명)를 재현.
  GS 전용으로 만들어진 곡을 재생할 때 발생하는 무음 문제에 대한 자동 대응 포함
  (아래 "알려진 이슈와 대응" 참고).
- **SoundFont**: TinySoundFont 기반 `.sf2` 재생. 채널별 프리셋명 실시간 표시.

### 안정성 보강 (실사용 중 발견된 버그 수정)
- RTP-MIDI SysEx가 여러 패킷에 걸쳐 전송될 때 경계 처리 오류로 LCD 애니메이션이
  깨지던 문제 수정
- SysEx가 손상되어 종료 마커(F0/F7)를 못 만나면 이후 모든 MIDI를 영구히 삼켜버리던
  파서 버그 수정 (USB 경로)
- MIDI 이벤트 큐 dedup 로직이 초당 다수 이벤트를 드롭하던 문제 수정, Note Off
  판별 오류(velocity=0) 수정
- GS(SC-55) 전용으로 만들어진 곡을 MT-32로 재생할 때 감지되는 "GS Reset" 신호를
  MT-32 자체 리셋 신호로 해석해 파트 채널배정이 꼬여 무음이 되는 문제 완화
- 엔진 전환 시 RTP-MIDI 세션이 정상 종료(BY 패킷)되도록 보강

---

## 알려진 이슈와 대응

- **RTP-MIDI 모드에서 엔진을 빠르게 전환하면 재연결까지 시간이 걸릴 수 있습니다.**
  AppleMIDI 라이브러리 쪽에서 세션 종료 신호를 받고도 즉시 새 연결을 받아주지
  않는 케이스가 확인되었으나, 근본 원인은 아직 못 찾았습니다. **USB 시리얼 또는
  USB MIDI기기 모드는 이 문제가 없습니다** — 잦은 엔진 전환이 필요하면 이 두 방식을
  권장합니다.
- MT-32로 재생 시, 게임이 GS(SC-55) 전용 SysEx만 보내고 MT-32용 설정을 전혀 안
  보내는 곡은 자동 리셋으로도 완전히 해결되지 않을 수 있습니다 (원래 그 곡이
  실제 MT-32 하드웨어에서도 온전히 재생되지 않을 가능성이 있습니다 — SC-55 모드로
  같은 곡을 먼저 확인해보세요).
- SoundFont 모드는 GM 표준까지만 지원하며 Roland GS 전용 확장은 표현하지 못합니다.
- SC-55 LCD 렌더링에 미세한 깜빡임이 있을 수 있습니다.

---

## ROM / 사운드폰트 파일 배치

| 엔진 | 경로 |
|---|---|
| SC-55 | `/sdcard/Download/rom_sc55/` |
| MT-32 | `/sdcard/Download/rom_munt/` |
| SoundFont | `/sdcard/Download/soundfont/` (`.sf2` 파일) |

SC-55에 정확히 필요한 ROM 파일명은 앱 실행 후 "ROM 파일 안내" 버튼에서 확인할 수
있습니다 (모델별로 다를 수 있음). ROM/사운드폰트 파일은 저작권 보호 대상이라
이 저장소에는 포함되어 있지 않습니다 — 직접 준비해야 합니다.

더 자세한 사용법은 [docs/사용설명서.md](docs/사용설명서.md)를 참고하세요.

---

## 빌드 환경

- Android Studio Hedgehog 이상
- NDK r25c 이상 (검증된 버전: 28.2.13676358)
- CMake 3.22.1
- Target SDK 36 (Android 15) / Min SDK 29 (Android 10)
- C++17 / C++20

빌드 방법은 [BUILDING.md](BUILDING.md)를 참고하세요.

---

## 프로젝트 구조

```
android-app/
└── app/src/main/
    ├── java/com/example/nukedsc55/
    │   ├── IEngine.kt              # SC55Engine/MuntEngine/SoundFontEngine 공통 인터페이스
    │   ├── SC55Engine.kt           # Nuked-SC55 코어 어댑터
    │   ├── MuntEngine.kt           # munt(mt32emu) 코어 어댑터
    │   ├── SoundFontEngine.kt      # TinySoundFont 어댑터
    │   ├── RtpMidiSession.kt       # RTP-MIDI(AppleMIDI) 클라이언트
    │   ├── UsbMidiManager.kt       # USB 시리얼 입력 (usb-serial-for-android)
    │   ├── MidiStreamParser.kt     # Running-status/SysEx MIDI 바이트 파서
    │   ├── EngineRegistry.kt       # USB MIDI 주변장치 모드용 현재 활성 엔진 참조
    │   ├── UsbMidiDeviceService.kt # 안드로이드를 가상 MIDI 장치로 노출 (MidiDeviceService)
    │   └── MainActivity.kt
    └── cpp/
        ├── SC55Bridge.cpp          # Nuked-SC55 JNI 브리지
        ├── MuntBridge.cpp          # munt(mt32emu) JNI 브리지
        ├── TsfBridge.cpp           # TinySoundFont JNI 브리지
        ├── munt/mt32emu/           # munt 코어 소스 (이식됨)
        └── nuked-sc55/             # Nuked-SC55 코어 소스
```

---

## 라이선스 / 크레딧

- 앱 코드: MIT License
- [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55) 코어: MAME License (비상업적 사용만 가능)
- [munt](https://github.com/munt/munt) (MT-32/CM-32L) 코어: LGPL-2.1
- [TinySoundFont](https://github.com/schellingb/TinySoundFont): MIT License
- [Arduino-AppleMIDI-Library](https://github.com/lathoub/Arduino-AppleMIDI-Library) (ESP32 RTP-MIDI): MIT License
- [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android): Apache 2.0
- ESP32 브리지 펌웨어: [Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)
- SC-55 / MT-32 ROM: 별도 라이선스 (Roland Corp.) — 미포함
- SF2 사운드폰트: 각 제작자의 라이선스를 따름 — 미포함
