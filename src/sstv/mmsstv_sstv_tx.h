/***************************************************************************
 *   mmsstv-linux-port: Step 5/8 -- mode-aware TX bridge to mmsstv-core    *
 *                                                                         *
 *   Not part of upstream QSSTV. Bridges QSSTV's existing TX pipeline to  *
 *   mmsstv-core (a portable extraction of MMSSTV's SSTV DSP core, see    *
 *   https://github.com/n5ac/mmsstv), used in place of QSSTV's own        *
 *   per-mode TX path for whichever modes mmsstv-core implements (see     *
 *   engineselection.h's mmsstvCoreSupports()). See the project plan's    *
 *   "Step 5" (Martin 1, the original single-mode version of this file)   *
 *   and "Step 8" (generalized into a per-mode dispatch table) for the    *
 *   full rationale and integration-point research.                       *
 ***************************************************************************/
#ifndef MMSSTV_SSTV_TX_H
#define MMSSTV_SSTV_TX_H

#include "sstvparam.h"

class imageViewer;

// Sends `ivPtr`'s current image (already guaranteed the mode's own
// dimensions, pre-scaled by sstvTx::applyTemplate()/imageViewer::setParam()
// before TX starts) as an SSTV transmission in `mode`, using mmsstv-core's
// CSSTVMOD instead of QSSTV's own per-mode TX path. Pushes audio into the
// real TX pipeline via synthesizer::writeBuffer(), so PTT/audio routing
// (already set up by the caller, txFunctions::run()) just works. Caller
// must check engineselection.h's mmsstvCoreSupports(mode) first -- this
// function does not fall back to QSSTV's own path for unsupported modes.
// Returns true on completion (mirrors sstvTx::sendImage()'s bool contract;
// this bridge has no abort path yet, so it currently always returns true
// once it runs).
bool sendImageViaMmsstv(imageViewer *ivPtr, esstvMode mode);

#endif // MMSSTV_SSTV_TX_H
