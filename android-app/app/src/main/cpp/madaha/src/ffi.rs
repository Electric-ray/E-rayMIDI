//! ffi.rs — Android(SYXG50Bridge.cpp) 연동용 C ABI 표면.
//!
//! madaha 데스크톱 바이너리는 MIDI 입력(ALSA Seq)과 오디오 출력(cpal)에
//! 강결합되어 있다("desktop-io" feature, lib.rs 참고). 이 파일은 그 두
//! 계층을 완전히 대체한다:
//!   - MIDI 입력: 호출자가 packed u32 / SysEx 바이트를 직접 넘김
//!     (ALSA Seq가 원래 해주던 RPN/NRPN 누적을 여기서 직접 구현 — 아래
//!     RpnTracker 참고. 자세한 배경은 통합 계획 참고)
//!   - 오디오 출력: VecBufferSink(테스트용으로 이미 존재하던 순수 버퍼 싱크)에
//!     렌더링한 뒤 f32 → i16 변환해서 호출자가 준 버퍼에 채운다.
//!
//! 전역 인스턴스 하나만 지원한다 — nuked-sc55-jni/munt-jni/soundfont-jni와
//! 동일하게 "엔진당 .so 하나, .so당 전역 상태 하나" 모델.

use std::ffi::CStr;
use std::os::raw::c_char;
use std::panic::{self, AssertUnwindSafe};
use std::sync::mpsc::sync_channel;
use std::sync::Mutex;

use crate::audio::AudioRender;
use crate::audio::sink::{AudioSink, GainSink, VecBufferSink};
use crate::config::{Config, ConfigObject};
use crate::midi::event::MidiEvent;
use crate::midi::note::Note;
use crate::midi::sysex::{ManufacturerId, SYSEX_MSG_END, SYSEX_MSG_START};
use crate::midi::Engine;

struct Instance {
    engine: Engine,
    render: AudioRender,
    rpn: [RpnChannelState; 16],
}

static INSTANCE: Mutex<Option<Box<Instance>>> = Mutex::new(None);
static LAST_ERROR: Mutex<String> = Mutex::new(String::new());

fn set_last_error(msg: impl Into<String>) {
    if let Ok(mut e) = LAST_ERROR.lock() {
        *e = msg.into();
    }
}

// ── RPN/NRPN 누적 상태 ───────────────────────────────────────────────
// madaha::midi::event::MidiEvent은 이미 해석된 RPN{parameter,value}/
// NRPN{parameter,value} 변형을 갖고 있는데, 데스크톱 빌드에서는 이걸
// ALSA Seq 라이브러리가 대신 만들어준다(alsa.rs의 Regparam/Nonregparam).
// Android에서는 원시 MIDI 바이트만 들어오므로 CC 98/99/100/101(파라미터
// 선택) + CC 6/38(Data Entry MSB/LSB)을 직접 누적해서 같은 모양의
// MidiEvent를 만들어야 한다. XG/GS 기기 대부분처럼 CC6(MSB)만으로도
// 값이 확정되도록 하고(가장 흔한 관례), CC38(LSB)이 오면 미세 조정한다.
#[derive(Clone, Copy, Default)]
struct RpnChannelState {
    mode: RpnMode,
    param_msb: u8,
    param_lsb: u8,
    data_msb: u8,
}

#[derive(Clone, Copy, Default, PartialEq)]
enum RpnMode {
    #[default]
    None,
    Rpn,
    Nrpn,
}

impl RpnChannelState {
    /// CC(controller, value) 하나를 먹인다. RPN/NRPN로 확정되면
    /// Some(is_rpn, parameter, value)를 돌려주고, 그 외(일반 CC로 그대로
    /// 흘려보내야 하는 경우)는 None을 돌려준다.
    fn feed(&mut self, controller: u8, value: u8) -> Option<(bool, u16, u16)> {
        match controller {
            101 => {
                // RPN MSB (127,127 = Null: 파라미터 선택 해제)
                if value == 0x7F {
                    self.mode = RpnMode::None;
                } else {
                    self.mode = RpnMode::Rpn;
                    self.param_msb = value;
                }
                None
            }
            100 => {
                if value == 0x7F {
                    self.mode = RpnMode::None;
                } else {
                    self.mode = RpnMode::Rpn;
                    self.param_lsb = value;
                }
                None
            }
            99 => {
                self.mode = RpnMode::Nrpn;
                self.param_msb = value;
                None
            }
            98 => {
                self.mode = RpnMode::Nrpn;
                self.param_lsb = value;
                None
            }
            6 => {
                self.data_msb = value;
                match self.mode {
                    RpnMode::None => None,
                    RpnMode::Rpn => Some((true, self.param(), (value as u16) << 7)),
                    RpnMode::Nrpn => Some((false, self.param(), (value as u16) << 7)),
                }
            }
            38 => match self.mode {
                RpnMode::None => None,
                RpnMode::Rpn => Some((
                    true,
                    self.param(),
                    ((self.data_msb as u16) << 7) | value as u16,
                )),
                RpnMode::Nrpn => Some((
                    false,
                    self.param(),
                    ((self.data_msb as u16) << 7) | value as u16,
                )),
            },
            _ => None,
        }
    }

    fn param(&self) -> u16 {
        ((self.param_msb as u16) << 7) | self.param_lsb as u16
    }
}

// ── packed u32(status|d1<<8|d2<<16) → MidiEvent ─────────────────────
// SoundFontEngine.kt/SC55Engine.kt와 동일한 패킹 규약을 그대로 따른다.
fn packed_to_event(rpn: &mut [RpnChannelState; 16], packed: u32) -> Option<MidiEvent> {
    let status = (packed & 0xFF) as u8;
    let d1 = ((packed >> 8) & 0xFF) as u8;
    let d2 = ((packed >> 16) & 0xFF) as u8;
    let ty = status & 0xF0;
    let ch = status & 0x0F;

    match ty {
        0x80 => Some(MidiEvent::NoteOff {
            channel: ch,
            note: Note::try_from(d1).ok()?,
            velocity: d2,
            off_velocity: d2,
            duration: 0,
        }),
        0x90 => Some(MidiEvent::NoteOn {
            channel: ch,
            note: Note::try_from(d1).ok()?,
            velocity: d2,
            off_velocity: 0,
            duration: 0,
        }),
        0xA0 => Some(MidiEvent::PolyPressure {
            channel: ch,
            note: Note::try_from(d1).ok()?,
            pressure: d2,
        }),
        0xB0 => match rpn[ch as usize].feed(d1, d2) {
            Some((true, parameter, value)) => Some(MidiEvent::RPN {
                channel: ch,
                parameter,
                value,
            }),
            Some((false, parameter, value)) => Some(MidiEvent::NRPN {
                channel: ch,
                parameter,
                value,
            }),
            None => Some(MidiEvent::ControlChange {
                channel: ch,
                controller: d1,
                value: d2,
            }),
        },
        0xC0 => Some(MidiEvent::ProgramChange {
            channel: ch,
            program: d1,
        }),
        0xD0 => Some(MidiEvent::ChannelPressure {
            channel: ch,
            pressure: d1,
        }),
        0xE0 => Some(MidiEvent::PitchBend {
            channel: ch,
            value: (d1 as u16) | ((d2 as u16) << 7),
        }),
        0xF0 if status == 0xFE => Some(MidiEvent::ActiveSensing),
        _ => None,
    }
}

/// SysEx 원시 바이트(0xF0 ... 0xF7 포함 여부 무관)를 MidiEvent::SysEx로.
fn sysex_to_event(data: &[u8]) -> Option<MidiEvent> {
    let mut body = data;
    if body.first() == Some(&SYSEX_MSG_START) {
        body = &body[1..];
    }
    if body.last() == Some(&SYSEX_MSG_END) {
        body = &body[..body.len() - 1];
    }
    if body.is_empty() {
        return None;
    }
    let manufacturer_id = ManufacturerId::try_from(body[0]).ok()?;
    Some(MidiEvent::SysEx {
        manufacturer_id,
        data: body[1..].into(),
    })
}

// ── extern "C" 표면 ──────────────────────────────────────────────────

/// bin_path/wave_path: sxgbin41.tbl / sxgwave4.tbl 절대경로(NUL 종료 UTF-8).
/// sample_rate: AAudio가 실제로 열어준 네이티브 레이트.
/// max_polyphony: 16의 배수, 32~2048 (모바일 CPU 예산에 맞춰 128~256 권장).
/// 반환: 성공 1, 실패 0 (madaha_get_last_error로 사유 조회).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn madaha_init(
    bin_path: *const c_char,
    wave_path: *const c_char,
    sample_rate: u32,
    max_polyphony: u16,
) -> i32 {
    let bin_path = unsafe { CStr::from_ptr(bin_path) }
        .to_string_lossy()
        .into_owned();
    let wave_path = unsafe { CStr::from_ptr(wave_path) }
        .to_string_lossy()
        .into_owned();
    let max_polyphony = max_polyphony.max(16);

    // TBL 파싱 + Engine/AudioRender 초기화 경로에서 큰 임시값이 스택을
    // 잠깐 거쳐가는 지점이 있다(실기기 SIGSEGV "stack pointer is not in
    // a rw map; likely due to stack overflow"로 확인됨 — 실측 결과
    // Engine 하나가 값으로 8.4MB, note_cent_table([[[f32;128];128];128])
    // 하나가 8MB를 차지한다). JNI 호출 스레드(보통 앱 UI 스레드)의 기본
    // 스택 크기로는 어림도 없어서, 넉넉한 스택(32MB)을 가진 전용 스레드에서
    // 초기화를 돌리고 *Box*로 즉시 힙에 옮긴 뒤 그 포인터만 돌려받는다
    // (Box를 안 쓰면 join()이 8MB 값을 그대로 호출 스레드 프레임에 다시
    // 얹으면서 똑같이 죽는다 — 실제로 한 번 그렇게 죽는 것까지 확인함).
    let build = move || -> Result<Box<Instance>, String> {
        let mut cfg = Config::new();
        cfg.sound_module.module_type = libmadaha::SoundModuleType::Syxg50;
        cfg.sound_module.tbl_bin_file = bin_path;
        cfg.sound_module.tbl_data_file = wave_path;
        cfg.audio.sample_rate = sample_rate;
        cfg.midi.max_polyphony = max_polyphony;

        let source_sample_rate = cfg.sound_module.module_type.get_sample_rate();
        let target_sample_rate = sample_rate as f32;
        let count =
            (cfg.midi.max_polyphony as u32 * cfg.midi.poly_replicant as u32 / 100) as usize;
        let scoring = cfg.midi.scoring.clone();

        let (tx, rx) = sync_channel(cfg.midi.channel_size);
        let mut render = AudioRender::new(
            count,
            cfg.midi.max_polyphony,
            source_sample_rate,
            target_sample_rate,
            scoring,
            false,
            rx,
        );
        render.dc_enabled = cfg.audio.dc_blocker;

        // GainSink로 감싸서 게인 축소 + tanh 소프트클립을 적용한다.
        // 하드 클램프(-1..1 절단)만 하던 이전 버전은 세게 겹치는 순간
        // (곡 도입부처럼 노트/파트가 한꺼번에 몰릴 때) 디지털 클리핑
        // 노이즈로 들렸다 — 실기기에서 확인됨(SC-55 대비 S-YXG50 쪽
        // 전체 볼륨이 커서 더 자주 걸렸던 것으로 보임). 0.7배로 헤드룸을
        // 두고, tanh로 그 헤드룸을 넘는 순간도 딱딱 끊기지 않고 부드럽게
        // 눌러준다.
        let raw_sink: Box<dyn AudioSink> = Box::new(VecBufferSink::new());
        render.sink = Box::new(GainSink::new(raw_sink, 0.7, true));

        let engine = Engine::new(&cfg, tx);
        engine.send_audio_init();

        Ok(Box::new(Instance {
            engine,
            render,
            rpn: [RpnChannelState::default(); 16],
        }))
    };

    let spawned = std::thread::Builder::new()
        .name("madaha-init".into())
        .stack_size(32 * 1024 * 1024)
        .spawn(build);

    let handle = match spawned {
        Ok(h) => h,
        Err(e) => {
            set_last_error(format!("madaha_init 스레드 생성 실패: {e}"));
            return 0;
        }
    };

    match handle.join() {
        Ok(Ok(inst)) => {
            *INSTANCE.lock().unwrap() = Some(inst);
            1
        }
        Ok(Err(msg)) => {
            set_last_error(msg);
            0
        }
        Err(panic_payload) => {
            let msg = panic_message(&panic_payload);
            set_last_error(format!("madaha_init panicked: {msg}"));
            0
        }
    }
}

fn panic_message(e: &(dyn std::any::Any + Send)) -> String {
    if let Some(s) = e.downcast_ref::<&str>() {
        s.to_string()
    } else if let Some(s) = e.downcast_ref::<String>() {
        s.clone()
    } else {
        "unknown panic".to_string()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn madaha_destroy() {
    *INSTANCE.lock().unwrap() = None;
}

#[unsafe(no_mangle)]
pub extern "C" fn madaha_is_ready() -> i32 {
    i32::from(INSTANCE.lock().unwrap().is_some())
}

/// packed = status | (data1<<8) | (data2<<16). SoundFontEngine.kt와 동일 규약.
#[unsafe(no_mangle)]
pub extern "C" fn madaha_send_midi(packed: u32) {
    let mut guard = INSTANCE.lock().unwrap();
    let Some(inst) = guard.as_mut() else { return };
    if let Some(ev) = packed_to_event(&mut inst.rpn, packed) {
        let _ = panic::catch_unwind(AssertUnwindSafe(|| inst.engine.on_event(ev)));
    }
}

/// data: 0xF0/0xF7 포함 여부 무관한 SysEx 바이트열, len: 바이트 수.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn madaha_send_sysex(data: *const u8, len: i32) {
    if data.is_null() || len <= 0 {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(data, len as usize) };
    let mut guard = INSTANCE.lock().unwrap();
    let Some(inst) = guard.as_mut() else { return };
    if let Some(ev) = sysex_to_event(bytes) {
        let _ = panic::catch_unwind(AssertUnwindSafe(|| inst.engine.on_event(ev)));
    }
}

/// CC#123(All Notes Off) + CC#120(All Sound Off)을 전 채널에 보낸다.
#[unsafe(no_mangle)]
pub extern "C" fn madaha_all_notes_off() {
    let mut guard = INSTANCE.lock().unwrap();
    let Some(inst) = guard.as_mut() else { return };
    for ch in 0..16u8 {
        for cc in [123u8, 120u8] {
            if let Some(ev) = packed_to_event(
                &mut inst.rpn,
                (0xB0 | ch as u32) | ((cc as u32) << 8),
            ) {
                inst.engine.on_event(ev);
            }
        }
    }
}

/// out: frames*2(stereo) 개의 i16을 채울 버퍼. 초기화 전이거나 인스턴스가
/// 없으면 무음(0)으로 채운다 — AAudio 콜백에서 매 호출 반드시 버퍼를
/// 채워야 하므로 실패를 조용히 무음 처리한다.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn madaha_render_i16(out: *mut i16, frames: i32) {
    if out.is_null() || frames <= 0 {
        return;
    }
    let n_samples = frames as usize * 2;
    let mut guard = INSTANCE.lock().unwrap();
    let Some(inst) = guard.as_mut() else {
        unsafe { std::ptr::write_bytes(out, 0, n_samples * size_of::<i16>()) };
        return;
    };

    let render_result = panic::catch_unwind(AssertUnwindSafe(|| {
        for _ in 0..frames {
            inst.render.audio_render();
        }
        inst.render.flush();
    }));
    if render_result.is_err() {
        unsafe { std::ptr::write_bytes(out, 0, n_samples * size_of::<i16>()) };
        return;
    }

    let Some(gain_sink) = inst.render.sink.as_any_mut().downcast_mut::<GainSink>() else {
        unsafe { std::ptr::write_bytes(out, 0, n_samples * size_of::<i16>()) };
        return;
    };
    let Some(vb) = gain_sink
        .inner_mut()
        .as_any_mut()
        .downcast_mut::<VecBufferSink>()
    else {
        unsafe { std::ptr::write_bytes(out, 0, n_samples * size_of::<i16>()) };
        return;
    };
    let buf = vb.take_buffer();
    let copy_n = buf.len().min(n_samples);
    unsafe {
        for i in 0..copy_n {
            *out.add(i) = (buf[i].clamp(-1.0, 1.0) * 32767.0) as i16;
        }
        if copy_n < n_samples {
            std::ptr::write_bytes(
                out.add(copy_n),
                0,
                (n_samples - copy_n) * size_of::<i16>(),
            );
        }
    }
}

/// 마지막 오류 메시지(UTF-8, NUL 종료)를 buf에 복사. 반환값 = 필요한 길이
/// (NUL 제외). buf가 null이거나 buf_len이 부족하면 잘라서 채운다.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn madaha_get_last_error(buf: *mut c_char, buf_len: i32) -> i32 {
    let msg = LAST_ERROR.lock().unwrap().clone();
    let bytes = msg.as_bytes();
    if !buf.is_null() && buf_len > 0 {
        let n = bytes.len().min((buf_len - 1) as usize);
        unsafe {
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), buf as *mut u8, n);
            *buf.add(n) = 0;
        }
    }
    bytes.len() as i32
}

#[cfg(test)]
mod size_diag {
    use super::*;
    #[test]
    fn print_sizes() {
        eprintln!("size_of::<Instance>()    = {}", size_of::<Instance>());
        eprintln!("size_of::<Engine>()      = {}", size_of::<Engine>());
        eprintln!("size_of::<AudioRender>() = {}", size_of::<AudioRender>());
    }
}
