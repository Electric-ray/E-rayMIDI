use std::sync::{Arc, mpsc::SyncSender};
use wd_log::{log_debug_ln, log_warn_ln};

use crate::audio::{AudioRenderActions, AudioShared};
use crate::double_buffer::DoubleBuffered;
use crate::config::Config;
use super::interface::PitchGetter;
use super::active_sensing::ActiveSensingState;
use crate::midi::ram::MemoryAddr;
use crate::midi::{
    MIDICallbackEffects,
    consts::{DEFAULT_MASTER_TUNING, DEFAULT_MASTER_VOLUME, MAX_PART_SIZE},
    event::MidiEvent,
    interface::EventParser,
    note::Note,
    part::Part,
    ram::{RAM, interface::Memory},
    sysex::{
        Event, ManufacturerId, gm::GeneralMIDISysEx, realtime::UniversalRealtimeSysEx,
        roland::RolandSysEx, yamaha::YamahaSysEx,
    },
};
use crate::voice_manager::DRUM_BANK_MSB_GS;
use crate::voice_manager::VoiceManager;

pub type NoteCentTable = [[[f32; 128]; 128]; 128];

#[derive(Debug)]
pub struct Engine {
    /// Master volume for SysEx 0x7F, little endian
    /// Default = 0x4000 (MAX)
    pub master_volume: u16,
    /// Master tuning, in cents.
    /// Default = 0x2000 (0)
    pub master_tuning: u16,
    pub note_cent_table: NoteCentTable,
    pub dev_id: u8,

    pub parts: Box<[Arc<DoubleBuffered<Part>>]>,

    pub ram: RAM,

    pub voice_manager: VoiceManager,
    /// Active Sensing heartbeat state + watchdog (passive; never sends 0xFE)
    pub active_sensing: Arc<ActiveSensingState>,

    pub chan_tx: SyncSender<AudioRenderActions>,
    /// GM2/GM1 master volume (14-bit), double-buffered for the audio thread
    pub audio_master_volume: Arc<DoubleBuffered<u16>>,
}

impl Engine {
    pub fn new(cfg: &Config, tx: SyncSender<AudioRenderActions>) -> Self {
        let voice_manager = VoiceManager::load_tbl(&cfg.sound_module).unwrap();
        log_debug_ln!("voice manager ready");
        
        let drum_data = voice_manager
            .get_drum_setup(DRUM_BANK_MSB_GS as u8, 0)
            .unwrap();
        log_debug_ln!("drum setup loaded");
        
        let ram = RAM::new(MidiResetMode::GM, drum_data);
        log_debug_ln!("engine ram ready");

        let parts: Vec<Arc<DoubleBuffered<Part>>> = (0..MAX_PART_SIZE)
            .map(|i| {
                Arc::new(DoubleBuffered::new(Part::new(
                    i,
                    &voice_manager,
                    ram.xg.multi_part[i].clone(),
                    ram.xg.multi_part_ext[i].clone(),
                )))
            })
            .collect();
        log_debug_ln!("parts ready, we have {} parts", MAX_PART_SIZE);

        let active_sensing = Arc::new(ActiveSensingState::new(500));
        // Watchdog resets parts + releases audio on heartbeat timeout (passive, never sends 0xFE)
        active_sensing.spawn_watchdog(parts.clone(), tx.clone());
        log_debug_ln!("watdog running for active sensing");

        Self {
            master_volume: DEFAULT_MASTER_VOLUME,
            note_cent_table: NOTE_CENT_TABLE,
            dev_id: cfg.midi.device_id,
            parts: parts.into(),
            ram,
            voice_manager,
            active_sensing,
            chan_tx: tx,
            audio_master_volume: Arc::new(DoubleBuffered::new(DEFAULT_MASTER_VOLUME)),
            master_tuning: DEFAULT_MASTER_TUNING,
        }
    }

    pub fn get_sample_playspeed_ratio(&self, channel: usize, sample: f32, note: usize) -> f32 {
        let part = self.parts[channel].snapshot();
        let current_tune_bank = part.rpn.tuning_bank_select;
        let current_tune_prog = part.rpn.tuning_prog_select;
        self.note_cent_table[current_tune_bank as usize][current_tune_prog as usize][note] / sample
    }

    pub fn on_event(&mut self, ev: MidiEvent) {
        log_debug_ln!("got midi event {:?}", ev);
        let callbacks = match ev {
            MidiEvent::SysEx {
                manufacturer_id,
                data,
            } => self.on_sysex(manufacturer_id, data),
            MidiEvent::ControlChange {
                channel,
                controller,
                value,
            } => self.on_controller(channel, controller, value),
            MidiEvent::RPN {
                channel,
                parameter,
                value,
            } => self.on_rpn(channel, parameter, value),
            MidiEvent::NRPN {
                channel,
                parameter,
                value,
            } => self.on_nrpn(channel, parameter, value),
            MidiEvent::ProgramChange { channel, program } => {
                self.on_program_change(channel, program)
            }
            MidiEvent::PitchBend { channel, value } => self.on_pitchbend(channel, value),
            MidiEvent::ChannelPressure { channel, pressure } => self.on_cat(channel, pressure),
            MidiEvent::PolyPressure {
                channel,
                note,
                pressure,
            } => self.on_pat(channel, note, pressure),
            MidiEvent::NoteOn {
                channel,
                note,
                velocity,
                off_velocity: _,
                duration: _,
            } => {
                if velocity == 0 {
                    // MIDI spec: NoteOn with velocity 0 is a NoteOff
                    self.find_all_part_arcs(channel).iter().for_each(|part| {
                        let _ = self.chan_tx.send(AudioRenderActions::Release {
                            note,
                            part: part.clone(),
                        });
                    });
                } else {
                    self.find_all_part_arcs(channel).iter().for_each(|part| {
                        let _ = self.chan_tx.send(AudioRenderActions::Play {
                            note,
                            vel: velocity,
                            part: part.clone(),
                        });
                    });
                }
                vec![]
            }
            MidiEvent::NoteOff {
                channel,
                note,
                velocity: _,
                off_velocity: _,
                duration: _,
            } => {
                self.find_all_part_arcs(channel).iter().for_each(|part| {
                    let _ = self.chan_tx.send(AudioRenderActions::Release {
                        note,
                        part: part.clone(),
                    });
                });
                vec![]
            }
            MidiEvent::ActiveSensing => self.on_active_sensing(),

            MidiEvent::PortUnsubscribed => {
                // The MIDI source disconnected: release every part's notes
                // (CC#123 All Notes Off semantics — including notes held by
                // the sustain pedal, so no voice can hang after a disconnect).
                self.parts.iter().for_each(|p| {
                    let _ = self.chan_tx.send(AudioRenderActions::ReleaseAll {
                        part: p.clone(),
                    });
                });
                vec![]
            }

            _ => {
                log_warn_ln!("non supported event: {:?}", ev);
                vec![]
            }
        };

        self.hook_exec(callbacks);

        // Commit double buffers (swap everything once at end of batch processing)
        self.parts.iter().for_each(|p| p.swap());
        self.ram.xg.multi_part.iter().for_each(|m| m.swap());
        self.ram.xg.multi_part_ext.iter().for_each(|m| m.swap());
        self.ram.xg.system.swap();
        self.ram.xg.effect1.swap();
        self.ram.xg.multi_eq.swap();
        self.ram.xg.effect_instertion.swap();
        self.ram.xg.drum_setup.swap();
        self.audio_master_volume.swap();
    }

    /// Send shared effect/system parameters to the audio thread (call once after engine start)
    pub fn send_audio_init(&self) {
        let shared = AudioShared {
            system: self.ram.xg.system.clone(),
            effect1: self.ram.xg.effect1.clone(),
            multi_eq: self.ram.xg.multi_eq.clone(),
            effect_instertion: self.ram.xg.effect_instertion.clone(),
            drum_setup: self.ram.xg.drum_setup.clone(),
            master_volume: self.audio_master_volume.clone(),
        };
        let _ = self.chan_tx.send(AudioRenderActions::Init { shared });
        log_debug_ln!("audio engine init signal sent");
    }

    fn on_active_sensing(&mut self) -> Vec<MIDICallbackEffects> {
        // Passive: record the heartbeat; the watchdog resets on timeout.
        // We never transmit 0xFE ourselves.
        self.active_sensing.beat();
        vec![]
    }

    pub fn find_all_parts(&self, channel: u8) -> Box<[usize]> {
        self.parts
            .iter()
            .enumerate()
            .filter(|(_, p)| p.snapshot().ram.snapshot().rcv_channel == channel)
            .map(|(i, _)| i)
            .collect()
    }

    /// Arc references of the parts on the same channel (for sending to the audio thread)
    pub fn find_all_part_arcs(&self, channel: u8) -> Box<[Arc<DoubleBuffered<Part>>]> {
        self.parts
            .iter()
            .filter(|p| p.snapshot().ram.snapshot().rcv_channel == channel)
            .cloned()
            .collect()
    }

    pub fn reset(&mut self, mode: MidiResetMode) {
        self.ram.reset_mode = mode;
        self.ram.reset();
        let master_tuning = tuning_14bit_to_xg(self.master_tuning);
        self.ram.xg.system.write_with(|s| s.set_master_tune(master_tuning));
        self.parts.iter().for_each(|p| {
            p.write_with(|p| p.reset(&self.voice_manager));
        });
    }

    #[allow(dead_code)] // pitch helper for cent table lookups (delta-pitch line commented out in caller)
    fn note_to_cent(&self, _note: Note, _part_id: usize) -> (u8, f32) {
        let part = self.parts[_part_id].snapshot();
        let _note = (_note as i8 + part.get_coarse()).clamp(0x00, 0x7F) as u8;
        let note_cent = self.note_cent_table[part.rpn.tuning_bank_select as usize]
            [part.rpn.tuning_prog_select as usize][_note as usize];

        (
            _note,
            note_cent, //    + part.get_delta_pitch(_note)
                      //    + (self.ram.xg.system.get_master_tune() as f32 - 0x0400 as f32) / 10.0,
        )
    }
}

impl EventParser for Engine {
    fn on_sysex(&mut self, mfid: ManufacturerId, data: Box<[u8]>) -> Vec<MIDICallbackEffects> {
        match mfid {
            ManufacturerId::UniversalRealTime => UniversalRealtimeSysEx::parse(self, data),
            ManufacturerId::UniversalNonRealTime => GeneralMIDISysEx::parse(self, data),
            ManufacturerId::Yamaha => YamahaSysEx::parse(self, data),
            ManufacturerId::Roland => RolandSysEx::parse(self, data),
            _ => {
                log_debug_ln!("non-supported manufacturer, ignore");
                vec![]
            }
        }
    }

    fn on_controller(&mut self, channel: u8, cc: u8, value: u8) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_controller(channel, cc, value));
                effects
            })
            .collect()
    }

    fn on_program_change(&mut self, channel: u8, program: u8) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_program_change(channel, program));

                // BUGFIX(Android 포팅 중 발견): on_program_change는 RAM에 새
                // program 번호만 기록할 뿐, 실제 NoteOn에서 쓰이는
                // program_entry(=Arc<Program>, 진짜 샘플 데이터 참조)는 절대
                // 갱신하지 않았다 — Part::set_program()이 Part::new() 최초
                // 1회 말고는 코드베이스 어디서도 호출되지 않는 죽은 코드였다.
                // 즉 Bank Select/Program Change가 MIDI로 아무리 들어와도 각
                // 파트는 초기 기본 악기(멜로디는 Acoustic Grand Piano, 드럼
                // 채널은 GS 드럼뱅크 킷0)에 영원히 고정되어 있었다 — "다른
                // 악기 소리가 안 난다", "드럼이 피아노로 들린다" 둘 다 이
                // 버그였다. bank_msb/lsb는 이전(이미 커밋된) CC0/CC32
                // 이벤트에서 읽고, program은 방금 받은 값을 그대로 쓴다.
                let ram = part.snapshot().get_ram();
                let bank_msb = ram.get(MemoryAddr::new(0x08, i as u8, 0x01)).unwrap_or(0);
                let bank_lsb = ram.get(MemoryAddr::new(0x08, i as u8, 0x02)).unwrap_or(0);
                part.write_with(|p| p.set_program(&self.voice_manager, bank_msb, bank_lsb, program));

                effects
            })
            .collect()
    }

    fn on_pitchbend(&mut self, channel: u8, value: u16) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_pitchbend(channel, value));
                effects
            })
            .collect()
    }

    fn on_rpn(&mut self, channel: u8, param: u16, value: u16) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_rpn(channel, param, value));
                effects
            })
            .collect()
    }

    fn on_nrpn(&mut self, channel: u8, param: u16, value: u16) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_nrpn(channel, param, value));
                effects
            })
            .collect()
    }

    fn on_cat(&mut self, channel: u8, pressure: u8) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_cat(channel, pressure));
                effects
            })
            .collect()
    }

    fn on_pat(&mut self, channel: u8, note: Note, pressure: u8) -> Vec<MIDICallbackEffects> {
        self.find_all_parts(channel)
            .iter()
            .flat_map(|&i| {
                let part = self.parts[i].clone();
                let mut effects = vec![];
                part.write_with(|p| effects = p.on_pat(channel, note, pressure));
                effects
            })
            .collect()
    }
}

const NOTE_CENT_TABLE: NoteCentTable = {
    let mut notes = [0.0; 128];
    let mut note = 0;
    while note < 128 {
        notes[note] = (note as f32) * 100.0;
        note += 1;
    }

    let mut progs = [[0.0; 128]; 128];
    let mut prog = 0;
    while prog < 128 {
        progs[prog] = notes;
        prog += 1;
    }

    let mut data = [[[0.0; 128]; 128]; 128];
    let mut bank = 0;
    while bank < 128 {
        data[bank] = progs;
        bank += 1;
    }
    data
};

#[derive(Debug, PartialEq, Clone, Copy)]
pub enum MidiResetMode {
    GM,
    XG,
    GS,
    GM2,
}

#[inline(always)]
pub fn tuning_14bit_to_xg(u: u16) -> u16 {
    (u >> 3) & 0x0FFF
}
