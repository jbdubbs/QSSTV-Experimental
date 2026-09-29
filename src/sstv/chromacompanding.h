/***************************************************************************
 *   mmsstv-linux-port: JB60 chroma magnitude companding (experiment only) *
 *   jb60-color-smear-ideas memory, idea 14.                              *
 ***************************************************************************/
#ifndef CHROMACOMPANDING_H
#define CHROMACOMPANDING_H

// Experimental only -- no QSettings-backed UI yet, no shipped default other than 1.0 (identity, bit-
// identical to today's linear Cr/Cb encoding). A shared TX/RX wire-format constant (both ends must agree,
// like idea 6's filter taps): TX compands the joint (Cr,Cb) *magnitude* by this exponent before
// downsampleChroma() decimates it (modeJB60::getLine()), RX decompands by its reciprocal after
// upsampleChroma() reconstructs the full-width row (modeJB60::emitPair()), both at modejb60.cpp's
// companChromaMagnitude()/decompandChromaMagnitude(). Hue (the (Cr,Cb) angle) is preserved exactly by
// construction -- both channels are scaled by the same factor, never rotated.
float chromaCompandGamma();

// In-memory only, never persisted: sets gamma for the running process. Used by the loopback test
// harness (--chroma-compand-gamma). Default 1.0 (identity).
void setChromaCompandGamma(float gamma);

#endif // CHROMACOMPANDING_H
