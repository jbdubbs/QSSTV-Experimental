/***************************************************************************
 *   QSSTV-Experimental: TX edge-adaptive chroma pre-emphasis boost for JB60 *
 *   See chromapreemphasis' history (now unconditional, modejb60.cpp) for  *
 *   the base filter this cascades; jb60-color-smear-ideas memory, idea 12.*
 ***************************************************************************/
#ifndef CHROMAEDGEBOOST_H
#define CHROMAEDGEBOOST_H

// Experimental, off by default: at TX, blends a second cascaded pass of the same (already-
// unconditional) chroma pre-emphasis filter into slots that sit near a real Cr/Cb transition
// (detected from the slot sequence itself, which TX already has noise-free), tapered smoothly so
// no slot ever sees a sudden jump in how hard it's being filtered. See modejb60.cpp's
// applyChromaEdgeBoost() for the mechanism and tests/jb60_loopback/README.md for the sweep this
// idea needs before it could ever graduate past "opt-in experiment".
bool chromaEdgeBoostEnabled();
void setChromaEdgeBoostEnabled(bool enabled);

// In-memory override of chromaEdgeBoostEnabled() for the running process only (never persisted):
// -1 = none (use the setting), 0 = off, 1 = on. Used by the loopback test harness (--chroma-edge-boost).
void setChromaEdgeBoostOverride(int override);

#endif // CHROMAEDGEBOOST_H
