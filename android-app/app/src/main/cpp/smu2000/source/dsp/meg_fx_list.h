// license:BSD-3-Clause
// S-MU2000: MEG と同じ作りで鳴らせるエフェクトの形の一覧（meg_fx.h が引く）。
// **tools/meg_fx が作ったもの。** 命令語の FNV-1a は MU2000 EX（firmware v2.01）で調べたもの。

#ifndef S_MU2000_DSP_MEG_FX_LIST_H
#define S_MU2000_DSP_MEG_FX_LIST_H

#include "meg_fx_chorus.h"
#include "meg_fx_chorus3.h"
#include "meg_fx_phaser_cho.h"
#include "meg_fx_ens_detune_cho.h"
#include "meg_fx_var_reverb.h"
#include "meg_fx_var_delay.h"
#include "meg_fx_var_er.h"
#include "meg_fx_var_chorus3.h"
#include "meg_fx_var_rotary.h"
#include "meg_fx_var_dt_rotary.h"
#include "meg_fx_var_phaser.h"
#include "meg_fx_var_dist.h"
#include "meg_fx_var_stereo_dist.h"
#include "meg_fx_var_eq.h"
#include "meg_fx_var_autowah.h"
#include "meg_fx_var_talkmod.h"
#include "meg_fx_var_dt_delay.h"
#include "meg_fx_var_wah_dt_delay.h"
#include "meg_fx_var_vdist.h"
#include "meg_fx_var_dual_rotary.h"
#include "meg_fx_var_isolator.h"
#include "meg_fx_var_autopan2.h"
#include "meg_fx_var_ampsim2.h"
#include "meg_fx_var_ringmod.h"
#include "meg_fx_var_lowreso.h"
#include "meg_fx_var_turntable.h"
#include "meg_fx_var_lofi.h"
#include "meg_fx_var_vflanger.h"
#include "meg_fx_var_multicomp.h"
#include "meg_fx_var_dynaflt.h"
#include "meg_fx_var_dynaflang.h"
#include "meg_fx_var_dynaphase.h"
#include "meg_fx_var_dynaring.h"
#include "meg_fx_var_slice.h"
#include "meg_fx_ins_reverb.h"
#include "meg_fx_ins_delay.h"
#include "meg_fx_ins_er.h"
#include "meg_fx_ins_chorus3.h"
#include "meg_fx_ins_rotary.h"
#include "meg_fx_ins_dt_rotary.h"
#include "meg_fx_ins_phaser.h"
#include "meg_fx_ins_dist.h"
#include "meg_fx_ins_stereo_dist.h"
#include "meg_fx_ins_eq.h"
#include "meg_fx_ins_autowah.h"
#include "meg_fx_ins_talkmod.h"
#include "meg_fx_ins_dt_delay.h"
#include "meg_fx_ins_wah_dt_delay.h"
#include "meg_fx_ins_vdist.h"
#include "meg_fx_ins_dual_rotary.h"
#include "meg_fx_ins_isolator.h"
#include "meg_fx_ins_autopan2.h"
#include "meg_fx_ins_ampsim2.h"
#include "meg_fx_ins_ringmod.h"
#include "meg_fx_ins_lowreso.h"
#include "meg_fx_ins_turntable.h"
#include "meg_fx_ins_lofi.h"
#include "meg_fx_ins_vflanger.h"
#include "meg_fx_ins_multicomp.h"
#include "meg_fx_ins_dynaflt.h"
#include "meg_fx_ins_dynaflang.h"
#include "meg_fx_ins_dynaphase.h"
#include "meg_fx_ins_dynaring.h"
#include "meg_fx_ins_slice.h"

namespace smu2000::dsp {

inline const meg_fx_entry MEG_FX_LIST[] = {
	{ 0xb5dbe418ddc66749ull, 0x98, &make_meg_fx<meg_fx_reverb> },   // リバーブ 18 種類
	{ 0x79d9173b02f47de5ull, 0x28, &make_meg_fx<meg_fx_chorus> },   // コーラス（CHORUS 1/2/4・GM CHORUS 1-4・FB CHORUS・CELESTE 1-4・FLANGER 1-3・GM FLANGER・SYMPHONIC）
	{ 0xe8efc619dd277059ull, 0x28, &make_meg_fx<meg_fx_chorus3> },   // コーラス（CHORUS 3）
	{ 0x9eb1e12d41c02ed8ull, 0x28, &make_meg_fx<meg_fx_phaser_cho> },   // コーラスの口の PHASER 1
	{ 0xfd8bd29ad0d6b4f9ull, 0x28, &make_meg_fx<meg_fx_ens_detune_cho> },   // コーラスの口の ENS DETUNE
	{ 0x71e24329693ce512ull, 0x60, &make_meg_fx<meg_fx_var_reverb> },   // バリエーション: HALL 1, HALL 2, HALL M, HALL L, ROOM 1, ROOM 2, ROOM 3, ROOM S, ROOM M, ROOM L, STAGE 1, STAGE 2, PLATE, GM PLATE, WHITE ROOM, TUNNEL, CANYON, BASEMENT
	{ 0x3b51e2a02697112full, 0x60, &make_meg_fx<meg_fx_var_delay> },   // バリエーション: DELAY LCR, DELAY L,R, ECHO, CROSSDELAY, T.DELAY, T.ECHO, T.CRS DLY, CHORUS 1, CHORUS 2, CHORUS 4, GM CHORUS1, GM CHORUS2, GM CHORUS3, GM CHORUS4, FB CHORUS, CELESTE 1, CELESTE 2, CELESTE 3, CELESTE 4, FLANGER 1, FLANGER 2, FLANGER 3, GM FLANGER, SYMPHONIC, T.FLANGER, THRU
	{ 0x1bcc8d37e7bac3b2ull, 0x60, &make_meg_fx<meg_fx_var_er> },   // バリエーション: ER 1, ER 2, GATE REV, REVRS GATE, KARAOKE 1, KARAOKE 2, KARAOKE 3
	{ 0x5f05b0b3339f1369ull, 0x60, &make_meg_fx<meg_fx_var_chorus3> },   // バリエーション: CHORUS 3
	{ 0x19ba2f9db25394a0ull, 0x60, &make_meg_fx<meg_fx_var_rotary> },   // バリエーション: ROTARY SP, TREMOLO, AUTO PAN, 2WAY ROTRY, AMBIENCE
	{ 0x4d949c5636e139d3ull, 0x60, &make_meg_fx<meg_fx_var_dt_rotary> },   // バリエーション: DT +RTRY, OD +RTRY, AMP+RTRY, DT +2RTRY, OD +2RTRY, AMP+2RTRY
	{ 0xcb5f7fe81b6573dfull, 0x60, &make_meg_fx<meg_fx_var_phaser> },   // バリエーション: PHASER 1, PHASER 2, T.PHASER
	{ 0x15b0f73ae435b259ull, 0x60, &make_meg_fx<meg_fx_var_dist> },   // バリエーション: DISTORTION, CMP+DT, OVERDRIVE, AMP SIM, HM ENHNCER, COMPRESSOR, NOISE GATE
	{ 0x2cbc3ce8c36e253dull, 0x60, &make_meg_fx<meg_fx_var_stereo_dist> },   // バリエーション: STREO DT, STREO OD, STREO AMP
	{ 0xa095324464112416ull, 0x60, &make_meg_fx<meg_fx_var_eq> },   // バリエーション: 3-BAND EQ, 2-BAND EQ, PITCH CNG1, PITCH CNG2, VOIC CANCL, ENS DETUNE
	{ 0x61762a931af209d8ull, 0x60, &make_meg_fx<meg_fx_var_autowah> },   // バリエーション: AUTO WAH, A-WAH+DT, A-WAH+OD, TOUCH WAH1, TOUCH WAH2, T-WAH+DIST, T-WAH+ODRV
	{ 0x41fa5b139b5febdcull, 0x60, &make_meg_fx<meg_fx_var_talkmod> },   // バリエーション: TALK MOD
	{ 0x1a74029d73325d86ull, 0x60, &make_meg_fx<meg_fx_var_dt_delay> },   // バリエーション: DT+DELAY, OD+DELAY, CMP+DT+DLY, CMP+OD+DLY
	{ 0xe55ed13bcadecd69ull, 0x60, &make_meg_fx<meg_fx_var_wah_dt_delay> },   // バリエーション: WAH+DT+DLY, WAH+OD+DLY
	{ 0x13adc1e540a6eb23ull, 0x60, &make_meg_fx<meg_fx_var_vdist> },   // バリエーション: V DT HARD, V DT H+DLY, V DT SOFT, V DT S+DLY
	{ 0xef8c174d06529e9bull, 0x60, &make_meg_fx<meg_fx_var_dual_rotary> },   // バリエーション: DUAL ROTR1, DUAL ROTR2
	{ 0x7d5fac042def35c3ull, 0x60, &make_meg_fx<meg_fx_var_isolator> },   // バリエーション: ISOLATOR
	{ 0xeb3037dd34e0745bull, 0x60, &make_meg_fx<meg_fx_var_autopan2> },   // バリエーション: AUTO PAN 2
	{ 0x4e14f05bf9826677ull, 0x60, &make_meg_fx<meg_fx_var_ampsim2> },   // バリエーション: AMP SIM 2
	{ 0x81ad3fcd5115e592ull, 0x60, &make_meg_fx<meg_fx_var_ringmod> },   // バリエーション: RING MOD
	{ 0xf7ec44d754349d6full, 0x60, &make_meg_fx<meg_fx_var_lowreso> },   // バリエーション: LOW RESO, D.SCRATCH
	{ 0x84ead76ebe8008f7ull, 0x60, &make_meg_fx<meg_fx_var_turntable> },   // バリエーション: D.TURNTBL
	{ 0x16e1d360ea4c134bull, 0x60, &make_meg_fx<meg_fx_var_lofi> },   // バリエーション: LO-FI
	{ 0x94a4a68bfe37e89cull, 0x60, &make_meg_fx<meg_fx_var_vflanger> },   // バリエーション: V-FLANGER
	{ 0x3775b4cd626f06c5ull, 0x60, &make_meg_fx<meg_fx_var_multicomp> },   // バリエーション: MULTI COMP
	{ 0x29f93dd87f99e786ull, 0x60, &make_meg_fx<meg_fx_var_dynaflt> },   // バリエーション: DYNA FLT
	{ 0x31af073f303fefe7ull, 0x60, &make_meg_fx<meg_fx_var_dynaflang> },   // バリエーション: DYNA FLANG
	{ 0xe10ef28528d28224ull, 0x60, &make_meg_fx<meg_fx_var_dynaphase> },   // バリエーション: DYNA PHASE
	{ 0x3209ea26c58f180dull, 0x60, &make_meg_fx<meg_fx_var_dynaring> },   // バリエーション: DYNA RING
	{ 0x7e38f500bc664ad8ull, 0x60, &make_meg_fx<meg_fx_var_slice> },   // バリエーション: SLICE
	{ 0xd64809406cae75feull, 0x60, &make_meg_fx<meg_fx_ins_reverb> },   // インサーション 1: HALL 1, HALL 2, HALL M, HALL L, ROOM 1, ROOM 2, ROOM 3, ROOM S, ROOM M, ROOM L, STAGE 1, STAGE 2, PLATE, GM PLATE, WHITE ROOM, TUNNEL, CANYON, BASEMENT
	{ 0xe4b959a333e1ebe5ull, 0x60, &make_meg_fx<meg_fx_ins_delay> },   // インサーション 1: DELAY LCR, DELAY L,R, ECHO, CROSSDELAY, T.DELAY, T.ECHO, T.CRS DLY, CHORUS 1, CHORUS 2, CHORUS 4, GM CHORUS1, GM CHORUS2, GM CHORUS3, GM CHORUS4, FB CHORUS, CELESTE 1, CELESTE 2, CELESTE 3, CELESTE 4, FLANGER 1, FLANGER 2, FLANGER 3, GM FLANGER, SYMPHONIC, T.FLANGER, THRU
	{ 0x0a06f49c00f544efull, 0x60, &make_meg_fx<meg_fx_ins_er> },   // インサーション 1: ER 1, ER 2, GATE REV, REVRS GATE, KARAOKE 1, KARAOKE 2, KARAOKE 3
	{ 0x7289a507398d908bull, 0x60, &make_meg_fx<meg_fx_ins_chorus3> },   // インサーション 1: CHORUS 3
	{ 0x9cbe229f96e6bd32ull, 0x60, &make_meg_fx<meg_fx_ins_rotary> },   // インサーション 1: ROTARY SP, TREMOLO, AUTO PAN, 2WAY ROTRY, AMBIENCE
	{ 0x0b5d651f086a531eull, 0x60, &make_meg_fx<meg_fx_ins_dt_rotary> },   // インサーション 1: DT +RTRY, OD +RTRY, AMP+RTRY, DT +2RTRY, OD +2RTRY, AMP+2RTRY
	{ 0x1bd6bbac60e5c1a6ull, 0x60, &make_meg_fx<meg_fx_ins_phaser> },   // インサーション 1: PHASER 1, PHASER 2, T.PHASER
	{ 0x6d554b6850e1a595ull, 0x60, &make_meg_fx<meg_fx_ins_dist> },   // インサーション 1: DISTORTION, CMP+DT, OVERDRIVE, AMP SIM, HM ENHNCER, COMPRESSOR, NOISE GATE
	{ 0xddce9dee5073fb51ull, 0x60, &make_meg_fx<meg_fx_ins_stereo_dist> },   // インサーション 1: STREO DT, STREO OD, STREO AMP
	{ 0x44bd7e743ddb172full, 0x60, &make_meg_fx<meg_fx_ins_eq> },   // インサーション 1: 3-BAND EQ, 2-BAND EQ, PITCH CNG1, PITCH CNG2, VOIC CANCL, ENS DETUNE
	{ 0x868a0b75c5ee8480ull, 0x60, &make_meg_fx<meg_fx_ins_autowah> },   // インサーション 1: AUTO WAH, A-WAH+DT, A-WAH+OD, TOUCH WAH1, TOUCH WAH2, T-WAH+DIST, T-WAH+ODRV
	{ 0x95911d0e027428baull, 0x60, &make_meg_fx<meg_fx_ins_talkmod> },   // インサーション 1: TALK MOD
	{ 0xebef96ab0c1c0785ull, 0x60, &make_meg_fx<meg_fx_ins_dt_delay> },   // インサーション 1: DT+DELAY, OD+DELAY, CMP+DT+DLY, CMP+OD+DLY
	{ 0x70251e1d70583dc9ull, 0x60, &make_meg_fx<meg_fx_ins_wah_dt_delay> },   // インサーション 1: WAH+DT+DLY, WAH+OD+DLY
	{ 0xd1265e5d76929c25ull, 0x60, &make_meg_fx<meg_fx_ins_vdist> },   // インサーション 1: V DT HARD, V DT H+DLY, V DT SOFT, V DT S+DLY
	{ 0x1ba57c779cb5d556ull, 0x60, &make_meg_fx<meg_fx_ins_dual_rotary> },   // インサーション 1: DUAL ROTR1, DUAL ROTR2
	{ 0x301ef55114e49b8eull, 0x60, &make_meg_fx<meg_fx_ins_isolator> },   // インサーション 1: ISOLATOR
	{ 0x4f009af540ebb533ull, 0x60, &make_meg_fx<meg_fx_ins_autopan2> },   // インサーション 1: AUTO PAN 2
	{ 0xf23253c3cebb1933ull, 0x60, &make_meg_fx<meg_fx_ins_ampsim2> },   // インサーション 1: AMP SIM 2
	{ 0x5eea2872f3990f0aull, 0x60, &make_meg_fx<meg_fx_ins_ringmod> },   // インサーション 1: RING MOD
	{ 0xae486244b211a6adull, 0x60, &make_meg_fx<meg_fx_ins_lowreso> },   // インサーション 1: LOW RESO, D.SCRATCH
	{ 0xfde196145f337c43ull, 0x60, &make_meg_fx<meg_fx_ins_turntable> },   // インサーション 1: D.TURNTBL
	{ 0xf894e80fd4de6f0aull, 0x60, &make_meg_fx<meg_fx_ins_lofi> },   // インサーション 1: LO-FI
	{ 0x965fac321d21d10dull, 0x60, &make_meg_fx<meg_fx_ins_vflanger> },   // インサーション 1: V-FLANGER
	{ 0xc88215488407d894ull, 0x60, &make_meg_fx<meg_fx_ins_multicomp> },   // インサーション 1: MULTI COMP
	{ 0x8b7a5ec9864f413full, 0x60, &make_meg_fx<meg_fx_ins_dynaflt> },   // インサーション 1: DYNA FLT
	{ 0xf50f2d18523ddc8full, 0x60, &make_meg_fx<meg_fx_ins_dynaflang> },   // インサーション 1: DYNA FLANG
	{ 0x756eb3f6d1ed13b8ull, 0x60, &make_meg_fx<meg_fx_ins_dynaphase> },   // インサーション 1: DYNA PHASE
	{ 0x6cdbad4831f07b0aull, 0x60, &make_meg_fx<meg_fx_ins_dynaring> },   // インサーション 1: DYNA RING
	{ 0x2ddaadc45515fc5eull, 0x60, &make_meg_fx<meg_fx_ins_slice> },   // インサーション 1: SLICE
};

} // namespace smu2000::dsp

#endif // S_MU2000_DSP_MEG_FX_LIST_H
