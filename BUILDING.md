# E-ray MIDI – 빌드 가이드

## 1. 필요한 것

- Android Studio (Hedgehog 이상) 또는 CLI 빌드 시 JDK 17 + Android SDK/NDK
- NDK r25c 이상 (검증된 버전: `28.2.13676358`), CMake 3.22.1
- SC-55mk2 ROM 5종 (직접 준비, 미포함)
- MT-32 ROM 2종: `MT32_CONTROL.ROM`, `MT32_PCM.ROM` (직접 준비, 미포함)
- SF2 사운드폰트 파일 (선택 사항, SoundFont 모드용, 미포함)

## 2. 클론 및 코어 소스 준비

이 저장소는 두 개의 신시사이저 코어 소스를 `android-app/app/src/main/cpp/` 아래에
직접 포함하는 형태로 갖고 있습니다.

**Nuked-SC55** (`cpp/nuked-sc55/`가 비어 있다면):
```cmd
cd android-app\app\src\main\cpp
git clone --recurse-submodules https://github.com/jcmoyer/Nuked-SC55.git nuked-sc55
```
jcmoyer 포크를 쓰는 이유: 에뮬레이터 백엔드가 라이브러리 형태로 분리되어 있어 Android
임베딩이 쉽고, 성능 최적화가 포함되어 있으며(Raspberry Pi 4 기준 약 30% 개선),
업스트림과 동일한 오디오 출력을 냅니다.

**munt (MT-32/CM-32L)** (`cpp/munt/mt32emu/`가 비어 있다면):
```cmd
cd android-app\app\src\main\cpp
git clone https://github.com/munt/munt.git munt_upstream
robocopy munt_upstream\mt32emu munt\mt32emu /E /XD test
```
`CMakeLists.txt`의 `munt-jni` 타겟은 이 소스 목록을 기준으로 빌드합니다.
`SamplerateAdapter.cpp`/`SoxrAdapter.cpp`는 외부 라이브러리(libsamplerate/soxr)가
필요해 빌드 대상에서 제외되어 있습니다 (내부 리샘플러만 사용).

TinySoundFont(`tsf.h`)는 `android-app/app/src/main/cpp/tsf/`에 이미 포함되어 있습니다
(단일 헤더 라이브러리, 별도 클론 불필요).

## 3. local.properties 설정

`android-app/local.properties` 파일을 새로 만들고 본인 환경에 맞게 채웁니다
(이 파일은 `.gitignore`에 포함되어 저장소에는 없습니다):

```properties
sdk.dir=C\:\\Users\\<사용자명>\\AppData\\Local\\Android\\Sdk
ndk.dir=C\:\\Users\\<사용자명>\\AppData\\Local\\Android\\Sdk\\ndk\\28.2.13676358
```

## 4. 빌드

```cmd
cd android-app
set JAVA_HOME=C:\Program Files\Android\Android Studio\jbr
gradlew.bat assembleDebug
```

빌드 결과물: `android-app\app\build\outputs\apk\debug\app-debug.apk`

빌드 성공 시 세 개의 네이티브 라이브러리가 APK에 포함됩니다:
`libnuked-sc55-jni.so`, `libmunt-jni.so`, `libsoundfont-jni.so`.

## 5. ROM / 사운드폰트 배치

기기에 ADB로 파일을 넣습니다:

```cmd
adb push rom1.bin     /sdcard/Download/rom_sc55/
adb push rom2.bin     /sdcard/Download/rom_sc55/
adb push rom_sm.bin   /sdcard/Download/rom_sc55/
adb push waverom1.bin /sdcard/Download/rom_sc55/
adb push waverom2.bin /sdcard/Download/rom_sc55/

adb push MT32_CONTROL.ROM /sdcard/Download/rom_munt/
adb push MT32_PCM.ROM     /sdcard/Download/rom_munt/

adb push MySoundFont.sf2 /sdcard/Download/soundfont/
```

정확히 필요한 SC-55 ROM 파일명은 앱을 한 번 실행해서 "ROM 파일 안내" 버튼으로
확인하는 것을 권장합니다 (모델별로 조합이 다를 수 있음).

## 6. 설치 및 실행

```cmd
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

Android 11+ 에서는 최초 실행 시 "모든 파일 접근" 권한 요청이 뜹니다. ROM/사운드폰트
파일을 읽으려면 반드시 허용해야 합니다.

USB MIDI 주변장치 모드를 쓰려면 기기의 개발자 옵션 또는 USB 연결 설정에서
"MIDI"를 USB 연결 방식으로 선택해야 PC가 이 기기를 MIDI 장치로 인식합니다.

---

## 아키텍처 메모

### 세 엔진의 공통 인터페이스 (`IEngine.kt`)
`SC55Engine`, `MuntEngine`, `SoundFontEngine`은 모두 `IEngine`을 구현합니다.
초기화(`initEngine`)는 엔진마다 필요한 리소스가 달라서(ROM 폴더 vs MT-32 ROM 2개
vs .sf2 파일 경로) 인터페이스에 포함하지 않고, 재생/정지/MIDI 입력/리셋 등 공통
런타임 동작만 통일했습니다. `MainActivity`는 이 인터페이스 하나로 세 엔진을
동일한 방식으로 전환합니다.

### 샘플레이트 전략
- SC-55: 66207Hz 네이티브 고정 (리샘플링 금지 — 실제 SC-55mk2 출력 레이트)
- MT-32: 32000Hz 네이티브 고정
- SoundFont: 디바이스가 부여한 값 그대로 사용 (고정 레이트 제약 없음)

세 엔진은 상호 배타적으로만 동작(엔진 전환 시 이전 엔진을 완전히 `stop()`한 뒤에만
다음 엔진을 시작)하므로 공통 리샘플링 레이어나 AAudio 스트림 공유는 불필요합니다.
각 엔진이 자신의 AAudio 스트림을 독립적으로 열고 닫습니다.

### 오디오 렌더링 모델
- **SC-55**: MCU 사이클 스텝을 전용 스레드에서 반복 호출(push 모델), PCM 출력은
  AAudio 콜백에서 큐를 32 샘플 단위로 드레인.
- **MT-32**: MIDI 이벤트 큐 + AAudio 콜백에서 슬라이스 단위로 드레인 후 렌더링.
- **SoundFont**: MIDI 이벤트 처리는 별도의 일반 우선순위 스레드에서, 렌더링은
  AAudio 실시간 콜백에서 수행하며 둘 사이는 짧은 뮤텍스로만 동기화합니다.

### LCD 렌더링 (SC-55 모드)
`LCD_Render()`는 실제 물리 LCD처럼 "펌웨어가 화면을 갱신하는 도중" 상태를 그대로
캡처하지 않도록, 최근 일정 시간(수십 ms) 안에 쓰기가 있었으면 이번 프레임 렌더링을
건너뛰고 직전 안정 프레임을 유지하는 quiescence 게이트를 둡니다.

### MT-32 GS Reset 대응
일부 곡은 GS(SC-55) 전용으로 만들어져 곡 시작 시 롤랜드 표준 "GS Reset"
(`F0 41 dev 42 12 40 00 7F 00 ck F7`)을 보냅니다. mt32emu는 이 헤더(모델 ID `42`)를
모르니 무시하고, 뒤이은 GS 전용 파트 설정도 전부 무시되어 MT-32가 직전 곡의 잔존
상태를 그대로 물려받는 문제가 있었습니다. `MuntEngine.dispatchMidi()`는 이 GS
Reset 패턴을 감지하면 MT-32 자체 리셋으로 해석해 기본 상태로 되돌립니다. 또한
MT-32 파트 채널배정을 전부 OFF로 끄는 SysEx(주소 `10 00 0D`~`15`, 값 `10`) 뒤에
재배정이 잘못된 주소로 시도되어 실패하는 케이스도 감지해 그 SysEx 자체를 차단,
파트가 영구히 무음이 되는 것을 방지합니다.

### RTP-MIDI 안정성
`RtpMidiSession.kt`는 다음을 처리합니다:
- WiFi 절전 해제(`WIFI_MODE_FULL_LOW_LATENCY`, API 29+)
- 세션당 데이터 소켓에서 처음 본 SSRC만 인정 (중복/유령 스트림 차단)
- SysEx가 여러 RTP 패킷에 걸쳐 이어질 때 경계 마커 바이트(F0/F7)를 실제 데이터로
  오인하지 않도록 정확히 처리, 손상된 SysEx 뒤로 모든 MIDI가 영구히 삼켜지는 것 방지
- Roland DT1 SysEx 체크섬 사전 검증
- CC64(서스테인)/개별 노트 워치독
- 세션 종료 시 BY(EndSession) 패킷을 네트워크 언바인드 전에 전송해 ESP32가 세션을
  즉시 해제하도록 시도 (다만 엔진 전환 후 재연결이 지연되는 문제가 완전히 해결되지는
  않았습니다 — README의 "알려진 이슈" 참고)

### USB MIDI 주변장치 모드
`UsbMidiDeviceService.kt`(매니페스트에 등록된 가상 `MidiDeviceService`)와는 별개로,
실제 물리 USB 케이블로 연결된 PC의 MIDI는 안드로이드가 시스템 차원에서 자동으로
만들어주는 별도의 "USB 주변장치 포트" `MidiDevice`를 통해 들어옵니다.
`MainActivity.startUsbMidiPeripheral()`이 `MidiManager.getDevices()`/
`registerDeviceCallback()`으로 이 장치를 직접 찾아 열고, 그 출력 포트에 리시버를
연결해 `EngineRegistry.active`(현재 선택된 엔진)로 데이터를 전달합니다.

### 32kHz/66207Hz 오디오 (SC-55 모드)
SC-55 코어는 66207Hz로 오디오를 생성합니다. AAudio가 이 레이트를 직접 지원하지
않는 기기에서는 OS가 폴백 레이트로 리샘플링합니다.
