# E-ray MIDI – 빌드 가이드

## 1. 필요한 것

- Android Studio (Hedgehog 이상) 또는 CLI 빌드 시 JDK 17 + Android SDK/NDK
- NDK r25c 이상 (검증된 버전: `28.2.13676358`), CMake 3.22.1
- SC-55mk2 ROM 5종 (직접 준비, 미포함)
- MT-32 ROM 2종: `MT32_CONTROL.ROM`, `MT32_PCM.ROM` (직접 준비, 미포함)
- SF2 사운드폰트 파일 (선택 사항, SoundFont 모드용, 미포함)
- S-YXG50 ROM 2종: `sxgbin41.tbl`, `sxgwave4.tbl` (직접 준비, 미포함)
- **(S-YXG50 빌드용, v1.3 추가)**
  - Rust stable 1.85 이상 — [rustup.rs](https://rustup.rs)로 설치 (edition 2024가
    stable에 정식 지원되므로 nightly 불필요)
  - Android 타겟 4종:
    `rustup target add aarch64-linux-android armv7-linux-androideabi x86_64-linux-android i686-linux-android`
  - `cargo install cargo-ndk`
  - LLVM/libclang — 일부 의존 크레이트가 bindgen으로 C 코드를 바인딩할 때 필요.
    Windows: `winget install LLVM.LLVM` (설치 후 `LIBCLANG_PATH`를
    `C:\Program Files\LLVM\bin`으로 설정, `CMakeLists.txt`에 이미 반영되어 있음)

## 2. 클론 및 코어 소스 준비

이 저장소는 신시사이저 코어 소스들을 `android-app/app/src/main/cpp/` 아래에
직접 포함(벤더링)하는 형태로 갖고 있습니다.

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

**FluidSynth** (`cpp/fluidsynth/`가 비어 있다면 — v1.3부터 TinySoundFont 대신 사용):
```powershell
$dir = "android-app\app\src\main\cpp\fluidsynth"
New-Item -ItemType Directory -Force -Path $dir
Invoke-WebRequest -Uri "https://github.com/FluidSynth/fluidsynth/releases/download/v2.6.0/fluidsynth-v2.6.0-android24.zip" -OutFile "$dir\fs.zip"
Expand-Archive -Path "$dir\fs.zip" -DestinationPath $dir -Force
Remove-Item "$dir\fs.zip"
```
공식 GitHub Release의 Android 프리빌트 바이너리를 그대로 씁니다(glib 의존성이
빠진 2.5.0 이후 버전이라 별도로 크로스컴파일할 필요가 없습니다). 최신 릴리즈
목록은 `https://github.com/FluidSynth/fluidsynth/releases` 참고.

> 이 저장소에는 `lib/arm64-v8a/`만 커밋되어 있습니다(현재 `abiFilters`가
> arm64-v8a 전용이라). 다른 ABI를 지원하려면 위 zip의 해당 ABI 폴더를 그대로
> `lib/<abi>/`에 추가하면 됩니다.

**madaha (S-YXG50)** (`cpp/madaha/`가 비어 있다면 — v1.3 신규):
```cmd
cd android-app\app\src\main\cpp
git clone https://github.com/madaha-dev/madaha.git madaha
```
클론 후 `.git` 폴더는 지우고 벤더링 형태로 커밋합니다(이 저장소 자체가 아직
git submodule을 안 쓰기 때문). madaha 소스에는 Android 이식을 위한 자체 패치가
이미 적용되어 있습니다 — 원본 그대로 새로 클론하면 이 패치들이 사라지니, 실제로는
**이 저장소에 이미 커밋되어 있는 `cpp/madaha/`를 그대로 쓰는 것을 권장**합니다.
새 버전으로 업데이트하려면 README의 "v1.3에서 달라진 점 - 2"에 정리된 수정
내역을 참고해서 다시 적용해야 합니다.

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

CMake 구성 단계에서 `cargo-ndk`가 자동으로 `madaha_core`를 arm64-v8a용으로
크로스컴파일합니다(처음 빌드할 때만 다소 걸릴 수 있음, 이후에는 cargo 자체가
증분 빌드라 소스 변경이 없으면 빠릅니다).

빌드 성공 시 다음 네이티브 라이브러리들이 APK에 포함됩니다:
`libnuked-sc55-jni.so`, `libmunt-jni.so`, `libsoundfont-jni.so`,
`libsyxg50-jni.so`, 그리고 FluidSynth의 의존 라이브러리 10개
(`libfluidsynth.so` 등, `cpp/fluidsynth/lib/arm64-v8a/`에서 그대로 복사됨).

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

adb push sxgbin41.tbl  /sdcard/Download/rom_s-yxg50/   # (v1.5부터 S-YXG50 엔진은 제거됨 — 예전 빌드용)
adb push sxgwave4.tbl  /sdcard/Download/rom_s-yxg50/

# 88emu: 기종 ROM (파일명 무관, 내용으로 자동 인식)
adb push <ROM 파일들> /sdcard/Download/rom_gearmulator/

# S-MU2000: 프로그램 + 웨이브 ROM 4개(파일명 정확히), (선택) LCD 문자 ROM
adb push mu2000_flash.bin   /sdcard/Download/rom_mu2000/
adb push xv364a0.ic49 xv365a0.ic50 xw848a0.ic53 xw849a0.ic54 /sdcard/Download/rom_mu2000/dump/
adb push hd44780u_b04.bin   /sdcard/Download/rom_mu2000/    # 없으면 내장 폰트로 LCD 표시
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

### 네 엔진의 공통 인터페이스 (`IEngine.kt`)
`SC55Engine`, `MuntEngine`, `SoundFontEngine`, `SYXG50Engine`은 모두 `IEngine`을
구현합니다. 초기화(`initEngine`)는 엔진마다 필요한 리소스가 달라서(ROM 폴더 vs
MT-32 ROM 2개 vs .sf2 파일 경로 vs S-YXG50 ROM 2개) 인터페이스에 포함하지 않고,
재생/정지/MIDI 입력/리셋 등 공통 런타임 동작만 통일했습니다. 채널별 LED 상태
표시용 `PartInfo` 데이터 클래스도 `IEngine.kt`에 공용으로 정의되어 있고, 네
엔진의 `getPartInfo()`가 전부 같은 형태로 반환합니다(v1.9부터 채널 LED 패널은 제거되어
화면에서는 쓰이지 않고, 모든 모드가 엔진마다 LCD 하나로 표시됩니다 — 아래 "가상 LCD" 참고). `MainActivity`는 `IEngine`
하나로 네 엔진을 동일한 방식으로 전환합니다.

### 샘플레이트 전략
- SC-55: 66207Hz 네이티브 고정 (리샘플링 금지 — 실제 SC-55mk2 출력 레이트)
- MT-32: 32000Hz 네이티브 고정
- SoundFont / S-YXG50: 디바이스가 부여한 값 그대로 사용 (고정 레이트 제약 없음.
  S-YXG50 내부적으로는 44100Hz 고정 소스를 madaha가 자체 리샘플링해서 디바이스
  레이트로 출력)

네 엔진은 상호 배타적으로만 동작(엔진 전환 시 이전 엔진을 완전히 `stop()`한 뒤에만
다음 엔진을 시작)하므로 공통 리샘플링 레이어나 AAudio 스트림 공유는 불필요합니다.
각 엔진이 자신의 AAudio 스트림을 독립적으로 열고 닫습니다.

### 오디오 렌더링 모델 (v1.3 — 네 엔진 모두 통일)
네 엔진 모두 "전용 렌더 스레드가 미리 렌더링해 락프리 링버퍼에 채워두고, AAudio
데이터 콜백은 그 링버퍼에서 pop만 한다"는 동일한 구조를 씁니다:

```
Kotlin 스레드 → MIDI 이벤트 큐(뮤텍스) → MIDI 처리 스레드 → 신스 엔진 API 호출
                                                                    │
전용 렌더 스레드가 신스를 계속 렌더링 ──────────────────────────────┘
     │
     ▼
락프리 링버퍼 (head/tail atomic)
     │
     ▼
AAudio 데이터 콜백 — 링버퍼에서 pop만 (절대 무거운 연산 안 함)
```

AAudio 콜백 안에서 렌더링을 직접 하면(v1.3 이전 SoundFont/S-YXG50이 이랬음)
노트/화음이 많은 순간 렌더 비용이 튀면서 콜백의 엄격한 데드라인을 놓쳐 끊김+
노이즈가 난다는 게 실기기 테스트로 확인되어, 모든 엔진을 이 구조로 통일했습니다.

또한 네 엔진 모두 AAudio 스트림을 열 때 `AAUDIO_SHARING_MODE_SHARED`를 우선
시도합니다(예전에는 지연을 낮추려고 `EXCLUSIVE`를 우선했으나, EXCLUSIVE 모드가
기기 내장 스피커 저지연 경로에만 고정되어 유선/블루투스 이어폰·외부 스피커로
라우팅이 안 되는 문제가 실기기에서 확인됨 — SHARED는 AudioFlinger 믹서를 거치므로
출력 기기 전환을 정상적으로 따라감).

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

### S-YXG50 / madaha 통합 구조 (v1.3 신규)
madaha는 원래 리눅스 데스크톱 CLI(ALSA MIDI 입력 + cpal 오디오 출력)로 설계된
Rust 프로그램입니다. Android 이식을 위해:

- `Cargo.toml`에 `[lib] crate-type = ["staticlib", "rlib"]`을 추가하고,
  `alsa`/`cpal`/`mimalloc` 의존성을 `desktop-io`라는 feature(기본 켜짐) 뒤로
  격리했습니다. Android 빌드는 `--no-default-features`로 이 셋을 통째로
  제외합니다.
- `src/ffi.rs`가 Android용 C ABI 표면입니다: `madaha_init/destroy/send_midi/
  send_sysex/render_i16/get_last_error` 등을 `#[unsafe(no_mangle)]`로 노출합니다.
  ALSA Seq가 원래 대신 처리해주던 RPN/NRPN(CC98/99/100/101/6/38) 누적 로직을
  여기서 직접 구현했습니다(패킷화된 MIDI 바이트만 들어오므로).
- `madaha_init`의 실제 초기화 작업(Engine/AudioRender 생성 등)은 32MB 스택을
  가진 전용 스레드에서 실행됩니다 — `Engine` 구조체 안에 정확히 8MB짜리
  사전계산 테이블(`note_cent_table`)이 값으로 들어있어서, 기본 스택 크기의
  스레드(특히 AAudio 실시간 콜백 스레드)에서 그대로 두면 스택 오버플로우로
  죽습니다. 완성된 `Instance`는 그 전용 스레드 안에서 즉시 `Box`로 힙에 옮긴 뒤
  포인터만 호출 스레드로 돌려받습니다.
- CMake가 `cargo ndk`를 커스텀 빌드 타겟으로 호출해서 ABI별 `.a` 정적
  라이브러리를 만들고, `SYXG50Bridge.cpp`와 함께 링크해 `libsyxg50-jni.so` 하나로
  만듭니다(`app/src/main/cpp/CMakeLists.txt`의 `build_madaha_core` 타겟 참고).

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

### 엔진별 볼륨 게인 (v1.3)
원본 코어 출력 레벨이 엔진마다 달라 체감 음량을 맞춰뒀습니다 — 값을 바꾸고
싶다면 각 브리지 파일에서 찾을 수 있습니다:
- `SC55Bridge.cpp`의 `sampleCallback()` 안 `kGain`(현재 2.4)
- `MuntBridge.cpp`의 `synthThreadLoop()` 안 `kGain`(현재 2.2)
- `FluidBridge.cpp`의 `nativeInit()` 안 `fluid_settings_setnum(..., "synth.gain", ...)`(현재 0.6)
- `madaha/src/ffi.rs`의 `madaha_init` 안 `GainSink::new(raw_sink, 0.7, true)`
  (게인 0.7 + tanh 소프트클립)

### AAudio 스트림 자동 복구 (v1.9)
블루투스 통화(A2DP → SCO → A2DP)나 이어폰 탈착처럼 출력 경로가 바뀌면 AAudio가
스트림을 `AAUDIO_STREAM_STATE_DISCONNECTED`로 만들고 데이터 콜백을 영영 멈춥니다.
에러 콜백이 없으면 앱은 이를 알 방법이 없어서 "재생 중인데 무음" 상태가 됩니다.
다섯 엔진 브리지(`SC55Bridge`, `MuntBridge`, `FluidBridge`, `GearmulatorBridge`,
`MU2000Bridge`)가 공용 헤더 `cpp/AAudioRecover.h`를 씁니다.

- 빌더에 `AAudioStreamBuilder_setErrorCallback(b, AAudioRecover::onError, &recover)`를
  등록합니다. 콜백 안에서는 스트림을 닫지 않고(교착) 복구 "요청"만 겁니다.
- 스트림을 start한 직후 `recover.arm(&stream, opener)`를 부르면 감시 스레드가 500ms마다
  확인해서, 상태가 DISCONNECTED이거나 STARTED인데 2초 넘게 `getFramesRead`가 늘지 않으면
  (재시작 간격 3초 이상) 스트림을 닫고 **처음 열렸던 레이트 그대로**(엔진 내부
  레이트/리샘플 설정이 유효하도록) 다시 열어 start 합니다. `opener`는 각 브리지가 같은
  조건으로 스트림을 여는 함수입니다.
- `nativeStop`/`nativeTerm`/`stopAAudio`에서는 스트림을 stop/close하기 **전에**
  `recover.disarm()`을 먼저 부릅니다(감시 스레드를 합류시켜야 안전).
- Kotlin에서는 `IEngine.restartAudio()`(→ 각 엔진 `nativeRestartAudio()`)로 "지금 한 번 더
  다시 열기"를 요청할 수 있고, `MidiPlayerPanel`이 오디오 포커스를 되찾을 때 부릅니다.

### LCD 렌더링 — 88emu / S-MU2000 (v1.9)
두 엔진은 모두 원본 펌웨어를 실행하는 LLE라서 LCD 내용은 펌웨어가 컨트롤러에 써 넣는
값입니다. 앱은 그 **원시 데이터만** 꺼내서 그립니다.

```
렌더 스레드(약 33ms마다, 수십~수천 바이트 복사, 뮤텍스)
     │  내용이 바뀐 경우에만 변경 카운터(seq) 증가
     ▼
JNI nativeGetLcdSize / nativeGetLcdSeq / nativeGetLcdFrame(bitmap)   ← Kotlin 백그라운드 스레드
     │  원시 데이터 → RGBA_8888 픽셀 합성 (Lcd88Renderer / Lcd2000Renderer, 순수 C++)
     ▼
LcdFramePump(~30fps, seq가 같으면 생략, 비트맵 3장 순환) → LcdView
```

- **88emu**: `88lib/c_interface.h`에 추가한 `emu88_get_display_raw()`가 `DisplaySnapshot`
  (HD44780 DDRAM/CGRAM, 또는 SC-8850·MT-32/CM의 도트 그리드)를 복사합니다. 기종별
  모양은 `Lcd88Renderer`가 정합니다(`Lcd88Look`: SC-55/88 유리 741×268, SC-8850
  640×256, MT-32/CM 연두색). `HardwareDevice`의 스냅샷 갱신은 렌더된 시간 기준
  약 60Hz로 제한해 두었습니다(원래는 렌더 청크마다 할당 포함 갱신).
- **S-MU2000**: `MU2000Engine::lcdSnapshot()`이 `lcd_render()`의 2행×24칸×8줄 도트를
  복사합니다. **`lcd_render()`는 에뮬레이션 스레드에서만** 호출해야 하므로 렌더 루프에서
  샘플 약 1470개(44100Hz/30)마다 부릅니다. HD44780 문자 ROM(CGROM)이 없으면
  `render()`가 전부 0을 돌려주므로, `hd44780u_b04.bin`이 없을 때는 내장
  폰트(`lcd_cgrom_fallback.h`)로 CGROM을 채웁니다.
- 새 기종/엔진에 붙일 때는 같은 3함수 계약(`Size`는 `(w<<16)|h` 또는 0, `Seq`, `Frame`)을
  구현하고 `LcdFramePump`에 람다로 넘기면 됩니다. 픽셀 포맷은 `ARGB_8888` 비트맵 =
  RGBA 바이트 순서이며 모든 알파는 0xFF로 씁니다.

### 가상 LCD — SoundFont / SC-8820 / MT-32(Munt) (v1.9)
LCD 하드웨어가 없는 엔진이나 1줄 문자 LCD는 별도 네이티브 라이브러리 `vlcd-jni`(`cpp/vlcd/VirtualLcd.cpp`)가
기존 두 렌더러(`Lcd2000Renderer`, `Lcd88Renderer`)를 그대로 재사용해 그립니다. 엔진 의존성이 없는 순수 계산
라이브러리입니다.

- **MU2000 스타일 패널**: Kotlin `VirtualPanelState`가 MIDI에서 파트별 값(레벨, 프로그램, 뱅크, CC7/10/11/91/93/94)을
  추적하고(`onMidi`는 MIDI 스레드, `tick`/`render`는 LCD 스레드; 표시 파트는 첫 노트에서 정해 고정하고 12초 넘게 조용할 때만 이동,
  GM/GS/XG 리셋 SysEx에서 초기화), `VirtualLcd.renderPanel(bitmap, params[32], name[8])`이
  이를 2×24칸 도트(384B)로 구성해 `lcd2000Render`로 그립니다. 도트 규약은 `Lcd2000Renderer.h` 주석 참고
  (위 면 0~16칸 = 레벨미터 18개 + 문자 8자, 아래 면 17~22칸, 23칸 = 제어 비트). `tick()`은 화면에 보이는 값이
  달라졌을 때만 카운터를 올려 `LcdFramePump`가 변화 없는 프레임을 건너뜁니다.
- **문자 LCD**: `TextLcdSource`(문자열이 바뀔 때만 갱신) → `VirtualLcd.renderText` → 120×9 도트 → `Lcd88Renderer`
  (`Lcd88Look::CmGreen`, 1000×92).
- 연결: `SoundFontEngine.panel`, `GearmulatorEngine.panel`(SC-8820만 사용)이 각 엔진의 `dispatchMidi`에서 `onMidi`를
  부르고, `MainActivity`가 엔진마다 `LcdFramePump` 하나로 LCD를 갱신합니다. 새 엔진을 추가할 때는 LCD가 있으면 JNI 3함수
  (`Size/Seq/Frame`)를, 없으면 `VirtualPanelState`를 쓰면 됩니다.
### MIDI 파일 플레이어의 상태 전달 (v1.9)
`MidiFilePlayer.onStateChanged`가 재생/일시정지/정지/자연 종료마다 호출되어
`MidiPlayerPanel`이 MediaSession 재생 상태를 갱신합니다 — UI 타이머(화면이 꺼지면
멈춤)에 의존하지 않습니다. 블루투스 재생/일시정지 토글 키는 세션 기록이 아니라
실제 플레이어 상태로 판단합니다. 마지막으로 재생한 곡은 SharedPreferences(`midi_last_file`)에
저장되어 플레이어 시작 시 그 곡이 선택됩니다.

### 벤더링 소스 갱신 시 주의 (v1.9)
- 앱에 들어 있는 gearmulator 88lib는 업스트림 `6ef301a`(2026-09-26), S-MU2000 코어는
  `b26bfa5`(2026-10-03) 시점입니다. 우리가 직접 수정한 업스트림 파일은 88lib의
  `c_interface.h/.cpp`, `hardwareDevice.h/.cpp`뿐이니 88lib를 갱신하면 이 네 파일의 변경을
  다시 적용해야 합니다.
- **헤더가 바뀌는 벤더링 소스를 갱신한 뒤에는 반드시 전체 재빌드(또는 파일 수정 시각 갱신)를
  하세요.** `git archive`/`git checkout`으로 가져온 파일은 수정 시각이 커밋 시각이라, 이미 만들어
  둔 오브젝트보다 "오래된" 것으로 보여 증분 빌드(Ninja)가 의존하는 소스를 다시 컴파일하지
  않을 수 있습니다. 클래스 레이아웃이 바뀐 경우(예: `swp30.h`에 멤버 추가 → `sizeof(mu2000)`
  증가) 그렇게 섞인 빌드는 한쪽은 옛 크기로 객체를 만들고 다른 쪽 생성자는 새 크기로
  초기화하여 `mu2000::mu2000()`의 `memset`에서 `SIGSEGV(SEGV_ACCERR)`로 죽습니다(10/3에 실제로
  겪음). 가장 간단한 방법은 가져온 폴더 전체의 `LastWriteTime`을 현재로 갱신하거나
  `gradlew clean` 후 빌드하는 것입니다.
- 크래시 기록은 PC 연결 없이도 기기에 남아 있습니다: `adb shell dumpsys dropbox --print
  data_app_native_crash`(백트레이스와 시각), `adb logcat -b crash -d`.
