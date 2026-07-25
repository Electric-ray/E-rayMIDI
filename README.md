# Nuked SC-55 for Android

Roland SC-55 시리즈 에뮬레이터를 Android로 포팅한 앱입니다.
[nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55) 및
[jcmoyer/Nuked-SC55](https://github.com/jcmoyer/Nuked-SC55) (성능 최적화 포크)를 기반으로 하며,
[schellingb/TinySoundFont](https://github.com/schellingb/TinySoundFont)를 이용한
SF2 사운드폰트 재생 모드도 함께 지원합니다.

DOS PC에서 나가는 MIDI 신호를 ESP32를 거쳐 WiFi(RTP-MIDI) 또는 USB로 안드로이드 기기에
전달하고, 안드로이드 기기가 실제 SC-55 칩 동작을 사이클 단위로 재현하거나(SC-55 모드),
SF2 사운드폰트로 재생(SoundFont 모드)하는 구조입니다.

---

## 시스템 구성

```
DOS PC --RS232--► ESP32 --WiFi(RTP-MIDI)--► Android
           └-----------USB OTG------------►    │
                                                ├─ SC-55 모드: Nuked-SC55 코어 (ROM 필요)
                                                └─ SoundFont 모드: TinySoundFont (.sf2 필요)
                                                        │
                                                        ▼
                                                   오디오 출력 (AAudio)
```

ESP32 쪽 펌웨어(RTP-MIDI 송신)는 별도 저장소
[Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)에서 관리합니다.

| 항목            | 사양                                          |
|-----------------|------------------------------------------------|
| 테스트 기기      | LG Velvet (LG-G910N), Android 10 (API 29)      |
| 아키텍처         | arm64-v8a, armeabi-v7a                         |
| 최소 안드로이드   | Android 10 (API 29)                            |
| SC-55 ROM       | 별도 준비 필요 (저작권으로 미포함)               |
| SF2 사운드폰트   | 별도 준비 필요 (미포함)                          |

---

## 주요 기능

- **연결 방식 선택**: RTP-MIDI(WiFi) 또는 USB 시리얼(추천), 앱 실행 시 선택
- **재생 엔진 선택**: SC-55(사이클 단위 에뮬레이션) 또는 SoundFont(SF2 샘플 재생)
- **SC-55 모드**: 실제 LCD 컨트롤러 동작을 픽셀 단위로 재현 (파라미터 레벨미터 애니메이션 포함)
- **SoundFont 모드**: `Download/soundfont` 폴더의 `.sf2` 파일 목록에서 선택, 마지막 사용 파일 자동 기억
- **RTP-MIDI 안정성 보강**: WiFi 절전 해제, 수신 SSRC 고정(중복 스트림 방지), SysEx 패킷
  경계 처리, DT1 체크섬 검증, 서스테인/노트 워치독(패킷 유실 시 자동 정리) 등
  실사용 과정에서 발견된 다수의 안정성 문제를 수정

## 알려진 한계

- RTP-MIDI(WiFi/UDP)는 구조적으로 USB(유선 시리얼)보다 패킷 유실 위험이 있습니다.
  ESP32 측 AppleMIDI 라이브러리(lathoub/Arduino-AppleMIDI-Library)가 RTP-MIDI
  Recovery Journal(RFC 6295)을 구현하지 않아, 유실된 메시지는 원천적으로 복구할 수
  없습니다. 앱은 노트/서스테인 워치독으로 그 여파(무한 잔향 등)만 완화합니다.
- SoundFont 모드는 GM 표준까지만 지원하며, Roland GS 전용 확장(리듬 채널 재배정,
  뱅크 배리에이션 등)은 표현하지 못해 일부 악기가 SC-55 모드와 다르게 들릴 수 있습니다.
- 현재 RTP-MIDI 연결은 자잘한 버그가 많습니다. 유선연결인 USB 연결을 추천합니다.
- SC55의 LCD의 깜빡임이 있습니다. 추후 개선토록 하겠습니다. 

---

## ROM 파일 배치 (SC-55 모드)

SC-55mk2 ROM 파일을 아래 경로에 배치합니다:

```
/sdcard/Download/rom_sc55/
```

필요한 정확한 파일명 목록은 앱 실행 후 "ROM 파일 안내" 버튼에서 확인할 수 있습니다
(모델별로 다를 수 있어 코드가 아닌 앱이 직접 안내하도록 되어 있습니다).

## 사운드폰트 파일 배치 (SoundFont 모드)

`.sf2` 파일을 아래 경로에 배치합니다:

```
/sdcard/Download/soundfont/
```

---

## 빌드 환경

- Android Studio Hedgehog 이상
- NDK r25c 이상
- CMake 3.22.1
- Target SDK 36 (Android 15) / Min SDK 29 (Android 10)
- C++17

빌드 방법은 [BUILDING.md](BUILDING.md)를 참고하세요.

---

## 라이선스 / 크레딧

- 앱 코드: MIT License
- [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55) 코어: MAME License (비상업적 사용만 가능)
- [TinySoundFont](https://github.com/schellingb/TinySoundFont): MIT License
- [Arduino-AppleMIDI-Library](https://github.com/lathoub/Arduino-AppleMIDI-Library) (ESP32 RTP-MIDI): MIT License
- [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android): Apache 2.0
- SC-55 ROM: 별도 라이선스 (Roland Corp.) — 미포함
- SF2 사운드폰트: 각 사운드폰트 제작자의 라이선스를 따름 — 미포함
