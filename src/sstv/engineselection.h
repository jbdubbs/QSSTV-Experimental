/***************************************************************************
 *   mmsstv-linux-port: Step 6/8/9/17 -- per-mode engine selection         *
 *                                                                         *
 *   Not part of upstream QSSTV. QSSTV's own engine is kept permanently   *
 *   available for every mode; mmsstv-core (a portable extraction of      *
 *   MMSSTV's SSTV DSP core, see https://github.com/n5ac/mmsstv) is an    *
 *   alternative engine for the modes it supports (the RGB family --      *
 *   Martin 1/2, Scottie 1/2/DX -- Robot 36/72/24, and the full PD/ML/MR/ *
 *   MP families). This lets the user pick which engine handles a given   *
 *   mode, defaulting to mmsstv-core where it's available. See the        *
 *   project plan's "Step 6" for the full rationale.                      *
 ***************************************************************************/
#ifndef ENGINESELECTION_H
#define ENGINESELECTION_H

#include "sstvparam.h"

enum eEngine { ENGINE_MMSSTV_CORE, ENGINE_QSSTV };

// True for modes mmsstv-core has an implementation for at all (currently
// M1, S1, S2, SDX, R36) -- independent of what the user has *selected*,
// this is about whether there's a choice to make in the first place.
bool mmsstvCoreSupports(esstvMode mode);

// True if at least one mmsstvCoreSupports() mode is currently set to
// ENGINE_MMSSTV_CORE. RX doesn't know which mode is incoming until VIS
// locks, so the raw-audio-tap drain (rxfunctions.cpp) needs this rather
// than a single mode's selectedEngine() to decide whether to run
// mmsstv-core's demodulator at all.
bool mmsstvCoreActiveForAnyMode();

// The engine to actually use for `mode` right now: reads a persisted
// per-mode QSettings choice if one exists, otherwise defaults to
// ENGINE_MMSSTV_CORE when mmsstvCoreSupports(mode), else ENGINE_QSSTV
// (the only option for a mode mmsstv-core doesn't implement).
eEngine selectedEngine(esstvMode mode);

// Persists a per-mode engine choice. Only meaningful for modes where
// mmsstvCoreSupports(mode) is true; the caller should not offer a choice
// otherwise.
void setSelectedEngine(esstvMode mode, eEngine engine);

// Step 17: RX-only master override, independent of the per-mode
// selectedEngine() table above -- RX auto-detection can lock onto any of
// dozens of modes at any moment, unlike TX (always exactly one active
// mode), so a single global preference is used instead of bulk-writing
// every kMmsstvCoreModes() entry, and it deliberately never touches the
// per-mode table TX's own checkbox writes to. Defaults to true. See
// mmsstv_sstv_rx.cpp/syncprocessor.cpp for how it's consulted, and
// rxwidget.cpp for the checkbox that drives it.
bool rxPreferCoreEngine();
void setRxPreferCoreEngine(bool prefer);

#endif // ENGINESELECTION_H
