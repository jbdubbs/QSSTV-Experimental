/***************************************************************************
 *   mmsstv-linux-port: wide video filter for the fast modes               *
 *                                                                         *
 *   QSSTV's picture demodulator filter (about +/-600 Hz) turns a 190 us   *
 *   pixel into an edge about four pixels wide. The fast modes (PD120 and  *
 *   its variants, JB60) are the ones that pay for that, so the receiver  *
 *   also runs a wider filter (about +/-1000 Hz) alongside it: visibly    *
 *   sharper on a clean signal, noisier on a weak one. One RX checkbox     *
 *   controls both, but the two mode families use it differently:          *
 *   - PD120 and its variants (1 px/slot everywhere) apply it to the       *
 *     whole picture, chosen once per picture (videoFilterWideSuits(),     *
 *     modeUsesWideVideoFilter(), read by sstvRx::modeDemodPtr()).         *
 *   - JB60 (which subsamples chroma far more than luma) applies it to     *
 *     Cr/Cb only, chosen per slot: modeJB60::getPixels() reads            *
 *     wideVideoFilterEnabled() directly and reads sampleWide (see         *
 *     modeBase::setWideDemod()) instead of sample for chroma slots only,  *
 *     so L/D always stay on the narrow, noise-robust filter regardless    *
 *     of the checkbox. That's why JB60 isn't in videoFilterWideSuits().   *
 ***************************************************************************/
#ifndef VIDEOFILTERSELECTION_H
#define VIDEOFILTERSELECTION_H

#include "sstvparam.h"

// RX checkbox "Wide Video Filter" (persisted under RX/wideVideoFilter). Defaults to off: it is sharper on a clean
// signal (about 30-35 dB SNR and better) but visibly noisier below that, which is where most HF reception is.
// (JB60 only applies this to chroma, so its own noise crossover is expected to sit lower -- see modejb60.cpp.)
bool wideVideoFilterEnabled();
void setWideVideoFilterEnabled(bool enabled);

// In-memory override of wideVideoFilterEnabled() for the running process only (never persisted):
// -1 = none (use the setting), 0 = off, 1 = on. Used by the command line (--wide-filter).
void setWideVideoFilterOverride(int override);

// Modes that apply the wide filter to the whole picture at once (PD120 and its variants). JB60 is deliberately
// not here: it applies the same checkbox to Cr/Cb only, read directly via wideVideoFilterEnabled() in
// modejb60.cpp, so its L/D never move off the narrow filter.
bool videoFilterWideSuits(esstvMode mode);

// True when the receiver should use the wide filter for the whole of `mode`'s picture right now
// (videoFilterWideSuits(mode) && wideVideoFilterEnabled()). Not what JB60 uses -- see above.
bool modeUsesWideVideoFilter(esstvMode mode);

#endif // VIDEOFILTERSELECTION_H
