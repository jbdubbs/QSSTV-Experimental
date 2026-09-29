/***************************************************************************
 *   mmsstv-linux-port: RX chroma deconvolution for JB60                   *
 *   See videofilterselection.h for the sibling "Wide Video Filter" idea.  *
 ***************************************************************************/
#ifndef CHROMADECONVOLUTION_H
#define CHROMADECONVOLUTION_H

// RX checkbox "JB60 Chroma Deconvolution" (persisted under RX/chromaDeconvolution). Defaults to
// off: a regularized (Wiener-style) inverse of the combined TX-pre-emphasis + video-filter slot-
// domain response, applied to JB60's already-demodulated Cr/Cb to sharpen colour the channel
// smeared. Unlike TX idea 6 (which boosts before the channel adds noise), this amplifies whatever
// noise is already in the received signal along with genuine edges -- see modejb60.cpp for the
// noise-crossover SNR found by the sweep, and tests/jb60_loopback/README.md for the derivation.
bool chromaDeconvolutionEnabled();
void setChromaDeconvolutionEnabled(bool enabled);

// In-memory override of chromaDeconvolutionEnabled() for the running process only (never
// persisted): -1 = none (use the setting), 0 = off, 1 = on. Used by the loopback test harness
// (--chroma-deconv).
void setChromaDeconvolutionOverride(int override);

#endif // CHROMADECONVOLUTION_H
