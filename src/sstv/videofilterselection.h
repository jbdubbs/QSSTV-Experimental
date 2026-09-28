/***************************************************************************
 *   mmsstv-linux-port: wide video filter for the fast modes               *
 *                                                                         *
 *   QSSTV's picture demodulator filter (about +/-600 Hz) turns a 190 us   *
 *   pixel into an edge about four pixels wide. The fast modes (PD120 and  *
 *   its variants, JB60) are the ones that pay for that, so for them the   *
 *   receiver can use a wider filter (about +/-1000 Hz): visibly sharper   *
 *   on a clean signal, noisier on a weak one. This is the on/off setting  *
 *   and the list of modes it applies to; see sstvRx for the plumbing.     *
 ***************************************************************************/
#ifndef VIDEOFILTERSELECTION_H
#define VIDEOFILTERSELECTION_H

#include "sstvparam.h"

// RX checkbox "Wide Video Filter" (persisted under RX/wideVideoFilter). Defaults to off: it is sharper on a clean
// signal (about 30-35 dB SNR and better) but visibly noisier below that, which is where most HF reception is.
bool wideVideoFilterEnabled();
void setWideVideoFilterEnabled(bool enabled);

// In-memory override of wideVideoFilterEnabled() for the running process only (never persisted):
// -1 = none (use the setting), 0 = off, 1 = on. Used by the command line (--wide-filter).
void setWideVideoFilterOverride(int override);

// Modes with a 190 us (or faster) pixel or slot time, the ones the wide filter is meant for.
bool videoFilterWideSuits(esstvMode mode);

// True when the receiver should use the wide filter for `mode` right now
// (videoFilterWideSuits(mode) && wideVideoFilterEnabled()).
bool modeUsesWideVideoFilter(esstvMode mode);

#endif // VIDEOFILTERSELECTION_H
