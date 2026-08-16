/// Effect DSP common infrastructure
mod chorus_effect;
pub(crate) mod core;
mod distortion_effects;
mod dynamics;
mod eq_effects;
mod misc_effects;
mod modulation_effects;
mod multi_eq;
mod params;
mod reverb_effect;
mod variation_effect;
mod wah_effects;
mod harmony_effect;
mod xg20_effects;

pub use chorus_effect::build_chorus;
pub use multi_eq::MultiEqDsp;
pub use reverb_effect::build_reverb;
pub use variation_effect::build_variation;

/// Allocates a zero-filled `Box<[f32; N]>` directly on the heap without ever
/// materializing the (potentially large, e.g. 512KB) array on the stack.
/// `Box::new([0.0; N])` does NOT guarantee this at low optimization levels
/// (`opt-level = "z"`, used for the Android release build): the array
/// literal can legally be built on the stack first and then memcpy'd into
/// the heap allocation. That overflowed AAudio's small real-time
/// callback-thread stack in practice (confirmed on-device). `vec![0.0; N]`
/// is specially recognized by the allocator/codegen for zero-fill and heap-
/// allocates directly, so this is the safe way to build any large ring
/// buffer used by the effect DSPs below.
pub(crate) fn zeroed_box<const N: usize>() -> Box<[f32; N]> {
    let v: Vec<f32> = vec![0.0; N];
    v.into_boxed_slice()
        .try_into()
        .unwrap_or_else(|_| unreachable!("vec![0.0; N] always has length N"))
}

/// Effect processor interface
///
/// Input and output are both stereo (L, R). `sample_rate` is passed in at construction time.
pub trait EffectProcessor: Send {
    fn process(&mut self, input: (f32, f32)) -> (f32, f32);

    /// Real-time parameter modulation from a controller source
    /// (`source`: 0=MW, 1=Bend, 2=CAT, 3=PAT, 4=AC1, 5=AC2, 6=CBC1, 7=CBC2;
    /// `value`: normalized -1..1). The `ins/variation control depth` is
    /// already folded into `value` by the caller. Default: no modulation.
    fn modulate(&mut self, _source: u8, _value: f32) {}

    /// Active notes feeding a harmony/vocoder effect (XG2.0 Harmony family).
    /// Collected by the render loop from the active voices; default: no-op.
    fn set_active_notes(&mut self, _notes: &[u8]) {}
}

/// Thru effect (Thru / NoEffect)
#[derive(Debug, Default)]
pub struct Thru;

impl EffectProcessor for Thru {
    #[inline]
    fn process(&mut self, input: (f32, f32)) -> (f32, f32) {
        input
    }
}
