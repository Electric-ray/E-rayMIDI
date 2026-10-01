# E-ray MIDI

Roland SC-55 / MT-32 / SoundFont(SF2) / **88emu(Roland Sound Canvas·MT-32 계열
통합 엔진)** / **S-MU2000(YAMAHA MU2000)** 다섯 가지 재생 엔진과, RTP-MIDI(WiFi) /
USB 시리얼 / USB MIDI 주변장치(peripheral) / **MIDI 파일(기기 내 저장소)** 네 가지
입력 방식을 하나로 통합한 Android MIDI 재생 앱입니다.



https://github.com/user-attachments/assets/527aa40e-b534-4c0e-bc5c-3464fcb0e702




https://github.com/user-attachments/assets/fb366aac-3d51-4342-96ac-d7def23d242d





원래는 [nukeykt/Nuked-SC55](https://github.com/nukeykt/Nuked-SC55)의 Android 포팅으로
시작했지만, [munt](https://github.com/munt/munt)(MT-32/CM-32L), FluidSynth(SF2),
[dsp56300/gearmulator](https://github.com/dsp56300/gearmulator)의 88emu(Roland
Sound Canvas·MT-32 계열 통합 LLE 엔진), [tarboh/S-MU2000](https://github.com/tarboh/S-MU2000)
(YAMAHA MU2000 LLE 엔진) 코어를 함께 통합하면서 단일 엔진 포팅 프로젝트를 넘어선
5-엔진 레트로 MIDI 모듈이 되었습니다.

> **v1.7 패치 노트**: 외부 PC 없이 **기기에 저장된 .mid/.midi/.kar 파일을 직접
> 재생**할 수 있는 MIDI 파일 플레이어를 추가했습니다. LINK 패널에서
> "MIDI 파일"을 선택하면 재생/일시정지/정지/이전곡/다음곡/볼륨 조절과 폴더 단위
> 재생목록(1곡/폴더 전체/폴더 반복/1곡 반복)을 쓸 수 있고, 다섯 엔진 모두에서
> 동일하게 동작합니다(RTP-MIDI/USB와 똑같이 "또 하나의 MIDI 입력원"으로 각
> 엔진에 그대로 흘려보내는 구조라 엔진 쪽 수정은 최소한입니다). 블루투스로
> 연결된 차량 헤드유닛(AVRCP)이나 유선 이어폰 리모컨의 재생/정지/이전곡/다음곡
> 버튼도 MediaSession으로 받아 처리합니다. 자세한 변경 내역은
> [아래 "v1.7에서 달라진 점"](#v17에서-달라진-점) 참고.
>
> **v1.6 패치 노트**: 새 엔진 **S-MU2000**(YAMAHA MU2000, SH7042+SWP30x2 LLE)을
> 추가했습니다. 88emu와 동일한 렌더 스레드+링버퍼 구조를 따르되, MIDI가 바이트
> 스트림 방식이라 SysEx도 별도 분기 없이 그대로 흘려보냅니다. 레이아웃이 겹쳐
> 쌓이는 FrameLayout 구조를 잘못 벗어나 다른 엔진들의 LCD/ROM 버튼이 통째로
> 사라졌던 레이아웃 버그도 함께 고쳤습니다. 자세한 변경 내역은
> [아래 "v1.6에서 달라진 점"](#v16에서-달라진-점) 참고.
>
> **v1.5 패치 노트**: 불안정했던 S-YXG50(madaha) 엔진을 제거하고, 대신 실제
> Roland 펌웨어를 그대로 구동하는 **88emu** 엔진을 새로 얹었습니다 — SC-55/SC-88/
> SC-88Pro/SC-8850, MT-32/CM-32L/CM-32P/CM-64 등 11개 기종을 하나의 엔진에서
> 선택할 수 있습니다. 오디오 파이프라인을 다른 세 엔진과 동일한 구조(전용 렌더
> 스레드+링버퍼)로 통일했고, Debug 빌드에서 CPU 에뮬레이션 코어가 최적화 없이
> 빌드되던 문제도 고쳤습니다. 자세한 변경 내역은
> [아래 "v1.5에서 달라진 점"](#v15에서-달라진-점) 참고.
>
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
다섯 가지 레트로 신시사이저 중 하나로 실제 소리를 냅니다.

```
                 ┌─ RTP-MIDI (WiFi, ESP32 경유)
DOS PC/호스트 ───┼─ USB 시리얼 (rs232 to USB 널모뎀케이블)
                 └─ USB MIDI 주변장치 (케이블로 PC와 직결, 안드로이드가 표준 MIDI 장치로 인식됨)
                                │
     MIDI 파일(.mid, 기기 내 저장소) ──┤  ← 외부 PC 없이 폰 안에서 바로 재생
                                │
                                ▼
                     E-ray MIDI (안드로이드 앱)
                                │
        ┌──────────────┬───────┬───────┴───────┬──────────────┐
        ▼              ▼               ▼               ▼              ▼
   SC-55 엔진      MT-32 엔진     SoundFont 엔진     88emu 엔진     S-MU2000 엔진
 (Nuked-SC55 코어)  (munt 코어)   (FluidSynth 2.6)  (dsp56300/gearmulator) (tarboh/S-MU2000)
   ROM 파일 필요    ROM 파일 필요   .sf2 파일 필요   ROM 파일 필요(11개 기종)  ROM 파일 필요(파일명 고정)
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
| **RAM** | 최소 3GB 권장 | 88emu는 선택한 기종의 ROM(웨이브 ROM 포함, 최대 수 MB)을, S-MU2000은 웨이브 ROM 32MB를 메모리에 올리고, FluidSynth·SoundFont 샘플 데이터도 메모리에 올라감 — 저사양 기기에서는 여유 있게 |
| **저장공간** | ROM/SF2 파일 크기에 따라 다름 | 앱 자체는 가볍지만(APK ~수십MB), SF2 사운드폰트는 파일에 따라 수백MB까지도 가능 |
| **테스트 기기** | LG Velvet (LG-G910N, Android 10), Galaxy A52s (Android 13) | 전체 개발/디버깅이 이 두 기기에서 이뤄짐. 다른 기기에서도 동작해야 하지만 폭넓게 테스트되지는 않음 |

### 왜 arm64-v8a 전용인가?
- FluidSynth 프리빌트 바이너리와 88emu 코어는 다른 ABI로도 빌드 가능하지만,
  현재 `build.gradle.kts`가 **arm64-v8a만** 타겟하도록 고정되어 있습니다(테스트 기기가
  전부 arm64 전용이라 다른 ABI는 실기기 검증을 못 했기 때문). armeabi-v7a/x86/x86_64를
  지원하려면 `abiFilters`를 넓히고 각 ABI별로 실제 빌드가 되는지 검증이 필요합니다.
- S-MU2000의 SH-2/SWP30 JIT(`a64asm.cpp`)는 **AArch64 어셈블리를 직접 찍어내는
  코드**라서, 애초에 arm64가 아니면 컴파일 자체가 안 됩니다. 다른 ABI를
  지원하려면 S-MU2000 쪽은 인터프리터 모드로 강제하거나 x86 JIT 백엔드를
  따로 연결해야 합니다.

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

## v1.5에서 달라진 점

### 1. S-YXG50(madaha) 엔진 제거
v1.3에서 추가한 S-YXG50 엔진을 제거했습니다. madaha 코어 자체의 버그(드럼 노트가
의도한 악기와 다르게 재생되는 문제 등, 위 "v1.3에서 달라진 점 - 2" 참고)가
계속 남아있었는데, 업스트림이 이 부분을 개선할 기미가 없어 더 기다리는 대신
제거하기로 결정했습니다. `SYXG50Engine.kt`, `SYXG50Bridge.cpp`,
`app/src/main/cpp/madaha/`(Rust 소스 전체)와 CMakeLists.txt의 `cargo-ndk` 빌드
스텝을 전부 삭제했습니다 — Rust 툴체인 없이도 빌드되는 상태로 되돌아갔습니다.

### 2. 새 엔진 추가 — 88emu (Roland Sound Canvas·MT-32 계열 통합 엔진)
[dsp56300/gearmulator](https://github.com/dsp56300/gearmulator) 저장소의 88emu
(`88lib`)를 새 엔진으로 얹었습니다. Nuked-SC55/munt와 같은 철학의 **LLE
(Low-Level Emulation)** 코어로, 실제 Roland 하드웨어의 MCU(H8/500, SH-2, MCS-96
계열)와 커스텀 음원 칩을 사이클 단위로 에뮬레이션해서 원본 ROM을 그대로
구동합니다.

- 한 엔진에서 **11개 기종**을 선택할 수 있습니다: SC-55/SC-55mk2, SC-88, SC-88Pro,
  SC-8820, SC-8850, MT-32(구형/신형 기판), CM-32L, CM-32P, CM-64. "88emu" 라디오
  버튼을 선택하면 나타나는 "🎹 기종: ..." 버튼에서 팝업으로 고를 수 있고, 각
  항목 옆에 해당 ROM이 실제로 인식됐는지(✅/⚠️) 실시간으로 표시됩니다.
- 88lib는 ROM을 파일명이 아니라 **내용(해시)** 으로 식별합니다 — 다른 세 엔진처럼
  정확한 파일명을 요구하지 않고, `rom_gearmulator/` 폴더(하위 폴더 포함)에 있는
  파일을 훑어서 자동으로 알맞은 세트를 찾아 씁니다. 기존에 SC-55용으로 준비해둔
  ROM 5개를 그대로 복사해 넣으면 SC-55/SC-55mk2 모두 바로 인식됩니다.
- upstream 저장소 전체(freetype/lunasvg/RmlUi/JUCE GUI/다른 신스 포함, 수백MB)를
  그대로 벤더링하지 않고, 88lib 구동에 실제로 필요한 최소 서브셋(88lib 본체,
  h8500/sh2/mcs96 CPU 코어, custom_chips JIT, asmjit, ~14MB)만 골라
  `app/src/main/cpp/gearmulator/source/`에 upstream과 동일한 상대 경로 구조로
  벤더링했습니다 — 나중에 upstream이 갱신되면 같은 서브셋만 다시 복사해오면
  됩니다.

### 3. 오디오 파이프라인을 88emu에도 동일한 구조로 통일 — 단, 스레드 하나 제약 있음
다른 세 엔진과 똑같이 "전용 렌더 스레드 → 락프리 링버퍼 → AAudio 콜백은 pop만"
구조로 만들었습니다. 다만 한 가지 차이가 있습니다: 88lib의 `emu88_context`는
**스레드 세이프하지 않습니다**(공식 API 문서: "use it from one thread at a
time"). FluidSynth(SoundFont)처럼 MIDI 처리 스레드와 렌더 스레드를 완전히
분리할 수 없어서, SC55Bridge의 `mcuLoop`와 같은 방식으로 "MIDI 드레인 + 렌더"를
같은 전용 스레드 안에서 순서대로 처리하도록 만들었습니다. AAudio 콜백과는
여전히 링버퍼로 완전히 분리되어 있어서, 렌더가 순간적으로 느려져도(리버브 계산
등) AAudio 콜백 데드라인은 영향받지 않습니다.

### 4. Debug 빌드에서 CPU 에뮬레이션 코어가 무최적화로 빌드되던 문제 수정
`nuked-sc55-jni`는 MCU 에뮬레이션 소스를 브리지와 같은 CMake 타겟에 넣고
`-O3`를 강제하지만, 88lib는 여러 개의 독립된 CMake 타겟(h8500/sh2/mcs96/
custom_chips/hardwareLib/...)으로 나뉘어 있어서 그 방식을 그대로 쓸 수
없었습니다. 처음엔 브리지 파일에만 `-O3`를 줬는데, 사이클 단위로 명령어를
해석하는 CPU 인터프리터 자체가 Debug 빌드 기본값(`-O0`)으로 컴파일되고 있어서
실기(Galaxy A52s)에서 뚜렷한 버벅임으로 나타났습니다. `gearmulator/source/
CMakeLists.txt` 최상단에서 이 디렉터리 스코프 전체의 컴파일 플래그에
`-O3 -DNDEBUG`를 강제로 얹어서, 88lib가 필요로 하는 모든 하위 라이브러리가
Debug 빌드에서도 항상 최적화되도록 고쳤습니다. 이 수정 하나로 체감 성능이
크게 개선됐습니다.

### 5. 기종에 따라 실제 LCD 텍스트 자동 표시
88lib의 공개 C API는 Nuked-SC55처럼 LCD 픽셀 프레임버퍼를 통째로 노출하지는
않지만, 문자 LCD(HD44780 컨트롤러)를 쓰는 기종에서는 실제 디스플레이 텍스트를
제공합니다. 확인해보니 SC-55/SC-88/SC-88Pro도 MT-32/CM-32L과 같은 HD44780
계열이라, 텍스트가 정상적으로 나옵니다(SC-8850만 진짜 그래픽 LCD라 API가
텍스트를 주지 않습니다). 그래서:
- 문자 LCD가 있는 기종(MT-32/CM-32L류, SC-55/SC-88/SC-88Pro)은 기존
  `MuntLcdView`(도트매트릭스 스타일 렌더링)로 실제 기기 텍스트를 보여줍니다.
- SC-8850처럼 그래픽 전용 LCD 기종은 LCD 영역이 자동으로 접히고, 채널별 LED
  패널(활성 상태 + Program Change 번호)만 크게 표시됩니다.
- 이 전환은 자동입니다 — 어떤 기종을 고르든 실행 중에 판별해서 알맞은 화면으로
  바뀝니다.

### 6. UI 정리
- MODE 패널의 라디오버튼 텍스트를 "Gearmulator"에서 짧은 **"88emu"** 로 변경
  (줄바꿈 방지).
- S-YXG50 라디오버튼과 관련 패널을 완전히 제거.

> ⚠️ **라이선스 주의**: 88lib(dsp56300/gearmulator)는 **GPL-3.0**입니다. 다른
> 세 엔진의 라이선스(MIT/MAME/LGPL-2.1)와 다르게, 이 코드가 포함된 결과물을
> 배포하면 GPLv3 조건(소스 공개 등)이 적용될 수 있습니다. 개인 사용에는 문제가
> 없지만, 배포 계획이 있다면 조건을 다시 확인하세요. 아래 "라이선스/크레딧"
> 참고.

---

## v1.6에서 달라진 점

### 1. 새 엔진 추가 — S-MU2000 (YAMAHA MU2000)
[tarboh/S-MU2000](https://github.com/tarboh/S-MU2000) 저장소의 코어(SH7042 CPU +
SWP30 음원칩 2개)를 새 엔진으로 얹었습니다. 88emu와 같은 철학의 **LLE** 코어로,
MU2000의 SH7042 펌웨어를 실제로 구동해 소리를 냅니다. 고정 커밋
`d44b0891cb9550567db818c269122184be6fe158` 기준으로 이식했습니다.

- 업스트림의 Makefile `SRCS` 목록 + `mu2000.cpp`를 그대로 따라, 필요한 최소
  서브셋(.cpp 19개 + 헤더 43개, 약 1.5MB)만 골라 `app/src/main/cpp/smu2000/
  source/`에 upstream과 동일한 상대 경로 구조로 벤더링했습니다(데스크톱
  전용 VST3/CLAP/GUI 프런트엔드는 제외).
- 88emu(`GearmulatorEngine`)와 다른 핵심 차이:
  - **MIDI가 바이트 스트림**입니다. 88lib처럼 3바이트로 패킹하지 않고,
    `midi_in(byte, port)`에 1바이트씩 그대로 흘려보내면 SysEx 경계 처리까지
    SH7042 펌웨어가 알아서 합니다 — 브리지 쪽 로직이 오히려 더 단순합니다.
  - **블로킹 "부팅 함수"가 없습니다.** 88lib의 `emu88_open_synth()`처럼 미리
    다 부팅해주는 함수 대신, `reset()` 직후 `run_sample()`을 계속 호출하는
    것 자체가 부팅 과정입니다. 펌웨어가 부팅을 마쳤다는 신호는
    `midi_ready()`이고, 그 전에 들어온 MIDI는 펌웨어가 그냥 무시합니다.
  - **출력이 44100Hz 고정**입니다(88lib처럼 내부 리샘플러가 없음). AAudio가
    44100Hz를 못 받는 극히 드문 기기에서는 리샘플링 없이 그대로 재생되어
    피치가 살짝 달라질 수 있습니다 — 초기 이식에서는 감수합니다.
  - 초기 이식은 안전하게 **단일 스레드**(`set_threaded(false)`)로 시작했습니다.
    SWP30 두 개를 별도 스레드로 병렬 처리하는 옵션이 업스트림에 있지만,
    모바일 big.LITTLE 코어 배치에서 스핀웨이트 지연이 커질 위험이 있어
    보류했습니다.
- **출력 스케일 주의**: `run_sample()`이 주는 값은 MAME 내부 스케일
  (`DAC_FULL_SCALE = 1<<17`)이라, 16비트 PCM으로 쓰려면 `>> 2`를 해야
  합니다(업스트림 헤더 주석에 명시). 이걸 놓치면 소리가 거의 안 들릴
  정도로 작게 나옵니다 — 이식 중 미리 발견해서 반영했습니다.
- ROM은 88lib와 달리 **정확한 파일명**이 필요합니다: 프로그램 ROM은 아무
  이름이나 되지만, 웨이브 ROM은 `dump/` 폴더 안에 `xv364a0.ic49`,
  `xv365a0.ic50`, `xw848a0.ic53`, `xw849a0.ic54` 4개 파일이 정확한 이름으로
  있어야 인식됩니다(아래 "ROM / 사운드폰트 파일 배치" 참고).

### 2. 레이아웃 버그 수정 — 다른 엔진들의 LCD/ROM 버튼이 사라지던 문제
S-MU2000용 채널 패널을 화면 가운데 영역에 추가하는 과정에서, SC-55 LCD/MT-32
LCD/SoundFont 패널/88emu 패널이 전부 겹쳐 쌓이는 `FrameLayout`의 **바깥에**
잘못 형제로 붙이는 실수가 있었습니다. 이 `FrameLayout`은 `weight=1`로 남은
공간을 전부 차지하도록 설계돼 있는데, 그 뒤에 `match_parent` 높이의 뷰가
형제로 추가되면서 레이아웃 측정이 꼬여, FrameLayout 자체는 물론 그 아래
있던 상태 로그/ROM 버튼 줄과 오른쪽 LINK 패널까지 전부 화면에서 사라지는
증상으로 나타났습니다. S-MU2000 패널을 다른 패널들과 동일하게 FrameLayout
**안**으로 옮겨서 고쳤습니다 — 이제 다섯 엔진 모두 같은 방식(겹쳐 쌓고
visibility로 하나만 표시)으로 동작합니다.

---

## v1.7에서 달라진 점

### 1. 새 기능 — 기기 내 MIDI 파일 플레이어
외부 PC/ESP32 없이, 폰에 저장된 `.mid`/`.midi`/`.kar` 파일을 다섯 엔진 중
아무거나로 바로 재생할 수 있습니다. LINK 패널에 네 번째 선택지 **"MIDI 파일"**
이 추가됐고, 선택하면 화면 아래에 재생바(제목/시간/진행바/이전곡·재생·정지·
다음곡/재생모드/볼륨/폴더 탐색)가 나타납니다.

- **아키텍처**: 새로 추가된 네 클래스(`SmfParser`/`MidiFilePlayer`/
  `MidiPlaylist`/`MidiPlayerPanel`)는 소리를 직접 내지 않습니다. 대신 RTP-MIDI/
  USB와 완전히 동일하게 "지금 선택된 엔진에 MIDI 메시지를 흘려보내는 또 하나의
  입력원"으로 동작합니다 — 그래서 다섯 엔진 모두 엔진 내부 로직은 거의 손대지
  않고 지원됩니다.
  - `SmfParser.kt`: Format 0/1/2 SMF를 파싱해 "재생 시각이 붙은 MIDI 메시지
    배열"로 변환 (트랙 병합, 템포맵, 러닝 스테이터스, 분할 SysEx, 잘린 파일까지
    처리하는 순수 Kotlin 파서, 안드로이드 API 의존 없음).
  - `MidiFilePlayer.kt`: 파싱된 한 곡을 재생(재생/일시정지/정지/시크/볼륨),
    시크 시 프로그램·CC·SysEx 상태를 복원(chase).
  - `MidiPlaylist.kt`: 재생목록, 이전/다음곡, 재생모드(1곡 / 폴더 전체 / 폴더
    반복 / 1곡 반복), 자연수 정렬("2.mid" < "10.mid"), 깨진 파일 자동 스킵.
  - `MidiPlayerPanel.kt`: 위 세 클래스를 실제 화면 위젯에 연결하고, 폴더 탐색
    다이얼로그(📂)를 제공. 기본 폴더는 `Download/midi`.
- **워치독 우회**: RTP-MIDI(UDP)는 패킷 유실에 대비해 노트/서스테인 워치독
  (일정 시간 지나면 강제 Note Off/서스테인 해제)을 쓰는데, 파일 재생은 유실이
  없는 입력이라 이 워치독을 그대로 두면 오르간/스트링처럼 길게 지속되는 음이
  재생 도중 끊깁니다. `IEngine`에 `bypassWatchdogs` 플래그를 추가해서 파일
  재생 중에는 끄도록 했습니다(워치독이 없는 MuntEngine은 영향 없음).

### 2. 블루투스(차량 AVRCP)/유선 이어폰 리모컨 지원
`MidiPlayerPanel`이 `android.media.session.MediaSession`을 하나 만들어
재생/일시정지/정지/이전곡/다음곡 액션을 올려둡니다. 블루투스로 연결된 차량
헤드유닛의 스티어링휠 버튼이나 유선 이어폰 리모컨의 싱글/더블/트리플 클릭을
누르면, 안드로이드 시스템이 그 명령을 표준 미디어 키 이벤트로 바꿔서 이 세션의
콜백(`onPlay`/`onPause`/`onStop`/`onSkipToNext`/`onSkipToPrevious`)으로 그대로
넣어줍니다 — 앱이 블루투스/유선 리모컨 신호를 직접 파싱할 필요가 없습니다.
`AudioFocus`도 함께 요청해서, 전화가 오는 등 다른 앱이 오디오를 가져가면
자동으로 일시정지됩니다. (화면의 재생/일시정지 버튼은 토글이지만, 외부 PLAY/
PAUSE 명령은 분리된 호출이라 `MidiPlaylist`에 명시적 `play()`/`pause()`를
따로 뒀습니다.)

### 3. 레이아웃 — 네 번째 LINK 옵션 추가 + 버튼 가림 버그 수정
재생바가 나타나면서 LINK 패널의 세로 공간이 부족해져, "연결 시작/정지" 버튼이
화면 밖으로 밀려 눌리지 않는 문제가 실기기에서 확인됐습니다. 재생바 자체를
최대한 얇게(버튼 28dp, 진행바/볼륨 18dp) 줄인 것과 별개로, LINK 패널의
라디오그룹+버튼 부분을 `ScrollView`(`fillViewport`)로 감싸서, 공간이 부족한
경우에도 버튼이 클리핑되어 사라지는 대신 스크롤로 항상 닿을 수 있게 안전장치를
추가했습니다.

> ⚠️ **알려진 한계**: 화면이 꺼지거나 앱이 완전히 백그라운드로 밀려나면,
> 안드로이드가 재생 스레드를 중단시킬 수 있습니다(현재 구조는 화면을 계속 켜둘
> 뿐 포그라운드 서비스가 아닙니다). 차량에서 장시간 들을 때 화면이 잠긴 뒤
> 끊기는 증상이 있다면 포그라운드 서비스 전환이 필요할 수 있습니다 — 이번
> 패치에는 포함되지 않았습니다.

---

## 주요 기능

### 연결 방식 (넷 중 선택)
- **📡 RTP-MIDI**: ESP32가 WiFi AppleMIDI로 중계. 케이블 없이 무선으로 연결.
- **🔌 USB 시리얼**: DOS PC와 시리얼널모뎀 케이블로 직결.
- **🎹 USB MIDI기기**: 안드로이드 기기 자체를 USB MIDI 주변장치로 노출 — Windows 등
  PC에 케이블로 연결하면 표준 MIDI 입력 장치로 인식되어, SoftMPU 같은 DOS MIDI 드라이버나
  DAW가 직접 이 폰으로 MIDI를 보낼 수 있습니다. (`MidiManager`로 시스템이 제공하는
  USB peripheral 포트를 직접 열어 연결)
- **🎵 MIDI 파일**: 외부 PC 없이, 폰에 저장된 `.mid`/`.midi`/`.kar` 파일을 바로
  재생. 재생/일시정지/정지/이전곡/다음곡/볼륨 조절과 폴더 단위 재생목록(1곡/
  폴더 전체/폴더 반복/1곡 반복)을 지원하고, 블루투스 차량(AVRCP)이나 유선
  이어폰 리모컨의 재생/정지/이전곡/다음곡 버튼으로도 조작할 수 있습니다.

### 재생 엔진 (다섯 중 선택)
- **SC-55**: Nuked-SC55 코어를 그대로 이식, MCU 사이클 단위 에뮬레이션. 실제 LCD
  컨트롤러 동작을 픽셀 단위로 재현 (파라미터 레벨미터 애니메이션 포함).
- **MT-32**: munt 코어 이식. 실제 mt32emu LCD 텍스트를 SC-55처럼 그래픽 도트매트릭스
  스타일로 재현(파트별 LED + 패치명 + LCD 디스플레이). GS 전용으로 만들어진 곡을
  재생할 때 발생하는 무음 문제에 대한 자동 대응 포함 (아래 "알려진 이슈와 대응" 참고).
- **SoundFont**: FluidSynth 2.6.0 기반 `.sf2` 재생. 채널별 프리셋명 실시간 표시.
- **88emu**: dsp56300/gearmulator 기반 Roland 통합 LLE 엔진. SC-55/SC-88/
  SC-88Pro/SC-8820/SC-8850, MT-32/CM-32L/CM-32P/CM-64 등 11개 기종을 팝업에서
  선택. HD44780 문자 LCD 기종은 실제 기기 텍스트를, 그래픽 LCD 전용 기종(SC-8850)은
  채널별 LED 패널을 자동으로 보여줌.
- **S-MU2000**: tarboh/S-MU2000 기반 YAMAHA MU2000 LLE 엔진(SH7042+SWP30x2 실제
  펌웨어 구동). 채널별 Program Change 번호를 LED 패널로 표시.

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
- **(v1.5)** 88emu Debug 빌드에서 CPU 에뮬레이션 코어가 무최적화(`-O0`)로 빌드되어
  버벅이던 문제 수정 (위 "v1.5에서 달라진 점 - 4" 참고)
- **(v1.6)** S-MU2000 패널 추가 과정에서 생긴 레이아웃 버그로 SC-55/MT-32/88emu의
  LCD·ROM 버튼·상태창이 전부 사라졌던 문제 수정 (위 "v1.6에서 달라진 점 - 2" 참고)

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
- **88emu 모드에서 SC-8850을 고르면 LCD 영역이 항상 접혀 있습니다.** SC-8850은
  진짜 그래픽 LCD(문자가 아닌 픽셀 프레임버퍼)를 쓰는데, 88lib의 공개 C API가
  텍스트 디스플레이(HD44780류)만 노출하고 그래픽 프레임버퍼는 제공하지 않아서,
  현재 API로는 Nuked-SC55처럼 픽셀 단위로 재현할 방법이 없습니다. 채널별 LED
  패널로 대신 표시됩니다.
- **(v1.6) S-MU2000 모드는 아직 LCD가 없고, 채널별 LED 패널만 표시됩니다.**
  업스트림에 `lcd_render()`로 실제 픽셀 프레임버퍼를 가져오는 API가 있어서
  구현 자체는 가능해 보이지만, 우선 소리부터 안정화하는 것을 우선해 이번
  버전에서는 연결하지 않았습니다.
- **(v1.6) S-MU2000은 초기 이식 상태로, 단일 스레드(`set_threaded(false)`)로만
  구동합니다.** 업스트림에는 SWP30 음원칩 두 개를 별도 스레드로 병렬 처리하는
  옵션이 있으나, 모바일 big.LITTLE 코어 배치에서 스핀웨이트 지연이 커질 위험이
  있어 아직 켜지 않았습니다. 실기 CPU 여유를 더 보고 나중에 검토할 예정입니다.
- **(v1.6) S-MU2000은 44100Hz 고정 출력**이라(88lib처럼 내부 리샘플러가 없음),
  AAudio가 44100Hz를 못 받는 극히 드문 기기에서는 피치가 살짝 달라질 수 있습니다.
- **(v1.7) MIDI 파일 재생 중 화면이 꺼지거나 앱이 백그라운드로 완전히 밀려나면
  끊길 수 있습니다.** 포그라운드 서비스가 아니라 화면을 계속 켜두는 방식으로만
  재생을 유지하고 있어서, 차량에서 장시간 재생 중 화면이 잠기면 안드로이드가
  재생 스레드를 중단시킬 가능성이 있습니다. 포그라운드 서비스 전환은 아직
  반영되지 않았습니다.
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
| 88emu | `/sdcard/Download/rom_gearmulator/` (파일명 무관 — 내용으로 자동 인식, 하위 폴더도 OK) |
| S-MU2000 | `/sdcard/Download/rom_mu2000/` — `mu2000_flash.bin`(프로그램, 이름 무관) + `dump/xv364a0.ic49`·`xv365a0.ic50`·`xw848a0.ic53`·`xw849a0.ic54`(웨이브 ROM, **파일명 정확히 일치해야 함**) |

SC-55에 정확히 필요한 ROM 파일명은 앱 실행 후 "ROM 파일 안내" 버튼에서 확인할 수
있습니다 (모델별로 다를 수 있음). ROM/사운드폰트 파일은 저작권 보호 대상이라
이 저장소에는 포함되어 있지 않습니다 — 직접 준비해야 합니다.

LINK에서 "MIDI 파일"을 선택했을 때 재생할 `.mid`/`.midi`/`.kar` 파일의 기본
폴더는 `/sdcard/Download/midi/`입니다(앱 안의 📂 버튼으로 다른 폴더도 탐색
가능, 마지막으로 연 폴더는 기억됩니다).

더 자세한 사용법은 [docs/사용설명서.md](docs/사용설명서.md)를 참고하세요.

---

## 빌드 환경

- Android Studio Hedgehog 이상
- NDK r25c 이상 (검증된 버전: 28.2.13676358)
- CMake 3.22.1
- Target SDK 36 (Android 15) / Min SDK 29 (Android 10)
- C++17 / C++20
- 88emu(gearmulator)와 S-MU2000 빌드에 별도 도구는 필요 없습니다 — 필요한
  소스가 전부 `app/src/main/cpp/gearmulator/source/`와 `app/src/main/cpp/
  smu2000/source/`에 벤더링되어 있어서 표준 CMake/NDK 빌드로 충분합니다.
- FluidSynth 관련 사전 준비물과 빌드 순서는 [BUILDING.md](BUILDING.md)에
  자세히 정리되어 있습니다.

---

## 프로젝트 구조

```
android-app/
└── app/src/main/
    ├── java/com/example/nukedsc55/
    │   ├── IEngine.kt              # 5개 엔진 공통 인터페이스 (PartInfo 등 공용 타입 포함)
    │   ├── SC55Engine.kt           # Nuked-SC55 코어 어댑터
    │   ├── MuntEngine.kt           # munt(mt32emu) 코어 어댑터
    │   ├── SoundFontEngine.kt      # FluidSynth 어댑터
    │   ├── GearmulatorEngine.kt    # 88emu(dsp56300/gearmulator) 어댑터, 11개 기종 선택
    │   ├── MU2000Engine.kt         # S-MU2000(tarboh/S-MU2000) 어댑터
    │   ├── LcdView.kt              # SC-55 그래픽 LCD (네이티브 프레임버퍼 → Bitmap)
    │   ├── MuntLcdView.kt          # MT-32/88emu 공용 그래픽 LCD (텍스트 → 도트매트릭스 렌더링)
    │   ├── RtpMidiSession.kt       # RTP-MIDI(AppleMIDI) 클라이언트
    │   ├── UsbMidiManager.kt       # USB 시리얼 입력 (usb-serial-for-android)
    │   ├── MidiStreamParser.kt     # Running-status/SysEx MIDI 바이트 파서
    │   ├── SmfParser.kt            # 표준 MIDI 파일(SMF) 파서 → 시각 붙은 메시지 배열
    │   ├── MidiFilePlayer.kt       # 한 곡 재생(재생/일시정지/정지/시크/볼륨)
    │   ├── MidiPlaylist.kt         # 재생목록/이전·다음곡/재생모드
    │   ├── MidiPlayerPanel.kt      # MIDI 파일 플레이어 UI + MediaSession(블루투스/유선 리모컨)
    │   ├── EngineRegistry.kt       # USB MIDI 주변장치/MIDI 파일 모드용 현재 활성 엔진 참조
    │   ├── UsbMidiDeviceService.kt # 안드로이드를 가상 MIDI 장치로 노출 (MidiDeviceService)
    │   └── MainActivity.kt         # 가로형 레트로 GUI, LED 채널 패널 공용 로직
    └── cpp/
        ├── SC55Bridge.cpp          # Nuked-SC55 JNI 브리지
        ├── MuntBridge.cpp          # munt(mt32emu) JNI 브리지
        ├── FluidBridge.cpp         # FluidSynth JNI 브리지
        ├── GearmulatorBridge.cpp   # 88emu JNI 브리지 (전용 렌더 스레드 + 링버퍼)
        ├── MU2000Bridge.cpp        # S-MU2000 JNI 브리지 (전용 렌더 스레드 + 링버퍼, 바이트스트림 MIDI)
        ├── munt/mt32emu/           # munt 코어 소스 (이식됨)
        ├── nuked-sc55/             # Nuked-SC55 코어 소스
        ├── fluidsynth/             # FluidSynth 공식 Android 프리빌트 (include/ + lib/<abi>/)
        ├── gearmulator/            # 88emu(dsp56300/gearmulator) 벤더링 소스
        │   ├── GearmulatorEngine.h/.cpp   # emu88 C API를 감싸는 C++ 어댑터
        │   └── source/             # 88lib 구동 최소 서브셋 (upstream과 동일한 경로 구조)
        └── smu2000/                # S-MU2000(tarboh/S-MU2000) 벤더링 소스
            ├── MU2000Engine.h/.cpp # mu2000 클래스를 감싸는 C++ 어댑터
            └── source/             # SH7042+SWP30 구동 최소 서브셋 (upstream과 동일한 경로 구조)
```

---

## 라이선스 / 크레딧

- 앱 코드: MIT License
- [Nuked SC-55](https://github.com/nukeykt/Nuked-SC55) 코어: MAME License (비상업적 사용만 가능)
- [munt](https://github.com/munt/munt) (MT-32/CM-32L) 코어: LGPL-2.1
- [FluidSynth](https://www.fluidsynth.org/): LGPL-2.1
- [dsp56300/gearmulator](https://github.com/dsp56300/gearmulator)의 88emu(`88lib`)
  코어: **GPL-3.0**
- [tarboh/S-MU2000](https://github.com/tarboh/S-MU2000) 코어: BSD-3-Clause,
  단 MAME 프로젝트에서 유래한 부분(SH-2/SWP30 에뮬레이션 등, `src/mame/` 경로)을
  포함하고 있어 **MAME License(비상업적 사용만 가능) 조건도 함께 적용될 수
  있습니다** — 정확한 출처는 저장소의 `NOTICE_S-MU2000.txt`를 참고하세요
- [Arduino-AppleMIDI-Library](https://github.com/lathoub/Arduino-AppleMIDI-Library) (ESP32 RTP-MIDI): MIT License
- [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android): Apache 2.0
- ESP32 브리지 펌웨어: [Electric-ray/E-RayDSB](https://github.com/Electric-ray/E-RayDSB)
- SC-55 / MT-32 / 88emu ROM: 별도 라이선스 (Roland Corp.) — 미포함
- S-MU2000 ROM: 별도 라이선스 (YAMAHA Corp.) — 미포함
- SF2 사운드폰트: 각 제작자의 라이선스를 따름 — 미포함

> ⚠️ FluidSynth/munt는 LGPL-2.1입니다. 이 앱은 둘 다 **동적 라이브러리(.so)로
> 링크**하고 있어 LGPL 조건(정적으로 링크한 클로즈드소스 앱에 배포하려면 안 되는
> 조건)에 저촉되지 않지만, 배포 형태를 바꿀 계획이 있다면 라이선스 조건을 다시
> 확인하세요.
>
> ⚠️ 88emu(dsp56300/gearmulator, `88lib`)는 **GPL-3.0**으로, 위 LGPL 두 개와는
> 조건이 다릅니다. 이 앱은 88lib를 **정적으로 링크**해서 하나의 `.so`
> (`libgearmulator-jni.so`)로 빌드합니다 — GPLv3는 이렇게 결합된 결과물을
> 배포할 경우 전체 결합물에 대해 소스 공개 등 copyleft 조건이 적용될 수 있습니다.
> 개인적으로 빌드해서 쓰는 데는 문제가 없지만, 앱을 배포(스토어 업로드, APK 공유
> 등)할 계획이 있다면 GPLv3 조건을 검토해야 합니다 (법률 자문 아님).
>
> ⚠️ S-MU2000(tarboh/S-MU2000) 코어는 저장소 표기상 BSD-3-Clause이지만, MAME
> 프로젝트에서 유래한 코드(SH-2/SWP30 에뮬레이션 등)를 포함하고 있어 **MAME
> License(비상업적 사용만 가능) 조건이 함께 적용될 가능성**이 있습니다. Nuked-SC55와
> 마찬가지로 개인/비상업 용도로만 쓰는 것을 전제로 포함했습니다. 정확한 조건은
> 저장소의 `NOTICE_S-MU2000.txt`를 직접 확인하세요 (법률 자문 아님).
