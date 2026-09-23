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

// Named gate for rxfunctions.cpp's raw-audio-tap drain: mirrors
// rxPreferCoreEngine(), since RX no longer consults the per-mode
// selectedEngine() table (see that function's comment below) -- every
// mmsstvCoreSupports() mode uses Core whenever the RX checkbox is on. RX
// doesn't know which mode is incoming until VIS locks, so this is a
// single up-front "is it worth running mmsstv-core's demodulator at all"
// check rather than a per-mode one.
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

// RX-only master switch ("MMSSTV Core As Default" checkbox), independent
// of the per-mode selectedEngine() table above -- RX auto-detection can
// lock onto any of dozens of modes at any moment, unlike TX (always
// exactly one active mode), and RX's own dispatch (syncprocessor.cpp,
// mmsstv_sstv_rx.cpp) deliberately never consults selectedEngine(), since
// that table is really "what did TX last pick for this mode" and mixing
// it into RX's decision let a TX-tab pick silently override the RX
// checkbox. So this is RX's sole source of truth: when true, every
// mmsstvCoreSupports() mode uses mmsstv-core and every other mode still
// uses QSSTV automatically; when false, QSSTV is used for everything.
// Defaults to true. See mmsstv_sstv_rx.cpp/syncprocessor.cpp for how it's
// consulted, and rxwidget.cpp for the checkbox that drives it.
bool rxPreferCoreEngine();
void setRxPreferCoreEngine(bool prefer);

#endif // ENGINESELECTION_H
