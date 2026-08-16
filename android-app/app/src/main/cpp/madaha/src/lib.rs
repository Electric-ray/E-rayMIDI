//! madaha_core — Android 통합용 lib 크레이트 진입점.
//!
//! 원래 madaha는 main.rs 하나짜리 바이너리 크레이트였다. 여기서는 같은
//! 모듈들을 lib.rs로 옮겨 staticlib(crate-type)로도 빌드되게 하고, 데스크톱
//! 전용(alsa MIDI 입력 / cpal 오디오 출력)은 "desktop-io" feature 뒤로
//! 숨긴다. Android 빌드는 --no-default-features 로 이 feature를 끈 채
//! staticlib만 빌드한다 (ffi.rs가 자체 렌더 루프 + JNI를 통해 들어오는
//! MIDI 이벤트로 완전히 대체한다).
#![cfg_attr(not(feature = "desktop-io"), allow(dead_code))]

pub mod args;
pub mod audio;
pub mod config;
pub mod double_buffer;
pub mod fast_sine;
pub mod ffi;
pub mod lfo;
pub mod midi;
pub mod plugin;
#[cfg(feature = "desktop-io")]
pub mod synth;
pub mod utils;
pub mod voice_manager;

#[cfg(test)]
mod audit_tests;
#[cfg(all(test, feature = "desktop-io"))]
mod e2e_tests;
