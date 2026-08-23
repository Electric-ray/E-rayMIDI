# E-ray MIDI

Roland SC-55 / MT-32 / SoundFont(SF2) / **S-YXG50(YAMAHA XG)** 네 가지 재생 엔진과,
RTP-MIDI(WiFi) / USB 시리얼 / USB MIDI 주변장치(peripheral) 세 가지 연결 방식을
하나로 통합한 Android MIDI 재생 앱입니다.

원래는 [nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55)의 Android 포팅으로
시작했지만, [munt](https://github.com/munt/munt)(MT-32/CM-32L), FluidSynth(SF2),
[madaha](https://github.com/madaha-dev/madaha)(S-YXG50) 코어를 함께 통합하면서
단일 엔진 포팅 프로젝트를 넘어선 4-엔진 레트로 MIDI 모듈이 되었습니다.

> **v1.4 패치 노트**: USB MIDI 주변장치 안정성 개선, SC-55 LCD 화면 깨짐(프레임버퍼
> data race) 수정, MT-32 모드에 실제 LCD 그래픽 디스플레이 추가, 앱 아이콘 교체.
> 자세한 변경 내역은 [아래 "v1.4에서 달라진 점"](#v14에서-달라진-점) 참고.
>
> **v1.3 패치 노트**: 이 버전에서 S-YXG50 엔진이 새로 추가되고, SoundFont 엔진이
> TinySoundFont → FluidSynth로 교체되고, GUI가 가로형 레트로 랙마운트 스타일로
> 전면 개편됐습니다. 자세한 변경 내역은 [아래 "v1.3에서 달라진 점"](#v13에서-달라진-점)
> 섹션을 참고하세요.

---

## 무엇을 할 수 있나요

DOS PC(또는 다른 MIDI 소스)에서 나가는 MIDI 신호를 안드로이드 기기로 가져와서,
네 가지 레트로 신시사이저 중 하나로 실제 소리를 냅니다.

```
                 ┌─ RTP-MIDI (WiFi, ESP32 경유)
DOS PC/호스트 ───┼─ USB 시리얼 (rs232 to USB 널모뎀케이블)
                 └─ USB MIDI 주변장치 (케이블로 PC와 직결, 안드로이드가 표준 MIDI 장치로 인식됨)
                                │
                                ▼
                     E-ray MIDI (안드로이드 앱)
                                │
        ┌──────────────┬───────┴───────┬──────────────┐
        ▼              ▼               ▼              ▼
   SC-55 엔진      MT-32 엔진     SoundFont 엔진   S-YXG50 엔진
 (Nuked-SC55 코어)  (munt 코어)   (FluidSynth 2.6)  (madaha 코어)
   ROM 파일 필요    ROM 파일 필요   .sf2 파일 필요    ROM 파일 필요
        │              │               │              │
        └──────────────┴───────┬───────┴──────────────┘
                                ▼
                         오디오 출력 (AAudio)
```

ESP32 쪽 RTP-MIDI/USB 브리지 펌웨어는 별도 저장소
[Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)에서 관리합니다.

---

## 실행 환경 (구동 가능한 안드로이드 사양)

| 항목 | 요구 사항 | 비고 |
|---|---|---|
| **최소 Android 버전** | **Android 10 (API 29)** 이상 | `minSdk = 29`. AAudio 자체는 API 26+부터 있지만, 이 앱은 API 29 기준으로 개발/검증됨 |
| **타겟 Android 버전** | Android 15 (API 36) | `targetSdk = 36` |
| **CPU 아키텍처** | **arm64-v8a (64비트 ARM) 전용** | `abiFilters`가 arm64-v8a만 포함 — 32비트 전용 기기(armeabi-v7a만 지원하는 구형 기기)나 x86/x86_64 기기(대부분의 에뮬레이터 포함)에서는 **네이티브 라이브러리가 로드되지 않아 실행되지 않습니다** |
| **오디오 API** | AAudio (`AAUDIO_PERFORMANCE_MODE_LOW_LATENCY`) | Android 8.0(API 26)부터 제공되는 저지연 오디오 API. API 29 이상에서 이미 충분히 성숙한 상태로 동작 |
| **RAM** | 최소 3GB 권장 | S-YXG50 엔진 내부에 8MB 크기의 사전계산 피치 테이블이 있고, FluidSynth·SoundFont 샘플 데이터도 메모리에 올라감 — 저사양 기기에서는 여유 있게 |
| **저장공간** | ROM/SF2 파일 크기에 따라 다름 | 앱 자체는 가볍지만(APK ~수십MB), SF2 사운드폰트는 파일에 따라 수백MB까지도 가능 |
| **테스트 기기** | LG Velvet (LG-G910N), Android 10 (API 29) | 전체 개발/디버깅이 이 기기에서 이뤄짐. 다른 기기에서도 동작해야 하지만 폭넓게 테스트되지는 않음 |

### 왜 arm64-v8a 전용인가?
- S-YXG50 엔진(Rust)과 FluidSynth 프리빌트 바이너리는 여러 ABI로 빌드/이식 가능하지만,
  현재 `build.gradle.kts`가 **arm64-v8a만** 타겟하도록 고정되어 있습니다(테스트 기기가
  arm64 전용이라 다른 ABI는 실기기 검증을 못 했기 때문). armeabi-v7a/x86/x86_64를
  지원하려면 `abiFilters`를 넓히고 각 ABI별로 실제 빌드가 되는지 검증이 필요합니다
  (S-YXG50 쪽은 `cargo ndk`로 4개 ABI 모두 크로스컴파일까지는 확인됐지만, 실기기
  구동 검증은 arm64-v8a에서만 이뤄졌습니다).

### 왜 API 29 이상인가?
- AAudio의 `AAUDIO_PERFORMANCE_MODE_LOW_LATENCY`, `setBufferSizeInFrames` 등 이 앱이
  쓰는 저지연 오디오 기능들은 API 26부터 제공되지만, `minSdk`를 테스트 기기(Android 10)
  기준으로 잡아뒀습니다. 이론상 API 26~28에서도 빌드/동작할 가능성이 있으나 검증되지
  않았습니다 — 낮추려면 `minSdk`를 조정하고 직접 검증이 필요합니다.

---

## v1.3에서 달라진 점

기존 3-엔진(SC-55/MT-32/SoundFont) 버전 대비 이번 패치에서 바뀐 부분을 정리합니다.

### 1. 새 엔진 추가 — S-YXG50 (YAMAHA XG)
[madaha](https://github.com/madaha-dev/madaha) (Rust로 작성된 S-YXG50 소프트웨어
신시사이저)를 JNI로 통합했습니다. `madaha_core`라는 Rust `staticlib`으로 빌드해서
C++ JNI 브리지(`SYXG50Bridge.cpp`)가 감싸는 구조입니다.

- ROM 파일 2개 필요: `sxgbin41.tbl`, `sxgwave4.tbl` → `/sdcard/Download/rom_s-yxg50/`
- Rust 소스는 `app/src/main/cpp/madaha/`에 그대로 벤더링(vendoring)되어 있습니다
  (원본 저장소를 git submodule이 아니라 통째로 복사해 넣은 형태 — 이 저장소 자체가
  아직 git 이력을 안 쓰기 때문에 이렇게 결정했습니다. 원본 대비 수정 사항은 아래
  "2. madaha 자체 버그 수정" 참고).
- 빌드 시 `cargo-ndk`로 4개 ABI(arm64-v8a/armeabi-v7a/x86_64/x86)를 크로스컴파일해서
  `.a` 정적 라이브러리를 만들고, CMake가 `SYXG50Bridge.cpp`와 함께 링크합니다
  (사전 준비물은 [BUILDING.md](BUILDING.md) 참고).

### 2. madaha 자체의 버그 수정 (업스트림에 기여 가능한 발견들)
S-YXG50을 Android에 이식하는 과정에서 madaha 코드베이스 자체에 있던 버그를 여러
개 발견하고 고쳤습니다. madaha는 아직 활발히 개발 중인 프로젝트라, 아래 항목들은
업스트림에도 유효한 수정일 가능성이 높습니다(PR을 아직 안 올렸다면 고려해볼 것):

- **`AudioSink`/`EffectProcessor` 트레잇에 `Send` 바운드 누락** — Android FFI에서
  멀티스레드로 쓰려니 컴파일이 안 됐던 문제. 두 트레잇 정의에 `: Send` 추가.
- **`ReverbEffect::init_delay` 필드가 스택에 256KB짜리 배열로 직접 박혀있던 문제** —
  다른 이펙트들(`ring: Box<[f32; 131072]>`)과 다르게 이 필드만 `Box`로 감싸지 않고
  있어서, `ReverbEffect::new()` 호출 시 스택에 256KB가 한꺼번에 올라감. 실기기(작은
  스택을 쓰는 AAudio 실시간 콜백 스레드)에서 SIGSEGV(스택 오버플로우)로 확인됨.
- **`Box::new([0.0; N])` 패턴이 `opt-level="z"`에서 스택 임시값을 거쳐가는 문제** —
  Rust의 `Box::new`는 값이 힙으로 바로 들어간다는 보장이 없어서, 크기 최적화
  빌드에서는 배열 리터럴이 스택에 먼저 만들어진 뒤 복사될 수 있음. 리버브 말고도
  코러스/베리에이션 이펙트 등 총 11곳에서 같은 패턴이 있었고, `vec![0.0; N]` 기반의
  안전한 힙 직접할당 헬퍼(`zeroed_box`)로 전부 교체.
- **`Engine` 구조체 안의 `note_cent_table: [[[f32;128];128];128]`(정확히 8MB)가
  값으로 스택을 거쳐가던 문제** — `madaha_init`을 32MB 스택을 가진 전용 스레드에서
  실행하고, 완성된 `Instance`를 그 스레드 안에서 즉시 `Box`로 힙에 옮긴 뒤 포인터만
  돌려받는 방식으로 해결(호출 스레드 쪽 스택은 절대 8MB짜리 값을 직접 들지 않음).
- **일반 드럼(SFX 아닌) PCM이 로드는 되는데 실제 재생 경로로 연결이 안 되던 문제** —
  `libmadaha::yxg50::drum_setup::DrumSetupEntry::set_wave()`가 PCM을 정상적으로
  추출해두는데, 앱 레벨의 `impl From<&YXG50DrumSetupEntry> for SampleMeta`
  변환에서 그 값을 복사하지 않고 항상 `None`으로 버리고 있었음.
- **`sfx_key()` 함수가 결과를 버리고 항상 `None`을 반환하던 문제** — SFX 계열
  드럼/악기의 `Key::new(...)` 호출 결과에 `return`이 빠져 있어서, `drum_key_type==0`
  경로(다수의 드럼 노트가 여기 해당)가 사실상 항상 비활성 상태였음.
- **`Part::set_program()`이 초기화 시점 1회 말고는 코드베이스 어디서도 호출되지
  않던 문제(가장 영향이 컸던 버그)** — Bank Select(CC0/CC32)/Program Change
  메시지가 RAM에 값은 기록되지만, 실제 재생에 쓰이는 `program_entry`(=어떤
  `Program`/샘플 데이터를 쓸지)는 절대 갱신되지 않아서, 모든 파트가 곡 시작부터
  끝까지 초기 기본 악기(멜로디는 Acoustic Grand Piano, 드럼 채널은 GS 드럼뱅크
  킷0)에 고정되어 있었음. `Engine::on_program_change`에서 RAM에 기록된 최신
  bank_msb/lsb를 다시 읽어 `set_program()`을 호출하도록 수정.

> ⚠️ **알려진 한계**: 위 수정들을 적용한 뒤에도, 일부 곡에서 드럼 노트가 여전히
> 의도한 악기와 다르게(또는 무음으로) 재생되는 경우가 남아있습니다. 근본 원인이
> madaha 코어의 더 깊은 부분(보이스 라우팅/얼터 그룹 처리 등 추정)에 있는 것으로
> 보이며, 이번 패치에서는 **madaha 쪽 개선을 기다리는 것으로 보류**했습니다.
> madaha가 업데이트되면 `app/src/main/cpp/madaha/`를 새 버전으로 교체하는 방식으로
> 반영할 계획입니다.

### 3. SoundFont 엔진 교체 — TinySoundFont → FluidSynth 2.6.0
[TinySoundFont](https://github.com/schellingb/TinySoundFont)(헤더 하나짜리 미니
신스)를 [FluidSynth](https://www.fluidsynth.org/) 2.6.0으로 교체했습니다. 더 정확한
SF2 해석, 보이스 관리, 리버브/코러스 내장 이펙트를 씁니다.

- FluidSynth 공식 GitHub Release의 **Android 프리빌트 바이너리**를 그대로 가져다
  씁니다 (`fluidsynth-v2.6.0-android24.zip`). 예전에는 FluidSynth를 Android에
  올리려면 `glib`까지 직접 크로스컴파일해야 해서 매우 번거로웠는데, 2.5.0부터
  glib 없는(C++11 OSAL) 공식 Android 배포본이 나와서 훨씬 간단해졌습니다.
- `app/src/main/cpp/fluidsynth/`에 `include/`(헤더)와 `lib/<abi>/`(프리빌트
  `.so` 10개: libfluidsynth, libfluidsynth-assetloader, libFLAC, liboboe, libogg,
  libopus, libsndfile, libvorbis, libvorbisenc, libvorbisfile)가 그대로 커밋되어
  있습니다.
- 새 브리지 `FluidBridge.cpp`가 `TsfBridge.cpp`를 대체했습니다(JNI 클래스명은
  기존과 동일하게 `SoundFontEngine`을 유지해서 Kotlin 쪽은 변경 없음).

### 4. 오디오 파이프라인 구조 통일 — 전용 렌더 스레드 + 링버퍼
SC-55/MT-32 엔진은 원래부터 "MIDI/렌더링은 전용 스레드가 미리 해두고, AAudio
콜백은 링버퍼에서 꺼내기만 한다"는 구조였는데(AAudio 콜백 안에서 무거운 연산을
직접 하면 노트/화음이 많은 순간 콜백 데드라인을 놓쳐 끊김+노이즈가 남), 새로 추가한
SoundFont(FluidSynth)/S-YXG50 엔진도 처음부터 동일한 구조로 만들었습니다. 네 엔진
모두 다음 패턴을 공유합니다:

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

### 5. 출력 라우팅 버그 수정 — AAudio EXCLUSIVE → SHARED 우선
4개 엔진 모두 AAudio 스트림을 열 때 `AAUDIO_SHARING_MODE_EXCLUSIVE`를 먼저 시도하고
있었는데(지연을 낮추려는 의도), 이 모드가 기기 내장 스피커의 저지연 경로에만
고정되고 **유선/블루투스 이어폰이나 외부 스피커로 출력 라우팅이 안 되는 경우가
실기기에서 확인**됐습니다. 위 4번 항목에서 이미 지터 문제를 렌더 스레드+링버퍼
구조로 해결했으므로, EXCLUSIVE의 지연 이점보다 라우팅 정상 동작이 훨씬 중요하다고
판단해 **SHARED를 우선 시도**하도록 네 엔진 전부 통일했습니다(SHARED가 실패하는
드문 경우에만 EXCLUSIVE로 폴백).

### 6. 볼륨 밸런싱
엔진마다 원본 코어의 출력 레벨이 제각각이라(SC-55/MT-32 쪽이 SoundFont/S-YXG50
대비 실기기에서 확연히 작게 들림) 체감 음량을 맞췄습니다:
- SC-55: 코어 원본 출력에 2.4배 게인(클리핑 방지 클램프 포함)
- MT-32: 코어 원본 출력에 2.2배 게인(클리핑 방지 클램프 포함)
- SoundFont: FluidSynth `synth.gain` 설정을 0.6으로
- S-YXG50: `GainSink`로 0.7배 게인 + tanh 소프트클립 (madaha에 이미 있던 기능을
  연결만 하면 됐음 — 처음엔 하드 클램프만 하고 있어서 도입부처럼 노트가 몰리는
  구간에서 디지털 클리핑 노이즈가 들렸던 문제도 같이 해결됨)

### 7. GUI 전면 개편 — 가로형 레트로 랙마운트 스타일
기존 세로형 Material Design 레이아웃을 버리고, Roland Sound Canvas / YAMAHA MU100
같은 실제 랙마운트 하드웨어 느낌의 **가로형(landscape) GUI**로 새로 만들었습니다.

- 화면 방향을 `sensorLandscape`로 설정 — 가로 정방향/역방향 두 방향 다 센서에
  맞춰 회전(세로 모드는 애초에 지원 안 함, 레이아웃이 가로 전용으로 설계됨)
- 왼쪽 "MODE" 패널: 4개 엔진을 LED 인디케이터 스타일 토글 버튼으로 선택
- 오른쪽 "LINK" 패널: 3개 연결 방식을 같은 스타일로 선택 + 초기화/연결 버튼
- 가운데: LCD(SC-55)/LED 채널 패널(MT-32·SoundFont·S-YXG50 — 채널별 LED +
  악기명/뱅크·프로그램 텍스트를 16채널까지 스크롤 표시, 세 엔진 모두 통일된
  형태) + 상태 로그 + ROM 상태를 좌우로 배치
- 볼륨 조절 등 실제로 기능하지 않는 장식용 노브는 넣지 않고, 실제로 쓰는 버튼만
  구성 (앱 자체의 오디오 게인 조절 UI는 없음 — 기기 볼륨 버튼을 사용)

### 8. 기타
- Rust `madaha_core` 빌드 최적화 프로파일을 `opt-level="z"`(크기 우선)에서
  `opt-level=3`(속도 우선)으로 변경 — 실시간 오디오 DSP에는 크기보다 속도가
  훨씬 중요해서, 노트/채널이 많을 때의 끊김이 크게 개선됨.

---

## v1.4에서 달라진 점

### 1. SC-55 LCD 화면 깨짐 수정 (data race, 근본 원인 2건)
Galaxy A50(Android 11)·일부 구형 Android 10 기기에서 "LCD 화면이 여러 장 겹쳐
보인다"/체크무늬 노이즈로 보이던 문제를 실제 코드 레벨에서 확인하고 고쳤습니다.
빠른 테스트 기기(LG Velvet)에서는 운 좋게 안 걸렸을 뿐, 두 개의 진짜 data race가
있었습니다.

- **네이티브: `lcd.buffer`(픽셀 프레임버퍼)가 뮤텍스로 보호되지 않던 문제** —
  벤더링한 nuked-sc55의 `lcd.mutex`는 `LCD_C`/`LCD_CG` 등 MCU가 쓰는 LCD *상태*만
  보호하고 있었고, `LCD_Render()`가 실제 픽셀을 그리는 구간(폰트 렌더링, 레벨미터
  등 수백 줄)은 그 뮤텍스 밖에서 실행되고 있었습니다. `nativeGetLcdFrame()`은
  "뮤텍스를 잡았으니 안전하다"고 가정하고 이 `lcd.buffer`를 그대로 memcpy해갔는데,
  보호 대상과 실제 동시접근 대상이 서로 달라서 렌더링 중간(절반만 그려진) 프레임을
  읽어가는 진짜 레이스였습니다. `LCD_Render()`의 픽셀 쓰기 구간 전체를 같은
  뮤텍스로 감싸서 해결했습니다.
- **Kotlin: 화면에 그려지고 있는 Bitmap을 백그라운드 스레드가 동시에 덮어쓰던
  문제** — 트리플 버퍼링이 "버퍼 3개만큼 순서가 지났으니 안전하겠지"라는 타이머
  기준 추측에만 의존하고 있어서, `onDraw()`가 그 여유 시간보다 오래 걸리는 느린
  기기에서는 화면에 그려지는 중인 Bitmap 인스턴스를 렌더 스레드가 그대로
  덮어쓸 수 있었습니다. `LcdView`가 "지금 세팅된 Bitmap"과 "지금 `onDraw()`가
  실제로 읽고 있는 Bitmap"을 모두 추적하도록 하고, 렌더 스레드는 그 어느 쪽도
  아닌 버퍼만 골라 쓰도록 고쳤습니다(셋 다 사용 중이면 이번 프레임은 스킵 —
  픽셀이 찢어지는 것보다 프레임 하나 스킵이 훨씬 낫습니다).

### 2. USB MIDI 주변장치 안정성 개선
- **재협상 후 죽은 포트를 붙잡는 문제 완화**: `MidiManager.DeviceCallback`에
  `onDeviceRemoved`가 아예 없어서, 장치가 재협상(사라짐→다시 나타남)되어도
  이전 연결 참조가 정리되지 않고 새 연결과 충돌할 수 있었습니다. 장치 제거 시
  참조를 정리하도록 수정하고, "🔄 초기화" 버튼을 누르면 USB MIDI 연결 자체를
  강제로 닫았다 다시 여는 수동 복구 경로도 추가했습니다(안드로이드가 재협상
  이벤트 자체를 못 쏴주는 경우까지는 소프트웨어로 자동 감지가 안 되어서, 사용자가
  직접 복구할 수 있는 버튼이 필요했습니다).
- **빠른 곡에서 USB MIDI만 끊기고 노이즈가 섞이던 문제 수정**: USB는 RTP-MIDI(WiFi)
  보다 훨씬 빨라서 화음/아르페지오가 거의 동시에 `onSend()` 콜백(안드로이드가
  `THREAD_PRIORITY_URGENT_AUDIO`로 돌리는 스레드)으로 몰려 들어올 수 있었고, 그
  순간 이 급한 우선순위 스레드가 파싱+엔진 디스패치까지 다 처리하느라 CPU를 오래
  붙잡아 오디오 렌더 스레드와 경합했습니다. `onSend()`는 큐에 넣고 즉시 리턴만
  하게 하고, 실제 처리는 한 단계 낮은 우선순위의 별도 드레인 스레드로 분리했습니다.

### 3. MT-32 모드에 실제 LCD 그래픽 디스플레이 추가
mt32emu가 자체적으로 에뮬레이션하는 실제 MT-32/CM-32L LCD 텍스트(`Synth::
getDisplayState()` — PC용 munt가 보여주는 것과 동일: 파트 활성 표시, Rhythm,
마스터 볼륨)를 가져와서, SC-55처럼 **그래픽 도트매트릭스 LCD**로 그립니다
(`MuntLcdView`). mt32emu는 픽셀이 아니라 텍스트만 주기 때문에, 커스텀 폰트
데이터를 직접 만드는 대신 더 안전한 방식을 썼습니다:
- 아주 작은 오프스크린 비트맵에 모노스페이스 폰트로 문자를 한 칸씩 고정폭으로
  그려서(칸마다 중앙 정렬 — 문자 폭이 흔들려 텍스트 위치가 밀리는 문제 방지),
- 그 픽셀 하나하나를 화면 배율에 맞춰 개별 사각형으로 그리되 아주 약간의 간격을
  둬서, 실제 LCD의 도트 사이 간격 느낌을 살렸습니다.
- 종횡비를 유지하는 단일 배율로 계산하고 뷰 중앙에 정렬해서, 가로만 과하게
  늘어나 보이던 문제도 해결했습니다.

### 4. 앱 아이콘 교체
"전기가오리" 마스코트 아이콘으로 교체했습니다.

### 5. SC-55 노트 지속시간 이슈 조사 (미해결, 조사 기록만)
일부 곡에서 특정 채널/패치의 노트가 의도한 길이보다 길게 늘어지는 제보가 있어
깊이 조사했습니다. MIDI 이벤트가 Kotlin↔JNI 큐를 거쳐 에뮬레이터 코어에 도달하는
경로는 진단 로그로 직접 확인해 지연·순서 문제가 없음을 확인했고, 게인/리버브
증폭 때문도 아님을 확인했습니다(진단용으로 게인을 1.0까지 낮춰봐도 증상 동일).
벤더링한 nuked-sc55 소스도 최신 upstream과 완전히 동일합니다 — 즉 원인이 우리
쪽 브리지/파이프라인이 아니라 코어의 사이클 단위 MCU 에뮬레이션 내부에 있을
가능성이 높다는 결론까지만 내고, 조사는 보류했습니다(아래 "알려진 이슈" 참고).

---

## 주요 기능

### 연결 방식 (셋 중 선택)
- **📡 RTP-MIDI**: ESP32가 WiFi AppleMIDI로 중계. 케이블 없이 무선으로 연결.
- **🔌 USB 시리얼**: DOS PC와 시리얼널모뎀 케이블로 직결.
- **🎹 USB MIDI기기**: 안드로이드 기기 자체를 USB MIDI 주변장치로 노출 — Windows 등
  PC에 케이블로 연결하면 표준 MIDI 입력 장치로 인식되어, SoftMPU 같은 DOS MIDI 드라이버나
  DAW가 직접 이 폰으로 MIDI를 보낼 수 있습니다. (`MidiManager`로 시스템이 제공하는
  USB peripheral 포트를 직접 열어 연결)

### 재생 엔진 (넷 중 선택)
- **SC-55**: Nuked-SC55 코어를 그대로 이식, MCU 사이클 단위 에뮬레이션. 실제 LCD
  컨트롤러 동작을 픽셀 단위로 재현 (파라미터 레벨미터 애니메이션 포함).
- **MT-32**: munt 코어 이식. 실제 mt32emu LCD 텍스트를 SC-55처럼 그래픽 도트매트릭스
  스타일로 재현(파트별 LED + 패치명 + LCD 디스플레이). GS 전용으로 만들어진 곡을
  재생할 때 발생하는 무음 문제에 대한 자동 대응 포함 (아래 "알려진 이슈와 대응" 참고).
- **SoundFont**: FluidSynth 2.6.0 기반 `.sf2` 재생. 채널별 프리셋명 실시간 표시.
- **S-YXG50**: madaha(Rust) 기반 YAMAHA XG 소프트웨어 신시사이저. 채널별 뱅크/
  프로그램 번호 실시간 표시(SF2와 달리 ROM에 사람이 읽는 악기명 테이블이 없음).

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
- **(v1.3)** 네 엔진 모두 AAudio 출력이 유선/블루투스 이어폰·외부 스피커로 정상
  라우팅되도록 수정 (위 "v1.3에서 달라진 점 - 5" 참고)
- **(v1.3)** SoundFont/S-YXG50 엔진에서 노트/채널이 많을 때 소리가 끊기고 노이즈가
  섞이던 문제 수정 (렌더 스레드+링버퍼 구조 도입, "v1.3에서 달라진 점 - 4" 참고)
- **(v1.4)** SC-55 LCD 화면이 깨지거나 여러 장 겹쳐 보이던 문제(프레임버퍼 data
  race) 수정
- **(v1.4)** USB MIDI기기 모드 재협상 시 죽은 포트를 붙잡던 문제 완화, 빠른 곡에서
  USB MIDI만 끊기던 문제 수정

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
- MT-32는 실제 하드웨어 관례상 MIDI 채널 2~9에 8개 파트, 채널 10에 리듬이
  고정 배정되며 **채널 1은 원래 어떤 파트에도 할당되지 않습니다** — 앱의 LED
  패널이 CH2부터 시작하는 것은 버그가 아니라 이 관례를 그대로 반영한 것입니다.
- SoundFont 모드는 GM 표준까지만 지원하며 Roland GS 전용 확장은 표현하지 못합니다.
- SC-55 LCD 렌더링에 미세한 깜빡임이 있을 수 있습니다(화면이 깨지거나 겹쳐 보이는
  더 심각한 증상은 v1.4에서 data race를 고쳐서 해결됨 — 위 "v1.4에서 달라진 점" 참고).
- **(v1.3) S-YXG50 모드에서 일부 곡의 드럼 노트가 여전히 의도한 악기와 다르게(또는
  무음으로) 재생될 수 있습니다.** madaha 자체의 버그 여러 개를 찾아 고쳤지만
  (위 "v1.3에서 달라진 점 - 2" 참고) 완전히 해결되지는 않았고, madaha 코어의 더
  깊은 부분에 원인이 있는 것으로 추정되어 업스트림 개선을 기다리며 보류 중입니다.
- **USB MIDI기기 모드에서, 아주 드물게 연결된 상태로 표시되는데도 소리가 안 나올
  수 있습니다.** 안드로이드 기기의 USB MIDI 주변장치(peripheral) 게이트웨이가
  스스로 재협상되면서 생기는 것으로 추정되는 플랫폼 레벨 이슈입니다(리눅스/MiSTer
  등 다른 호스트에서는 이 증상이 보고되지 않음). v1.4에서 재협상 시 정리 로직을
  보강해 빈도가 줄었을 것으로 보이나, 완전히 사라졌는지는 미확인입니다. 이런 증상이
  나오면 **"🔄 초기화" 버튼을 눌러보세요** — USB MIDI 연결을 강제로 닫았다 다시 엽니다.
- **SC-55 모드에서 일부 곡의 특정 채널/패치 노트가 의도한 길이보다 길게 늘어져
  들릴 수 있습니다** (예: 8분음표+8분쉼표 패턴이 4분음표처럼 이어짐). MIDI 이벤트가
  Kotlin↔JNI 큐를 거쳐 에뮬레이터 코어(`PostMIDI`)에 도달하는 경로는 진단 로그로
  직접 확인해 지연·순서 문제가 없음을 확인했고, 게인/리버브 증폭 때문도 아님을
  확인했습니다. 벤더링한 nuked-sc55 소스도 최신 upstream과 완전히 동일합니다 —
  즉 원인이 우리 쪽 브리지/파이프라인이 아니라 **코어의 사이클 단위 MCU
  에뮬레이션 내부(보이스 릴리즈/얼터 그룹 처리 등 추정)에 있을 가능성이
  높습니다.** 재현 조건(특정 채널의 짧은 노트+긴 쉼표, 같은 코드 재발음 패턴)만
  파악된 상태로 조사를 보류했습니다.

---

## ROM / 사운드폰트 파일 배치

| 엔진 | 경로 |
|---|---|
| SC-55 | `/sdcard/Download/rom_sc55/` |
| MT-32 | `/sdcard/Download/rom_munt/` |
| SoundFont | `/sdcard/Download/soundfont/` (`.sf2` 파일) |
| S-YXG50 | `/sdcard/Download/rom_s-yxg50/` (`sxgbin41.tbl`, `sxgwave4.tbl`) |

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
- **(v1.3 추가)** S-YXG50(madaha) 빌드용:
  - Rust stable 1.85+ (edition 2024 지원 버전 — `rustup`으로 설치, nightly 불필요)
  - Android 타겟 4종: `rustup target add aarch64-linux-android armv7-linux-androideabi x86_64-linux-android i686-linux-android`
  - `cargo install cargo-ndk`
  - LLVM/libclang (일부 의존 크레이트의 bindgen 빌드용 — `winget install LLVM.LLVM` 등)
- FluidSynth/madaha 관련 사전 준비물과 빌드 순서는 [BUILDING.md](BUILDING.md)에
  자세히 정리되어 있습니다.

---

## 프로젝트 구조

```
android-app/
└── app/src/main/
    ├── java/com/example/nukedsc55/
    │   ├── IEngine.kt              # 4개 엔진 공통 인터페이스 (PartInfo 등 공용 타입 포함)
    │   ├── SC55Engine.kt           # Nuked-SC55 코어 어댑터
    │   ├── MuntEngine.kt           # munt(mt32emu) 코어 어댑터
    │   ├── SoundFontEngine.kt      # FluidSynth 어댑터
    │   ├── SYXG50Engine.kt         # madaha(S-YXG50) 어댑터
    │   ├── LcdView.kt              # SC-55 그래픽 LCD (네이티브 프레임버퍼 → Bitmap)
    │   ├── MuntLcdView.kt          # MT-32 그래픽 LCD (텍스트 → 도트매트릭스 렌더링)
    │   ├── RtpMidiSession.kt       # RTP-MIDI(AppleMIDI) 클라이언트
    │   ├── UsbMidiManager.kt       # USB 시리얼 입력 (usb-serial-for-android)
    │   ├── MidiStreamParser.kt     # Running-status/SysEx MIDI 바이트 파서
    │   ├── EngineRegistry.kt       # USB MIDI 주변장치 모드용 현재 활성 엔진 참조
    │   ├── UsbMidiDeviceService.kt # 안드로이드를 가상 MIDI 장치로 노출 (MidiDeviceService)
    │   └── MainActivity.kt         # 가로형 레트로 GUI, LED 채널 패널 공용 로직
    └── cpp/
        ├── SC55Bridge.cpp          # Nuked-SC55 JNI 브리지
        ├── MuntBridge.cpp          # munt(mt32emu) JNI 브리지
        ├── FluidBridge.cpp         # FluidSynth JNI 브리지
        ├── SYXG50Bridge.cpp        # madaha(Rust staticlib) JNI 브리지
        ├── munt/mt32emu/           # munt 코어 소스 (이식됨)
        ├── nuked-sc55/             # Nuked-SC55 코어 소스
        ├── fluidsynth/             # FluidSynth 공식 Android 프리빌트 (include/ + lib/<abi>/)
        └── madaha/                 # madaha(S-YXG50) Rust 소스 (벤더링됨, Cargo 프로젝트)
```

---

## 라이선스 / 크레딧

- 앱 코드: MIT License
- [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55) 코어: MAME License (비상업적 사용만 가능)
- [munt](https://github.com/munt/munt) (MT-32/CM-32L) 코어: LGPL-2.1
- [FluidSynth](https://www.fluidsynth.org/): LGPL-2.1
- [madaha](https://github.com/madaha-dev/madaha) (S-YXG50) 코어: 원본 저장소의
  라이선스를 따름 — `app/src/main/cpp/madaha/LICENSE` 참고
- [Arduino-AppleMIDI-Library](https://github.com/lathoub/Arduino-AppleMIDI-Library) (ESP32 RTP-MIDI): MIT License
- [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android): Apache 2.0
- ESP32 브리지 펌웨어: [Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)
- SC-55 / MT-32 ROM: 별도 라이선스 (Roland Corp.) — 미포함
- S-YXG50 ROM: 별도 라이선스 (YAMAHA Corp.) — 미포함
- SF2 사운드폰트: 각 제작자의 라이선스를 따름 — 미포함

> ⚠️ FluidSynth/munt는 LGPL-2.1입니다. 이 앱은 둘 다 **동적 라이브러리(.so)로
> 링크**하고 있어 LGPL 조건(정적으로 링크한 클로즈드소스 앱에 배포하려면 안 되는
> 조건)에 저촉되지 않지만, 배포 형태를 바꿀 계획이 있다면 라이선스 조건을 다시
> 확인하세요.
