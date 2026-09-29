/***************************************************************************
 *   mmsstv-linux-port: JB60 chroma sampling-grid phase (experiment only)  *
 *   jb60-color-smear-ideas memory, idea 12 attempt 3.                    *
 ***************************************************************************/
#ifndef CHROMAGRIDPHASE_H
#define CHROMAGRIDPHASE_H

// Experimental only -- no QSettings-backed UI, no shipped default other than 0 (off, bit-identical to
// today's fixed grid). A constant sub-slot phase offset (in units of one Cr/Cb slot width) applied to
// BOTH the TX footprint grid (downsampleChroma) and the matching RX reconstruction grid (upsampleChroma)
// so the two ends stay consistent about which physical column each slot represents. See modejb60.cpp's
// downsampleChroma()/upsampleChroma() for where this is read, and the jb60-color-smear-ideas memory for
// why attempts 1/2 (pre-emphasis gain) were closed before trying this.
float chromaGridPhase();

// In-memory only, never persisted: sets the phase (slot widths, typically in [-0.5, 0.5]) for the
// running process. Used by the loopback test harness (--chroma-grid-phase). Default 0.0.
void setChromaGridPhase(float phase);

#endif // CHROMAGRIDPHASE_H
