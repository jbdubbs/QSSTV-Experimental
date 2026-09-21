/***************************************************************************
 *   mmsstv-linux-port: Step 7 -- Martin 1 RX bridge to mmsstv-core        *
 *                                                                         *
 *   Not part of upstream QSSTV. Unlike TX (a simple dispatch swap),      *
 *   RX needs its own independent demodulation path: QSSTV's own RX       *
 *   pipeline (sstvRx::run(), fed from soundBase::rxBuffer) already runs  *
 *   its own FM demod on decimated (12kHz) audio before any per-mode      *
 *   dispatch happens, but mmsstv-core's CSSTVDEM needs genuinely raw,    *
 *   un-decimated audio and does its own complete demodulation from       *
 *   scratch. This class is fed from a separate raw-audio tap             *
 *   (soundBase::rawRxBuffer) and drives its own VIS/sync detection and   *
 *   pixel decode, writing into the same imageViewer/dispatcher QSSTV's   *
 *   own pipeline uses. See the project plan's "Step 7" for the full      *
 *   rationale and integration-point research.                            *
 ***************************************************************************/
#ifndef MMSSTV_MARTIN_RX_H
#define MMSSTV_MARTIN_RX_H

#include "pixelconv.h"
#include "sstv.h"

// Only meaningful to feed samples to when engineselection.h's
// selectedEngine(M1)==ENGINE_MMSSTV_CORE -- the caller (rxFunctions::run())
// is responsible for that gating, not this class.
class MmsstvMartinRx
{
public:
	MmsstvMartinRx();
	~MmsstvMartinRx();

	// Feeds `count` raw (un-decimated, ~48kHz) audio samples through
	// mmsstv-core's demodulator. Internally detects VIS/sync, and for a
	// Martin 1 picture specifically, decodes and writes pixels into
	// QSSTV's own RX imageViewer, posting the same lineDisplayEvent/
	// startImageRXEvent/endImageSSTVRXEvent QSSTV's own pipeline uses for
	// cross-thread-safe GUI updates. Silently ignores (but still drains)
	// any other mode's VIS lock -- this engine only implements Martin 1.
	void processSamples(const double *samples, int count);

private:
	// Heap-allocated and constructed *after* the constructor body sets
	// SampFreq/SampBase -- CSSTVDEM's constructor designs all its internal
	// filters (BPF, tone detectors, PLL) from the global SampFreq at the
	// moment it runs. A plain member object would be constructed as part
	// of MmsstvMartinRx's own construction, *before* the constructor body
	// gets a chance to set SampFreq -- silently building every filter for
	// mmsstv-core's default 11025 Hz while actually being fed 48kHz
	// samples, which never locks VIS/sync (found via live-test debugging,
	// not by inspection -- see the project plan's "Step 7" for how this
	// surfaced).
	CSSTVDEM *dem;
	CMartinRxDecoder rxDecoder;
	bool trackingImage = false;
	int decodedRows = 0;
};

#endif // MMSSTV_MARTIN_RX_H
