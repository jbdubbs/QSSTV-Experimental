/***************************************************************************
 *   mmsstv-linux-port: Step 5 -- Martin 1 TX bridge to mmsstv-core        *
 *                                                                         *
 *   Not part of upstream QSSTV. Bridges QSSTV's existing TX pipeline to  *
 *   mmsstv-core (a portable extraction of MMSSTV's SSTV DSP core, see    *
 *   https://github.com/n5ac/mmsstv), used in place of QSSTV's own        *
 *   modeGBR TX path for Martin 1 only. See the project plan's "Step 5"   *
 *   for the full rationale and integration-point research.               *
 ***************************************************************************/
#ifndef MMSSTV_MARTIN_TX_H
#define MMSSTV_MARTIN_TX_H

class imageViewer;

// Sends `ivPtr`'s current image (already guaranteed 320x256, pre-scaled by
// sstvTx::applyTemplate()/imageViewer::setParam() before TX starts) as a
// Martin 1 SSTV transmission, using mmsstv-core's CSSTVMOD/EncodeMartinLine
// instead of QSSTV's own modeGBR TX path. Pushes audio into the real TX
// pipeline via synthesizer::writeBuffer(), so PTT/audio routing (already
// set up by the caller, txFunctions::run()) just works. Returns true on
// completion (mirrors sstvTx::sendImage()'s bool contract; this bridge has
// no abort path yet, so it currently always returns true once it runs).
bool sendMartin1ImageViaMmsstv(imageViewer *ivPtr);

#endif // MMSSTV_MARTIN_TX_H
