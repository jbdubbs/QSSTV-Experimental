/***************************************************************************
 *   mmsstv-linux-port: JB60 pseudo-luma edge cue (experiment only)        *
 *   jb60-color-smear-ideas memory, idea 16.                              *
 ***************************************************************************/
#ifndef CHROMAPSEUDOLUMA_H
#define CHROMAPSEUDOLUMA_H

// Experimental only -- no QSettings-backed UI, no shipped default other than 0.0 (off, bit-identical to
// today's L encoding). Unlike every other idea in this investigation, this one *deliberately* injects a
// small fake luma perturbation at a detected chroma-only edge (a real colour transition with no
// coincident luma change), betting that human vision's much higher luma acuity reads it as a sharper
// edge than the chroma channel alone could transmit ("pseudo-colour enhancement", a real analog-TV
// technique). It does not improve PSNR against the true source by design -- see modejb60.cpp's
// applyPseudoLumaCue() and the jb60-color-smear-ideas memory for why the usual PSNR bar doesn't apply.
float chromaPseudoLumaAmplitude();

// In-memory only, never persisted: sets the cue amplitude (luma counts, roughly 0..15 is sane) for the
// running process. Used by the loopback test harness (--chroma-pseudo-luma). Default 0.0 (off).
void setChromaPseudoLumaAmplitude(float amplitude);

#endif // CHROMAPSEUDOLUMA_H
