// SC55Bridge.cpp  v3.0
// JNI bridge: Nuked-SC55 (jcmoyer fork) + AAudio
//
// Architecture:
//   nativeInit()  : ROM load only (fast, UI thread safe)
//   nativeStart() : starts MCU thread -> warmup -> GS Reset -> normal play
//   MCU thread    : warm-up (64k samples) -> GS Reset -> MIDI drain + Step loop
//   sampleCallback: 32->48kHz SRC -> ring buffer
//   AAudio CB     : ring buffer -> speaker

#include <jni.h>
#include <android/log.h>
#include <aaudio/AAudio.h>

#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdint>
#include <span>
#include <filesystem>
#include <chrono>
#include <sched.h>
#include <pthread.h>

#include "nuked-sc55/src/backend/emu.h"
#include "nuked-sc55/src/backend/rom.h"
#include "nuked-sc55/src/backend/rom_io.h"
#include "nuked-sc55/src/backend/audio.h"
#include "nuked-sc55/src/backend/pcm.h"
#include "nuked-sc55/src/backend/lcd.h"
#include <android/bitmap.h>

#define TAG "SC55Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Audio ring buffer  (MCU thread -> AAudio callback)
// ---------------------------------------------------------------------------
static constexpr int RING_FRAMES     = 16384;
// CRITICAL FIX (v5.0 diagnostic confirmed): SC-55mk2 does NOT natively
// output 32000Hz. PCM_GetOutputFrequency() returns 66207Hz (oversampled;
// pcm_t::enable_oversampling defaults to true in the backend). We had
// hardcoded 32000 based on an incorrect assumption from documentation
// skimming, and this 2.07x rate mismatch (66207/32000) is the most likely
// root cause of the persistent timing artifacts (dropped/masked/staccato
// notes, timing struggles under load) that survived every other fix -
// because our ring-buffer pacing logic was fighting the MCU's real cycle
// timing to compensate for producing samples ~2x "too fast" relative to
// what we assumed.
static uint32_t s_scActualRate = 66207; // set from PCM_GetOutputFrequency() in nativeInit
static constexpr int kSampleRate     = 48000; // AAudio fallback rate if native rate rejected
static constexpr int kChannels       = 2;
static constexpr int kFramesBurst    = 512;

struct StereoS16 { int16_t l, r; };
static StereoS16        s_ring[RING_FRAMES];
static std::atomic<int> s_ringHead{0};
static std::atomic<int> s_ringTail{0};

static inline int ring_size() {
    return (s_ringHead.load(std::memory_order_acquire) -
            s_ringTail.load(std::memory_order_acquire) + RING_FRAMES) % RING_FRAMES;
}
// Diagnostics: count ring buffer overflow (MCU produced faster than AAudio
// consumed - samples dropped) and underrun (AAudio wanted samples but ring
// was empty - silence inserted). Logged once per second from mcuLoop.
static std::atomic<uint32_t> g_ringOverflowCount{0};
static std::atomic<uint32_t> g_ringUnderrunCount{0};

static void ring_push(const StereoS16& f) {
    int h  = s_ringHead.load(std::memory_order_relaxed);
    int nh = (h + 1) % RING_FRAMES;
    if (nh == s_ringTail.load(std::memory_order_acquire)) {
        g_ringOverflowCount.fetch_add(1, std::memory_order_relaxed);
        return; // full, drop
    }
    s_ring[h] = f;
    s_ringHead.store(nh, std::memory_order_release);
}
static bool ring_pop(StereoS16& f) {
    int t = s_ringTail.load(std::memory_order_relaxed);
    if (t == s_ringHead.load(std::memory_order_acquire)) {
        g_ringUnderrunCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    f = s_ring[t];
    s_ringTail.store((t + 1) % RING_FRAMES, std::memory_order_release);
    return true;
}

// ---------------------------------------------------------------------------
// MIDI event queue  (munt-style: mutex + deque)
// ---------------------------------------------------------------------------
struct MidiEv {
    bool     isSysex;
    uint32_t packed;
    std::shared_ptr<std::vector<uint8_t>> sysex;
};
static std::deque<MidiEv>    g_evQ;
static std::mutex            g_evMtx;
static std::atomic<uint64_t> g_midiCount{0};
static std::atomic<uint64_t> g_sysexCount{0};
static std::atomic<uint64_t> g_midiDedupDropped{0};
static std::atomic<uint64_t> g_sysexDedupDropped{0};

// ---------------------------------------------------------------------------
// Emulator instance
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// LCD backend: renders the real SC-55 LCD controller emulation output
// (lcd_t::buffer, an ARGB pixel framebuffer produced by lcd.cpp - the exact
// same rendering logic used by the original PC frontends) so our Android
// UI can show a pixel-accurate replica of the real hardware LCD.
// ---------------------------------------------------------------------------
class AndroidLcdBackend : public LCD_Backend {
public:
    bool Start(const lcd_t& /*lcd*/) override { return true; }
    void Stop() override {}
    void Render() override { /* lcd.buffer is already up to date by the time
                                 this is called; nativeGetLcdFrame() reads
                                 it directly under lcd.mutex on demand. */ }
};
static AndroidLcdBackend s_lcdBackend;
static std::atomic<bool> s_lcdStarted{false};

static Emulator s_emu;

// Dedicated LCD render thread - runs on its own core, fully independent
// from the MCU thread. This is the key to using the device's multiple
// CPU cores properly: the MCU thread's only job is audio+MIDI (must never
// stall), and LCD rendering (real background compositing + font drawing
// over a 741x268 framebuffer, non-trivial work) gets its own thread that
// the Android/Linux scheduler will freely migrate to any idle core.
static std::thread       s_lcdThread;
static std::atomic<bool> s_lcdThreadRunning{false};

static void lcdThreadLoop() {
    // lcd.cpp's LCD_Render() now takes a BLOCKING lock (was try_lock(),
    // silently dropping frames - see lcd.cpp fix). That means a single
    // call per frame is now sufficient and always up to date; the old
    // 8x-retry-with-sleep loop here is no longer needed and was actually
    // burning up to 16ms/frame of thread time for nothing.
    constexpr auto kLcdInterval = std::chrono::milliseconds(33); // ~30fps

    while (s_lcdThreadRunning.load(std::memory_order_relaxed)) {
        auto frameStart = std::chrono::steady_clock::now();
        if (s_lcdStarted.load(std::memory_order_relaxed)) {
            LCD_Render(s_emu.GetLCD());
        }
        auto elapsed = std::chrono::steady_clock::now() - frameStart;
        auto sleepFor = kLcdInterval - elapsed;
        if (sleepFor > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleepFor);
        }
    }
}

// ---------------------------------------------------------------------------
// MIDI drain  (called from MCU thread every kDrainInterval samples)
// ---------------------------------------------------------------------------
static constexpr int kDrainInterval = 32; // ~1ms @ 32kHz
static int s_drainCounter = 0;

// ChatGPT diagnosis (validated): the old version held g_evMtx for the
// ENTIRE duration of draining the queue (while(!empty()){...pop_front()}).
// If many events piled up in a short window (e.g. one RTP packet contains
// several MIDI commands, which is common), the mutex stayed locked long
// enough to block the RTP/USB receive threads from pushing new events -
// causing exactly the "sound cutting in and out rapidly, but musical
// timing stays correct" symptom (network receive stalls, not audio
// underrun). Fix: swap the queue out under the lock (O(1), lock held for
// only a few microseconds), then process the local copy without holding
// any lock at all.
// ---------------------------------------------------------------------------
// UART bit-rate simulation.
//
// Root cause (user-confirmed persistent symptoms): notes not terminating,
// dropped/masked notes, and timing struggles specifically when many events
// arrive in a short window (song intro sending Bank/Program/SysEx for many
// channels almost simultaneously). We were flushing the ENTIRE pending
// queue instantly on every drain, i.e. potentially dozens of MIDI bytes hit
// PostMIDI() within the same MCU instant. Real hardware receives these one
// byte at a time over a 38400 baud UART (~260us/byte) - GSPlay->ESP32 is
// itself rate-limited this way, but RTP/WiFi transport can deliver a burst
// of already-elapsed-in-real-time events in one go, and our code replayed
// that burst instantly instead of at the original wire rate.
//
// Fix: keep a persistent pending queue; each call only "spends" the number
// of bytes that would have elapsed over a real 38400bps UART since the last
// call, and leaves the rest queued for subsequent calls (fed every ~1ms by
// mcuLoop). This reproduces the real device's pacing without introducing
// unbounded latency (worst case for a normal few-byte MIDI message is a
// couple of extra ms).
// ---------------------------------------------------------------------------
static std::deque<MidiEv> g_pendingQueue;
static std::chrono::steady_clock::time_point g_lastUartTime = std::chrono::steady_clock::now();
static double g_byteBudget = 0.0;
constexpr double kUartBytesPerMs = 3.84; // 38400 baud, 10 bits/byte -> 3840 B/s
// CONFIRMED via test: disabling pacing (huge budget cap) brought BACK the
// "gunshot"-like wrong-instrument bursts and excessive reverb/notes-not-
// terminating regressions. This proves UART bitrate pacing is NOT the
// cause of the staccato dropped/masked-notes complaint - it is in fact a
// necessary safeguard against those other regressions. Reverted to the
// original, validated cap. The staccato issue must have a different,
// still-unidentified root cause - do not touch this value again without
// new evidence.
// Reverted 128->32: increasing the budget made note dropping WORSE, not
// better - larger budget = larger instantaneous bursts through PostMIDI,
// confirming smaller bursts are better now just as they were before the
// sample-rate fix. Back to the smallest validated value.
constexpr double kUartBudgetCapBytes = 32.0;

static int midiEventByteSize(const MidiEv& ev) {
    if (ev.isSysex && ev.sysex) return (int)ev.sysex->size();
    uint8_t st = ev.packed & 0xFF;
    if ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) return 2;
    if ((st & 0x80) && (st & 0xF0) <= 0xE0)          return 3;
    return 1;
}

static void postOneMidiEvent(const MidiEv& ev) {
    if (ev.isSysex && ev.sysex) {
        auto& v = *ev.sysex;
        if (v.size() >= 4)
            LOGI("SysEx[%zuB]: %02X %02X %02X %02X ...", v.size(),
                 v[0], v[1], v[2], v[3]);
        s_emu.PostMIDI(std::span<const uint8_t>(v.data(), v.size()));
    } else {
        uint8_t st  = ev.packed & 0xFF;
        uint8_t d1  = (ev.packed >> 8)  & 0xFF;
        uint8_t d2  = (ev.packed >> 16) & 0xFF;
        uint8_t msg[3] = { st, d1, d2 };
        int len = 1;
        if ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0)          len = 2;
        else if ((st & 0x80) && (st & 0xF0) <= 0xE0)              len = 3;
        if ((st & 0xF0) == 0xC0) {
            LOGI("ProgChange ch%d prog=%d", (st & 0x0F)+1, d1);
        } else if ((st & 0xF0) == 0xB0) {
            if (d1 == 0 || d1 == 32)
                LOGI("BankSel ch%d cc%d=%d", (st & 0x0F)+1, d1, d2);
            else if (d1 == 98 || d1 == 99 || d1 == 6 || d1 == 38)
                LOGI("NRPN ch%d cc%d=%d", (st & 0x0F)+1, d1, d2);
            else if (d1 == 91 || d1 == 93)
                LOGI("FX ch%d cc%d=%d", (st & 0x0F)+1, d1, d2);
        }
        s_emu.PostMIDI(std::span<const uint8_t>(msg, len));
    }
}

// UART BITRATE PACING REMOVED (v5.5) - confirmed unnecessary and harmful
// after reading the actual reference implementation (nukeykt/Nuked-SC55
// src/mcu.cpp):
//
//   void MCU_PostUART(uint8_t data) {
//       uart_buffer[uart_write_ptr] = data;
//       uart_write_ptr = (uart_write_ptr + 1) % uart_buffer_size;
//   }
//
// The reference PostUART is a trivial 2-line ring-buffer push - no pacing,
// no mutex, nothing. The ACTUAL UART bitrate simulation lives inside the
// MCU core itself: MCU_UpdateUART_RX(), called every MCU cycle, checks
// `mcu.cycles < uart_rx_delay` and only pulls ONE byte at a time from the
// buffer, exactly reproducing real 38400bps timing as part of the
// cycle-accurate emulation. Our v4.5 "UART bitrate pacing" was a complete,
// harmful duplicate of a mechanism the emulator core already implements
// correctly - two independent pacing systems fighting each other, which
// is the most likely explanation for the persistent occasional wrong-
// instrument-bursts/stuck-notes that survived every other fix. Reverting
// to matching upstream: push everything into the core immediately, let the
// core's own UART emulation pace itself.
static void drainMidiQueue() {
    std::deque<MidiEv> local;
    {
        std::lock_guard<std::mutex> lk(g_evMtx);
        local.swap(g_evQ); // lock held only for this swap - microseconds
    }
    while (!local.empty()) {
        postOneMidiEvent(local.front());
        local.pop_front();
    }
}

// ---------------------------------------------------------------------------
// 32kHz -> 48kHz SRC  (3:2 integer ratio, linear interpolation)
// ---------------------------------------------------------------------------
// (old fixed-ratio 3:2 SRC state removed - replaced by dynamic-ratio
//  resampler state declared near sampleCallback)

// Sample counter: pure atomic increment, no mutex, no PostMIDI call here.
// sampleCallback fires DURING Step() execution, so it must stay side-effect
// free w.r.t. MCU state (see mcuLoop comment). mcuLoop polls this counter
// (relaxed atomic load - extremely cheap) to decide when it's time to
// drain the MIDI queue, safely, between Step() calls.
static std::atomic<uint32_t> s_sampleCounterAtomic{0};

// s_srcEnabled: true only if AAudio could NOT give us native 32kHz and we
// had to open at a different rate (typically 48kHz). In that case we do a
// crude linear-interpolation upsample. This is a known source of subtle
// timbre change (linear interpolation acts as a mild low-pass filter and
// can alias) - if the device grants us true 32kHz, we skip this entirely
// and push samples straight through, matching the PC reference bit-for-bit
// (modulo final DAC playback, obviously).
static std::atomic<bool> s_srcEnabled{true};

// Dynamic-ratio linear-interpolation resampler.
// Replaces the old hardcoded "32->48, 3:2" resampler, which was wrong
// because the SC-55 core's real native rate is 66207Hz, not 32000Hz.
// s_resampleRatio = s_scActualRate / kSampleRate (e.g. 66207/48000 ~= 1.38),
// set once in nativeInit after the actual rate is known.
static double    s_resampleRatio     = 1.0;
static double    s_resamplePos       = 0.0;
static StereoS16 s_resamplePrev      = {0, 0};
static bool      s_resampleHaveFirst = false;

static void sampleCallback(void* /*ud*/, const AudioFrame<int32_t>& frame) {
    AudioFrame<int16_t> out;
    Normalize(frame, out);
    StereoS16 cur = {out.left, out.right};

    if (!s_srcEnabled.load(std::memory_order_relaxed)) {
        // AAudio granted the SC-55 core's true native rate - push 1:1,
        // no resampling, no timbre-altering interpolation.
        ring_push(cur);
    } else {
        if (!s_resampleHaveFirst) {
            s_resampleHaveFirst = true;
            s_resamplePrev = cur;
        } else {
            s_resamplePos += 1.0; // one source sample elapsed
            while (s_resamplePos >= s_resampleRatio) {
                double overshoot = s_resamplePos - s_resampleRatio;
                double frac = 1.0 - overshoot; // interpolation weight toward `cur`
                if (frac < 0.0) frac = 0.0;
                if (frac > 1.0) frac = 1.0;
                StereoS16 outSample = {
                    (int16_t)(s_resamplePrev.l + (int32_t)((cur.l - s_resamplePrev.l) * frac)),
                    (int16_t)(s_resamplePrev.r + (int32_t)((cur.r - s_resamplePrev.r) * frac))
                };
                ring_push(outSample);
                s_resamplePos -= s_resampleRatio;
            }
            s_resamplePrev = cur;
        }
    }

    s_sampleCounterAtomic.fetch_add(1, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// AAudio data callback  (audio thread)
// ---------------------------------------------------------------------------
static aaudio_data_callback_result_t audioCallback(
        AAudioStream*, void*, void* audioData, int32_t numFrames)
{
    // NOTE: SCHED_FIFO was removed here - it triggered Linux/Android kernel
    // RT throttling (a SCHED_FIFO thread that keeps demanding CPU gets
    // forcibly paused for a slice of every second), producing periodic
    // audio stutter ("sound cutting in and out rapidly" while musical
    // timing itself stayed correct). AAudio's LOW_LATENCY performance mode
    // already requests appropriate scheduling from the audio server: we
    // don't need to (and shouldn't) fight it with our own SCHED_FIFO calls.
    auto* out = static_cast<int16_t*>(audioData);
    for (int i = 0; i < numFrames; ++i) {
        StereoS16 f{};
        ring_pop(f);
        out[i * 2 + 0] = f.l;
        out[i * 2 + 1] = f.r;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

// ---------------------------------------------------------------------------
// MCU thread
//
// Phase 1 - Warm-up (CRITICAL for SC-55mk2 firmware bug):
//   Run MCU for 64000 samples (~2 sec real-time equivalent) without MIDI.
//   SC-55mk2 firmware bug: parameters don't init correctly until boot done.
//   jcmoyer v0.5.0: "run for a bit before MIDI to ensure ready for prog changes"
//
// Phase 2 - GS Reset via PostSystemReset() (proper API, not raw SysEx bytes)
//
// Phase 3 - Normal: drain MIDI queue + Step() with pacing
// ---------------------------------------------------------------------------
static AAudioStream*     s_stream = nullptr;
static std::atomic<bool> s_running{false};
static std::thread       s_mcuThread;
static bool              s_initialized = false;

// Exposes warmup status to Kotlin (for UI feedback)
static std::atomic<bool> s_warmupDone{false};

static void mcuLoop() {
    // NOTE: SCHED_FIFO was tried here and reverted - it caused Linux/Android
    // kernel RT throttling, which forcibly pauses a demanding SCHED_FIFO
    // thread for a slice of every second. That produced exactly the
    // symptom we were chasing (periodic audio stutter, correct musical
    // timing but sound cutting in/out). A normal (non-realtime) thread
    // with SCHED_OTHER, combined with the ring-buffer pacing below, is
    // sufficient and avoids kernel-level throttling entirely.

    // FINAL VERDICT (v5.7): auto GS Reset via PostSystemReset() has now
    // been tried THREE times (v4.3, v4.7, v5.6), across completely
    // different states of the rest of the codebase (before/after the
    // isNoteOff fix, before/after the sample-rate fix, before/after
    // removing the duplicate UART pacing layer) - and every single time
    // produced the exact same signature regression: excessive reverb and
    // notes not terminating cleanly. This consistency across otherwise
    // very different conditions means the effect is intrinsic to how we
    // invoke PostSystemReset(GS_RESET) itself (most likely: it resets
    // reverb/chorus send levels to a stronger-than-expected default, or
    // interacts badly with per-part reverb state at the specific moment we
    // call it - moments before real playback data arrives). This is
    // independent of the ("BY" disconnect) issue, which stayed fixed.
    // The actual MIDI stream's own GS Reset (confirmed via logs to arrive
    // correctly, in order, unmodified) is sufficient. Do not re-attempt
    // this again without a fundamentally different theory of why it
    // reliably alters reverb/note-termination behavior.
    s_emu.SetSampleCallback(sampleCallback, nullptr);
    s_warmupDone.store(true);

    // Start the LCD (the real SC-55 LCD controller emulation). Must be
    // called after Reset(). If it fails we just log and continue - LCD is
    // cosmetic, not audio-critical.
    if (s_emu.StartLCD()) {
        s_lcdStarted.store(true);
        s_lcdThreadRunning.store(true);
        s_lcdThread = std::thread(lcdThreadLoop);
        LOGI("LCD started (dedicated thread)");
    } else {
        LOGW("LCD start failed (non-fatal, continuing without LCD)");
    }

    // ------------------------------------------------------------------
    // Active Sensing (0xFE) injection.
    //
    // Known issue (nukeykt/Nuked-SC55 #106): the emulated SC-55 MCU expects
    // Active Sensing messages at a certain rate. Real physical MIDI cables
    // send 0xFE periodically (typically <=300ms) to indicate the connection
    // is alive; if the MCU doesn't see one in time it can flag "MIDI Off
    // Line" internally, which resets/reinitializes part state - causing
    // exactly the symptom we're chasing (correct MIDI bytes arrive, but the
    // wrong instrument/drum kit plays after some elapsed time).
    //
    // RTP-MIDI streams commonly do NOT carry Active Sensing at all (it's
    // meaningless over a reliable network transport), so GSPlay/ESP32 may
    // never send 0xFE - starving the emulated MCU of a signal it silently
    // expects. We inject our own 0xFE every 200ms as a safety net; this is
    // harmless if the MCU doesn't need it, and fixes the timeout if it does.
    // ------------------------------------------------------------------
    // Active Sensing (0xFE) injection DISABLED for testing.
    // Timeline evidence points to this being the actual cause of the audio
    // stutter: stutter first appeared exactly when this was introduced
    // (alongside SCHED_FIFO), and removing SCHED_FIFO alone (keeping this)
    // made stutter WORSE rather than better - meaning SCHED_FIFO was
    // masking a cost created by this PostMIDI(0xFE) call, not fixing an
    // unrelated scheduling problem. Disabling entirely to verify.
    [[maybe_unused]] auto lastActiveSensing = std::chrono::steady_clock::now();
    [[maybe_unused]] constexpr auto kActiveSensingInterval = std::chrono::milliseconds(200);

    // PERFORMANCE-CRITICAL FIX: do NOT call drainMidiQueue() (which locks a
    // mutex) on every single Step() call. Step() may represent one MCU
    // cycle and can be called extremely often (potentially millions of
    // times/sec) - locking a mutex that often causes severe slowdown,
    // starving the MCU of real-time progress and producing audio
    // stutter/glitching (correct musical timing, but sound cutting in
    // and out rapidly) even though the emulation itself is not behind.
    //
    // Instead: sampleCallback only increments an atomic counter (cheap).
    // We poll that counter here (relaxed load, no lock) and only call the
    // real drain (which takes the mutex) once per ~32 samples (~1ms @
    // 32kHz) - same cadence as before, but checked without blocking Step().
    // Adjusted from 32 to 66: kDrainEverySamples was tuned for the WRONG
    // assumed rate (32000Hz, where 32 samples = 1ms). At the real native
    // rate (66207Hz), 66 samples ~= 1ms, preserving the original drain
    // cadence now that the rate assumption is corrected.
    constexpr uint32_t kDrainEverySamples = 66;
    uint32_t lastDrainSample = s_sampleCounterAtomic.load(std::memory_order_relaxed);

    // Diagnostics: log ring buffer overflow/underrun + MCU throughput once/sec
    auto lastDiagLog = std::chrono::steady_clock::now();
    uint32_t lastDiagSample = s_sampleCounterAtomic.load(std::memory_order_relaxed);

    constexpr int kHighWater = (RING_FRAMES * 3) / 4;
    while (s_running.load(std::memory_order_relaxed)) {
        while (ring_size() >= kHighWater && s_running.load())
            std::this_thread::sleep_for(std::chrono::microseconds(500));

        uint32_t curSample = s_sampleCounterAtomic.load(std::memory_order_relaxed);
        if (curSample - lastDrainSample >= kDrainEverySamples) {
            lastDrainSample = curSample;

            auto now = std::chrono::steady_clock::now();
            // Active Sensing injection disabled - see comment above.
            // if (now - lastActiveSensing >= kActiveSensingInterval) {
            //     lastActiveSensing = now;
            //     uint8_t activeSensing = 0xFE;
            //     s_emu.PostMIDI(std::span<const uint8_t>(&activeSensing, 1));
            // }

            drainMidiQueue();  // safe: MCU is between steps here, not executing

            // LCD rendering moved OFF the MCU thread entirely (see lcdThreadLoop) -
            // LCD_Render() does real work (background compositing, font
            // rendering, level meters over a 741x268 framebuffer) and
            // calling it here caused the MCU thread to stall long enough to
            // trigger RTP "BY" disconnects, audio note overlap, and worse
            // instrument-selection nondeterminism. LCD now runs on its own
            // independent thread; lcd_t's own internal mutex keeps it safe.

            // Once-per-second diagnostics
            if (now - lastDiagLog >= std::chrono::seconds(1)) {
                uint32_t samplesThisSec = curSample - lastDiagSample;
                lastDiagSample = curSample;
                lastDiagLog = now;
                uint32_t ovf = g_ringOverflowCount.exchange(0);
                uint32_t udr = g_ringUnderrunCount.exchange(0);
                uint64_t dedupMidi = g_midiDedupDropped.exchange(0);
                uint64_t dedupSysex = g_sysexDedupDropped.exchange(0);
                LOGI("DIAG: mcu_samples/s=%u (target=%u) ring_overflow=%u ring_underrun=%u ring_fill=%d/%d dedup_midi=%llu dedup_sysex=%llu",
                     samplesThisSec, s_scActualRate, ovf, udr, ring_size(), RING_FRAMES,
                     (unsigned long long)dedupMidi, (unsigned long long)dedupSysex);
            }
        }

        s_emu.Step();
    }
}

// ---------------------------------------------------------------------------
// ROM loading helper
// ---------------------------------------------------------------------------
static bool loadRoms(const std::string& romDir, Romset romset) {
    std::filesystem::path basePath(romDir);
    RomsetInfo info;
    SetRomsetFilenames(info, basePath, romset, ROMLOCATION_ALL);

    RomCompletionStatusSet completion;
    if (!IsCompleteRomset(info, romset, &completion)) {
        LOGE("Incomplete ROM set in %s", romDir.c_str());
        return false;
    }
    RomLoadStatusSet loaded;
    if (!LoadRomset(info, &loaded)) {
        LOGE("LoadRomset failed"); return false;
    }
    RomLocationSet locSet{};
    if (!s_emu.LoadRoms(romset, info, &locSet)) {
        LOGE("Emulator::LoadRoms failed"); return false;
    }
    LOGI("ROM loaded OK from %s", romDir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// JNI exports
// ---------------------------------------------------------------------------
extern "C" {

// nativeInit: ROM load + emulator init only (FAST - safe on UI thread)
JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeInit(
        JNIEnv* env, jobject, jstring jRomDir, jint /*modelId*/)
{
    if (s_initialized) return JNI_TRUE;

    const char* cDir = env->GetStringUTFChars(jRomDir, nullptr);
    std::string romDir(cDir);
    env->ReleaseStringUTFChars(jRomDir, cDir);
    LOGI("nativeInit: romDir=%s", romDir.c_str());

    EMU_Options opts; opts.lcd_backend = &s_lcdBackend;
    if (!s_emu.Init(opts)) { LOGE("Emulator::Init failed"); return JNI_FALSE; }
    if (!loadRoms(romDir, Romset::MK2)) return JNI_FALSE;

    // CRITICAL DIAGNOSTIC: check the ACTUAL native output frequency the
    // emulator core produces. We have been hardcoding 32000 this whole
    // time based on assumption, but jcmoyer's own docs state SC-55mk2
    // natively produces 64000Hz or 66207Hz (oversampled by default -
    // pcm_t::enable_oversampling defaults to true), with 32000/33103Hz
    // only available via --disable-oversampling. If our assumed rate has
    // been wrong, our ring buffer pacing, SRC ratio, and UART budget
    // timing calculations have ALL been operating on incorrect timing
    // assumptions - which could explain persistent, hard-to-pin-down
    // timing artifacts (dropped/masked/staccato notes) that survived every
    // other fix so far.
    s_scActualRate = PCM_GetOutputFrequency(s_emu.GetPCM());
    LOGI("SC-55 actual native output rate = %u Hz", s_scActualRate);

    // Reset emulator (MCU starts boot sequence internally)
    s_emu.Reset();

    // Try to open AAudio at SC-55's NATIVE 32kHz first. If the device's
    // audio server grants it (even via its own internal resampling to the
    // hardware rate - AudioFlinger's resampler is much higher quality than
    // our crude linear-interpolation fallback), we skip our manual SRC
    // entirely and push samples straight through, which should sound
    // identical to the PC reference. Only if 32kHz is flatly rejected do we
    // fall back to opening at 48kHz and doing our own (lower quality) SRC.
    auto openStream = [](AAudioStream** st, aaudio_sharing_mode_t mode, int32_t rate) {
        AAudioStreamBuilder* b = nullptr;
        AAudio_createStreamBuilder(&b);
        AAudioStreamBuilder_setDirection           (b, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode     (b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode         (b, mode);
        AAudioStreamBuilder_setSampleRate          (b, rate);
        AAudioStreamBuilder_setChannelCount        (b, kChannels);
        AAudioStreamBuilder_setFormat              (b, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setFramesPerDataCallback(b, kFramesBurst);
        AAudioStreamBuilder_setDataCallback        (b, audioCallback, nullptr);
        aaudio_result_t r = AAudioStreamBuilder_openStream(b, st);
        AAudioStreamBuilder_delete(b);
        return r;
    };

    // Attempt order: 32kHz SHARED (let AudioFlinger resample if needed -
    // SHARED mode is what allows the mixer to do this, unlike EXCLUSIVE)
    // -> 32kHz EXCLUSIVE -> 48kHz SHARED (our manual SRC) -> 48kHz EXCLUSIVE.
    aaudio_result_t res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, (int32_t)s_scActualRate);
    if (res != AAUDIO_OK) {
        LOGW("Native rate SHARED failed (%s), trying native rate EXCLUSIVE", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_EXCLUSIVE, (int32_t)s_scActualRate);
    }
    if (res != AAUDIO_OK) {
        LOGW("Native rate EXCLUSIVE failed (%s), falling back to 48kHz SHARED + manual SRC",
             AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_SHARED, kSampleRate);
    }
    if (res != AAUDIO_OK) {
        LOGW("48kHz SHARED failed (%s), trying 48kHz EXCLUSIVE", AAudio_convertResultToText(res));
        res = openStream(&s_stream, AAUDIO_SHARING_MODE_EXCLUSIVE, kSampleRate);
    }
    if (res != AAUDIO_OK) {
        LOGE("AAudio open failed: %s", AAudio_convertResultToText(res));
        return JNI_FALSE;
    }

    int32_t actualRate = AAudioStream_getSampleRate(s_stream);
    bool nativeRate = (actualRate == (int32_t)s_scActualRate);
    s_srcEnabled.store(!nativeRate);
    if (!nativeRate) {
        // Resampling from the SC-55 core's actual native rate to whatever
        // rate AAudio granted us (actualRate, e.g. 48000).
        s_resampleRatio = (double)s_scActualRate / (double)actualRate;
        s_resamplePos = 0.0;
        s_resampleHaveFirst = false;
        LOGI("Resample ratio set: %u Hz -> %d Hz (ratio=%.6f)",
             s_scActualRate, actualRate, s_resampleRatio);
    }
    LOGI("AAudio opened @ actual rate=%d (native %uHz=%s, manual SRC %s)",
         actualRate, s_scActualRate, nativeRate ? "YES" : "NO", nativeRate ? "DISABLED" : "ENABLED");

    s_warmupDone.store(false);
    s_initialized = true;
    LOGI("nativeInit OK");
    return JNI_TRUE;
}

// nativeStart: launch MCU thread (warmup + GS Reset happen inside mcuLoop)
JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeStart(JNIEnv*, jobject)
{
    if (!s_initialized || s_running.load()) return;
    s_running = true;
    s_mcuThread = std::thread(mcuLoop);
    AAudioStream_requestStart(s_stream);
    LOGI("nativeStart: MCU thread + AAudio started (warmup in progress)");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeStop(JNIEnv*, jobject)
{
    if (!s_running.load()) return;
    s_running = false;
    if (s_stream) AAudioStream_requestStop(s_stream);
    if (s_mcuThread.joinable()) s_mcuThread.join();
    LOGI("nativeStop");
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeTerm(JNIEnv*, jobject)
{
    if (!s_initialized) return;
    if (s_running.load()) {
        s_running = false;
        if (s_stream) AAudioStream_requestStop(s_stream);
        if (s_mcuThread.joinable()) s_mcuThread.join();
    }
    if (s_lcdThreadRunning.load()) {
        s_lcdThreadRunning.store(false);
        if (s_lcdThread.joinable()) s_lcdThread.join();
    }
    if (s_lcdStarted.load()) {
        s_emu.StopLCD();
        s_lcdStarted.store(false);
    }
    if (s_stream) { AAudioStream_close(s_stream); s_stream = nullptr; }
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    g_pendingQueue.clear();
    g_byteBudget = 0.0;
    s_initialized = false;
    s_warmupDone.store(false);
    LOGI("nativeTerm");
}

// ---------------------------------------------------------------------------
// nativeGetLcdFrame: copy the current LCD framebuffer (lcd_t::buffer, ARGB
// pixels produced by the real SC-55 LCD controller emulation) into an
// Android Bitmap (must be ARGB_8888, sized to match lcd.width x lcd.height
// - query via nativeGetLcdWidth/Height first). Called from Kotlin at ~30fps
// to display a pixel-accurate replica of the real hardware LCD.
// ---------------------------------------------------------------------------
// Scratch buffer for the raw LCD framebuffer copy - reused across calls to
// avoid per-frame heap allocation.
static std::vector<uint32_t> s_lcdScratch;

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetLcdFrame(JNIEnv* env, jobject, jobject bitmap)
{
    if (!s_initialized || !s_lcdStarted.load()) return JNI_FALSE;

    AndroidBitmapInfo info;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) return JNI_FALSE;
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) return JNI_FALSE;

    lcd_t& lcd = s_emu.GetLCD();
    if (info.width < lcd.width || info.height < lcd.height) return JNI_FALSE;

    // CRITICAL FIX: LCD_Render() uses lcd.mutex.try_lock() and silently
    // DROPS the frame if the lock is unavailable (see lcd.cpp: "if the MCU
    // is currently updating something, just drop the frame"). Our old code
    // held lcd.mutex for the ENTIRE pixel loop (741x268 = ~200k pixels with
    // per-pixel bit-shuffling) - a relatively long time to hold a mutex that
    // the render thread polls with try_lock() at 30fps, causing frequent
    // dropped LCD_Render() calls and visible flicker. Fix: hold the mutex
    // only for a raw memcpy of the framebuffer (fast), then do the
    // byte-order conversion afterward with no lock held at all.
    size_t pixelCount = lcd.height * lcd.width;
    if (s_lcdScratch.size() < pixelCount) s_lcdScratch.resize(pixelCount);

    {
        std::lock_guard<std::mutex> lk(lcd.mutex);
        for (size_t y = 0; y < lcd.height; ++y) {
            memcpy(&s_lcdScratch[y * lcd.width], lcd.buffer[y], lcd.width * sizeof(uint32_t));
        }
    }

    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) return JNI_FALSE;

    uint32_t stridePixels = info.stride / 4;
    uint32_t* dst = static_cast<uint32_t*>(pixels);
    for (size_t y = 0; y < lcd.height; ++y) {
        uint32_t* row = dst + y * stridePixels;
        const uint32_t* srcRow = &s_lcdScratch[y * lcd.width];
        for (size_t x = 0; x < lcd.width; ++x) {
            // lcd.buffer format under test: try swapping R/B (BGR hypothesis)
            // since raw palette values (e.g. SC-55mk2 lcd.color2=0x0050c8)
            // only make sense as the real orange SC-55 LCD color if read as
            // B,G,R rather than R,G,B (0x0050c8 as BGR = RGB(200,80,0), a
            // clear orange - as RGB it's blue, which is what we saw on screen).
            uint32_t argb = srcRow[x];
            uint8_t a = (argb >> 24) & 0xFF;
            uint8_t comp1 = (argb >> 16) & 0xFF; // was treated as R
            uint8_t comp2 = (argb >> 8)  & 0xFF; // G
            uint8_t comp3 =  argb        & 0xFF; // was treated as B
            // Swapped: comp1 is actually B, comp3 is actually R
            uint8_t r = comp3;
            uint8_t g = comp2;
            uint8_t b = comp1;
            row[x] = (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
        }
    }

    AndroidBitmap_unlockPixels(env, bitmap);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetLcdWidth(JNIEnv*, jobject)
{
    if (!s_initialized) return 741; // SC-55mk2 default, safe fallback
    return (jint)s_emu.GetLCD().width;
}

JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetLcdHeight(JNIEnv*, jobject)
{
    if (!s_initialized) return 268;
    return (jint)s_emu.GetLCD().height;
}

// Packed int MIDI (same as munt-android convention)
// Time-windowed duplicate filter: ESP32 firmware sends every MIDI event twice
// as two back-to-back RTP packets (same content, different seq numbers,
// typically <1ms apart). We only drop a repeat if it arrives within
// kDedupWindowMs of the previous IDENTICAL event - this catches the ESP32
// double-send bug while still allowing legitimate fast repeated notes
// (e.g. rapid hi-hat hits with identical velocity) which arrive tens of
// milliseconds apart in real music.
//
// Previous version compared values with no time window, which incorrectly
// dropped intentional repeated notes -> caused "notes that should stop
// keep ringing" bug (the repeat Note On that should re-trigger/replace the
// decaying note was silently dropped).
static uint32_t g_lastMidiPacked = 0xFFFFFFFF;
static std::chrono::steady_clock::time_point g_lastMidiTime{};
// Narrowed from 3ms to 1ms: earlier logs confirmed the actual ESP32
// double-send lands within ~1ms of the original. A 3ms window was wider
// than necessary and risked swallowing legitimate fast repeated notes
// (staccato/tremolo passages), which matches the user's persistent
// "notes dropped/masked" report even after the isNoteOff fix. Narrowing
// the window keeps the ESP32 bug workaround while reducing false positives.
static constexpr int kDedupWindowMs = 1;

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeSendMidi(JNIEnv*, jobject, jint packed)
{
    if (!s_initialized) return;
    uint32_t p = (uint32_t)packed;
    uint8_t status = p & 0xFF;
    // CRITICAL FIX: this file's MIDI encodes ALL note-offs as
    // "Note On, velocity=0" (0x9X note 0x00) - a very common MIDI encoding
    // convention - and NEVER uses the explicit 0x8X Note Off status at all
    // (verified: 3521 note-off events, 0 of them 0x8X, all of them 0x9X
    // vel=0). The old check `(status & 0xF0) == 0x80` therefore NEVER
    // matched in practice, meaning every note-off in this file was treated
    // as an ordinary event and subject to dedup - if the real ESP32
    // double-send happened to land >3ms apart (missed by the dedup window)
    // OR if a genuine note-off got unlucky and matched some other filter
    // path, notes could end up not being turned off cleanly, or (more
    // likely per the user's report) other legitimate short-window repeats
    // got over-aggressively treated. Recognizing vel=0 Note-On as a real
    // note-off closes this gap: note-offs are idempotent (turning off an
    // already-off note is harmless), so ALWAYS letting them through
    // (bypassing dedup entirely) is safe and fixes potential stuck/cut
    // notes without any downside.
    uint8_t d2check = (p >> 16) & 0xFF;
    bool isNoteOff  = ((status & 0xF0) == 0x80) ||
                      (((status & 0xF0) == 0x90) && d2check == 0);
    bool isRealTime = status >= 0xF8;

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_lastMidiTime).count();

    // DISABLED: diagnostics on v5.3 showed this dedup filter dropping
    // 7-148 MIDI events PER SECOND during normal playback (dedup_midi
    // counter) - far too many to be genuine ESP32 double-sends, meaning it
    // was routinely discarding legitimate fast-repeated notes and
    // Program-Change/Bank-Select events. This is almost certainly the
    // actual cause of "dropped notes" and "occasionally different
    // instrument on repeat playback" (a dropped Program Change/Bank Select
    // leaves the previous patch active for that channel). Now that the
    // real root cause (wrong assumed sample rate, v5.1) is fixed, the
    // ESP32 double-send problem this was meant to guard against may not
    // even manifest as an audible issue anymore - disabling entirely to
    // verify, matching upstream/PC frontend behavior (no dedup at all).
    bool isDuplicate = false;
    (void)elapsedMs;

    g_lastMidiPacked = p;
    g_lastMidiTime   = now;

    if (isDuplicate) {
        g_midiDedupDropped.fetch_add(1, std::memory_order_relaxed);
        return; // drop ESP32 double-send only
    }

    ++g_midiCount;
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({false, p, nullptr});
}

static std::vector<uint8_t> g_lastSysex;
static std::chrono::steady_clock::time_point g_lastSysexTime{};

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeSendSysEx(JNIEnv* env, jobject, jbyteArray data, jint len)
{
    if (!s_initialized || len <= 0) return;
    jbyte* buf = env->GetByteArrayElements(data, nullptr);
    std::vector<uint8_t> v_data((uint8_t*)buf, (uint8_t*)buf + len);
    env->ReleaseByteArrayElements(data, buf, JNI_ABORT);

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_lastSysexTime).count();

    bool isDuplicate = (v_data == g_lastSysex) && (elapsedMs < kDedupWindowMs);
    g_lastSysex = v_data;
    g_lastSysexTime = now;

    if (isDuplicate) {
        g_sysexDedupDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    ++g_sysexCount;
    auto v = std::make_shared<std::vector<uint8_t>>(std::move(v_data));
    std::lock_guard<std::mutex> lk(g_evMtx);
    g_evQ.push_back({true, 0, v});
}

JNIEXPORT void JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeResetSynth(JNIEnv*, jobject)
{
    { std::lock_guard<std::mutex> lk(g_evMtx); g_evQ.clear(); }
    g_pendingQueue.clear();
    g_byteBudget = 0.0;
    if (!s_initialized || !s_warmupDone.load()) return;
    for (int ch = 0; ch < 16; ch++) {
        std::lock_guard<std::mutex> lk(g_evMtx);
        g_evQ.push_back({false, (uint32_t)((0xB0|ch)|(123u<<8)), nullptr});
        g_evQ.push_back({false, (uint32_t)((0xB0|ch)|(120u<<8)), nullptr});
    }
    s_emu.PostSystemReset(EMU_SystemReset::GS_RESET);
    LOGI("Manual GS Reset via PostSystemReset");
}

JNIEXPORT jboolean JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeIsWarmupDone(JNIEnv*, jobject)
{
    return s_warmupDone.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetRomList(JNIEnv* env, jobject, jint)
{
    RomsetInfo info;
    SetRomsetFilenames(info, std::filesystem::path("/dummy"), Romset::MK2, ROMLOCATION_ALL);
    std::string result;
    for (int i = 0; i < (int)ROMLOCATION_COUNT; ++i) {
        if (!info.rom_paths[i].empty()) {
            if (!result.empty()) result += "|";
            result += info.rom_paths[i].filename().string();
        }
    }
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetStats(JNIEnv* env, jobject)
{
    size_t qsz; { std::lock_guard<std::mutex> lk(g_evMtx); qsz = g_evQ.size(); }
    char buf[128];
    snprintf(buf, sizeof(buf), "midi=%llu sx=%llu q=%zu ring=%d warm=%d",
             (unsigned long long)g_midiCount.load(),
             (unsigned long long)g_sysexCount.load(),
             qsz, ring_size(), (int)s_warmupDone.load());
    return env->NewStringUTF(buf);
}

JNIEXPORT jstring JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetVersion(JNIEnv* env, jobject)
{
    return env->NewStringUTF("Nuked SC-55 for Android v6.12 (shorter watchdog timeouts + robustness, longer LCD quiescence window)");
}

// IEngine 공통 계약용 신규 accessor (통합작업순서.md Phase 1) — 엔진 전환 시
// AAudio 스트림을 이 엔진의 진짜 네이티브 레이트로 열 수 있도록 노출.
JNIEXPORT jint JNICALL
Java_com_example_nukedsc55_SC55Engine_nativeGetSampleRate(JNIEnv*, jobject)
{
    return (jint)s_scActualRate;
}

} // extern "C"
