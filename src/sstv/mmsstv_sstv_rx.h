/***************************************************************************
 *   mmsstv-linux-port: Step 7/8 -- mode-aware RX bridge to mmsstv-core    *
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
 *   own pipeline uses. See the project plan's "Step 7" (Martin 1, the    *
 *   original single-mode version of this file) and "Step 8" (generalized *
 *   to dispatch by detected mode) for the full rationale.                *
 *                                                                         *
 *   Generalized (not just extended) from Step 7's Martin-1-only version  *
 *   for a real correctness reason, not just to widen mode coverage:      *
 *   mmsstv-core's CSSTVDEM VIS scanner recognizes *any* MMSSTV mode's    *
 *   VIS code and mutates the single global `SSTVSET` singleton the       *
 *   moment it locks. Two independent CSSTVDEM instances fed the same     *
 *   raw audio would both attempt to lock and both mutate that same       *
 *   global state concurrently -- a real correctness bug, not just        *
 *   inelegant. This class owns exactly one CSSTVDEM (mirroring MMSSTV's  *
 *   own original architecture, which likewise has exactly one            *
 *   demodulator for the whole app) and dispatches to a per-mode pixel    *
 *   decoder only after VIS lock reveals which mode actually arrived.     *
 ***************************************************************************/
#ifndef MMSSTV_SSTV_RX_H
#define MMSSTV_SSTV_RX_H

#include "pixelconv.h"
#include "sstv.h"
#include "sstvparam.h"

// Only meaningful to feed samples to when at least one mode
// engineselection.h's mmsstvCoreSupports() covers is currently set to
// ENGINE_MMSSTV_CORE -- the caller (rxFunctions::run()) is responsible for
// that gating, not this class. Since RX doesn't know which mode is
// incoming until VIS locks, this class always scans for *any* mode's VIS
// code once fed, but only tracks/decodes a picture for a locked mode if
// that specific mode's selectedEngine() is ENGINE_MMSSTV_CORE at lock time
// -- otherwise it drains the demodulator (so its internal ring buffer
// doesn't stall) without producing any image, leaving QSSTV's own
// (correspondingly gated) per-mode RX path to handle it instead.
class MmsstvSstvRx
{
public:
	MmsstvSstvRx();
	~MmsstvSstvRx();

	// Feeds `count` raw (un-decimated, ~48kHz) audio samples through
	// mmsstv-core's demodulator. Internally detects VIS/sync for any
	// MMSSTV mode; for a mode this class has a decoder for (see
	// mmsstvCoreSupports() in engineselection.h) and that mode's engine
	// setting is ENGINE_MMSSTV_CORE, decodes and writes pixels into
	// QSSTV's own RX imageViewer, posting the same lineDisplayEvent/
	// startImageRXEvent/endImageSSTVRXEvent QSSTV's own pipeline uses for
	// cross-thread-safe GUI updates.
	void processSamples(const double *samples, int count);

private:
	// Heap-allocated and constructed *after* the constructor body sets
	// SampFreq/SampBase -- CSSTVDEM's constructor designs all its internal
	// filters (BPF, tone detectors, PLL) from the global SampFreq at the
	// moment it runs. A plain member object would be constructed as part
	// of MmsstvSstvRx's own construction, *before* the constructor body
	// gets a chance to set SampFreq -- silently building every filter for
	// mmsstv-core's default 11025 Hz while actually being fed 48kHz
	// samples, which never locks VIS/sync (found via live-test debugging
	// in Step 7, not by inspection).
	CSSTVDEM *dem;

	// One decoder instance per mmsstv-core-supported mode -- a small,
	// fixed set (see engineselection.h's mmsstvCoreSupports()), so plain
	// per-mode members plus a switch in the .cpp is simpler and clearer
	// than a polymorphic decoder hierarchy for classes with unrelated
	// pixel math (RGB-family vs. Robot 36's YUV-family).
	CMartinRxDecoder martinDecoder;
	CScottieRxDecoder scottieDecoder;
	CRobot36RxDecoder robot36Decoder;

	esstvMode trackingMode = NOTVALID; // which mode's picture is being decoded, if any
	bool trackingImage = false;
	int decodedRows = 0;
};

#endif // MMSSTV_SSTV_RX_H
