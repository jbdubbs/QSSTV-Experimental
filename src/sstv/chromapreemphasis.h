/***************************************************************************
 *   mmsstv-linux-port: JB60 TX chroma pre-emphasis                        *
 *                                                                         *
 *   QSSTV's picture demodulator filter (about +/-600 Hz) turns a 190 us   *
 *   pixel into an edge about four pixels wide (see videofilterselection.h *
 *   for the RX-side wide-filter attempt at the same problem). This is a   *
 *   TX-side attempt instead: a small opt-in checkbox boosts JB60's Cr/Cb  *
 *   slot sequence before modulation, matched to the measured video        *
 *   filter response, so old and new receivers alike get a slightly        *
 *   sharper picture without any RX-side change. Off by default -- see     *
 *   the jb60-color-smear-ideas memory, TX idea 6, for the kernel          *
 *   derivation and the numbers behind the default.                        *
 ***************************************************************************/
#ifndef CHROMAPREEMPHASIS_H
#define CHROMAPREEMPHASIS_H

// TX checkbox "JB60 Chroma Pre-emphasis" (persisted under TX/jb60ChromaPreEmphasis). Defaults to off.
bool chromaPreEmphasisEnabled();
void setChromaPreEmphasisEnabled(bool enabled);

// In-memory override of chromaPreEmphasisEnabled() for the running process only (never persisted):
// -1 = none (use the setting), 0 = off, 1 = on. Used by the loopback test harness (--chroma-preemph).
void setChromaPreEmphasisOverride(int override);

#endif // CHROMAPREEMPHASIS_H
