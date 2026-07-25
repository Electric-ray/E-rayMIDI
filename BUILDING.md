# Nuked SC-55 Android – 빌드 가이드

## 1. 필요한 것

- Android Studio (Hedgehog 이상) 또는 CLI 빌드 시 JDK 17 + Android SDK/NDK
- NDK r25c 이상, CMake 3.22.1
- SC-55mk2 ROM 5종 (직접 준비, 미포함)
- SF2 사운드폰트 파일 (선택 사항, SoundFont 모드용, 미포함)

## 2. 클론 및 서브모듈

이 저장소는 Nuked-SC55 코어 소스를 서브모듈/직접 포함 형태로 갖고 있습니다.
`android-app/app/src/main/cpp/nuked-sc55` 디렉터리가 비어 있다면:

```cmd
cd android-app\app\src\main\cpp
git clone --recurse-submodules https://github.com/jcmoyer/Nuked-SC55.git nuked-sc55
```

jcmoyer 포크를 쓰는 이유:
- 에뮬레이터 백엔드가 라이브러리 형태로 분리되어 있어 Android 임베딩이 쉬움
- 성능 최적화 포함 (Raspberry Pi 4 기준 약 30% 개선)
- 업스트림과 동일한 오디오 출력

TinySoundFont(`tsf.h`)는 `android-app/app/src/main/cpp/tsf/`에 이미 포함되어 있습니다
(단일 헤더 라이브러리, 별도 클론 불필요).

## 3. local.properties 설정

`android-app/local.properties` 파일을 새로 만들고 본인 환경에 맞게 채웁니다
(이 파일은 `.gitignore`에 포함되어 저장소에는 없습니다):

```properties
sdk.dir=C\:\\Users\\<사용자명>\\AppData\\Local\\Android\\Sdk
ndk.dir=C\:\\Users\\<사용자명>\\AppData\\Local\\Android\\Sdk\\ndk\\25.2.9519653
```

## 4. 빌드

```cmd
cd android-app
set JAVA_HOME=C:\Program Files\Android\Android Studio\jbr
gradlew.bat assembleDebug
```

빌드 결과물: `android-app\app\build\outputs\apk\debug\app-debug.apk`

## 5. ROM / 사운드폰트 배치

기기에 ADB로 파일을 넣습니다:

```cmd
adb push rom1.bin     /sdcard/Download/rom_sc55/
adb push rom2.bin     /sdcard/Download/rom_sc55/
adb push rom_sm.bin   /sdcard/Download/rom_sc55/
adb push waverom1.bin /sdcard/Download/rom_sc55/
adb push waverom2.bin /sdcard/Download/rom_sc55/

adb push MySoundFont.sf2 /sdcard/Download/soundfont/
```

정확히 필요한 ROM 파일명은 앱을 한 번 실행해서 "ROM 파일 안내" 버튼으로 확인하는 것을
권장합니다 (모델별로 조합이 다를 수 있음).

## 6. 설치 및 실행

```cmd
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

Android 11+ 에서는 최초 실행 시 "모든 파일 접근" 권한 요청이 뜹니다. ROM/사운드폰트
파일을 읽으려면 반드시 허용해야 합니다.

---

## 포팅 관련 메모

### SDL2 제거
원본 Nuked-SC55는 SDL2로 오디오를 출력합니다. Android 포팅에서는 SDL2를 완전히
제거하고 AAudio로 대체했습니다 (`CMakeLists.txt`의 `NO_SDL=1` 정의로 SDL 관련 코드
비활성화).

### 오디오 엔진 구조
- **SC-55 모드**: MCU 사이클 스텝은 전용 스레드에서 반복 호출하고, PCM 출력은
  AAudio 콜백에서 가져갑니다. SC-55는 32kHz가 아니라 66207Hz로 오디오를 오버샘플링
  출력하며, AAudio가 이 레이트를 직접 못 받으면 48kHz로 폴백 후 선형보간 SRC를 씁니다.
- **SoundFont 모드**: TinySoundFont는 고정 네이티브 레이트 제약이 없어서 훨씬
  단순합니다. MIDI 이벤트 처리는 별도의 일반 우선순위 스레드에서, 렌더링은 AAudio
  실시간 콜백에서 수행하며 둘 사이는 짧은 뮤텍스로만 동기화합니다 (MIDI 버스트가
  몰려도 실시간 오디오/네트워크 스레드의 스케줄링을 방해하지 않기 위함).

### LCD 렌더링 (SC-55 모드)
`LCD_Render()`는 실제 물리 LCD처럼 "펌웨어가 화면을 갱신하는 도중" 상태를 그대로
캡처하지 않도록, 최근 일정 시간(수십 ms) 안에 쓰기가 있었으면 이번 프레임 렌더링을
건너뛰고 직전 안정 프레임을 유지하는 quiescence 게이트를 둡니다. 비트맵도 트리플
버퍼링으로 화면 표시 중인 프레임을 곧바로 재사용하지 않도록 합니다.

### RTP-MIDI 안정성
`RtpMidiSession.kt`는 다음을 처리합니다:
- WiFi 절전 해제(`WIFI_MODE_FULL_LOW_LATENCY`, API 29+)
- 세션당 데이터 소켓에서 처음 본 SSRC만 인정 (중복/유령 스트림 차단)
- SysEx가 여러 RTP 패킷에 걸쳐 이어질 때, 경계 마커 바이트(F0/F7)를 실제 데이터로
  오인하지 않도록 정확히 처리
- Roland DT1 SysEx는 전달 전에 체크섬을 직접 검증해서, 송신측에서 유실된 바이트로
  깨진 메시지가 엔진에 들어가 "DATA ERROR"를 유발하지 않도록 사전 차단
- CC64(서스테인)/개별 노트에 대해 일정 시간 이상 응답이 없으면 강제로 정리하는
  워치독 (RTP 특성상 Note Off 패킷 자체가 유실될 수 있어 필요)

ESP32 측(별도 저장소 [E-RayDSB](https://github.com/Electric-ray/E-RayDSB))에서도
`WiFi.setSleep(false)`와 AppleMIDI 세션 `MaxNumberOfParticipants=1` 설정이 되어
있어야 위 안정성 수정과 맞물려 정상 동작합니다.

### 32kHz/66207Hz 오디오 (SC-55 모드)
SC-55 코어는 66207Hz로 오디오를 생성합니다. AAudio가 이 레이트를 직접 지원하지
않는 기기에서는 48kHz로 폴백 후 선형보간 리샘플링을 거칩니다.
