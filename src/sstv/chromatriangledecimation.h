/***************************************************************************
 *   QSSTV-Experimental: JB60 triangle-windowed chroma decimation           *
 *   jb60-color-smear-ideas memory, idea 13.                              *
 ***************************************************************************/
#ifndef CHROMATRIANGLEDECIMATION_H
#define CHROMATRIANGLEDECIMATION_H

// Experimental, off by default: replaces TX's disjoint box-average chroma decimation
// (modeJB60::downsampleChroma()) with an overlapping, triangle-windowed one
// (modeJB60::downsampleChromaTriangle()) -- a box window is a mediocre anti-aliasing prefilter (sinc
// frequency response, poor sidelobe suppression); a triangle (sinc^2) is the classic step up. Purely a
// TX-side change -- RX's upsampleChroma() reconstruction is unchanged and doesn't need to know which
// window TX used, only where each slot's centre sits.
bool chromaTriangleDecimationEnabled();
void setChromaTriangleDecimationEnabled(bool enabled);

// In-memory override of chromaTriangleDecimationEnabled() for the running process only (never
// persisted): -1 = none (use the setting), 0 = off, 1 = on. Used by the loopback test harness
// (--chroma-triangle).
void setChromaTriangleDecimationOverride(int override);

#endif // CHROMATRIANGLEDECIMATION_H
