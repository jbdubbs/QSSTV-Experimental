/***************************************************************************
 *   mmsstv-linux-port: real-time slant / clock-drift correction for the  *
 *   MMSSTV Core RX engine (mmsstv_sstv_rx.cpp).                          *
 *                                                                         *
 *   QSSTV's own engine already has this (syncProcessor::slantAdjust()/    *
 *   regression(), syncprocessor.cpp) -- mmsstv-core's CSSTVDEM has no     *
 *   equivalent, live or dead (confirmed by direct audit: its line-wrap,  *
 *   CSSTVDEM::IncWP(), is a blind fixed-sample counter with no awareness *
 *   of where a sync pulse actually lands), and the original MMSSTV app's *
 *   own "Automatic slant adjustment" algorithm isn't recoverable from    *
 *   the available source dump either -- so this is a fresh              *
 *   implementation, not a port, built to have the same *effect* as       *
 *   QSSTV's own checkbox, driven by the same shared "Auto Slant"         *
 *   setting (autoSlantAdjust, sstvparam.h).                              *
 *                                                                         *
 *   Detection is mode-agnostic and needs no mmsstv-core changes: every   *
 *   mmsstv-core-supported mode transitions from its sync/porch tone into *
 *   real picture data once per line, a hard frequency discontinuity that *
 *   shows up as a sharp edge in CSSTVDEM's own demodulated output        *
 *   (dem->m_Buf, already read by the bridge for pixel decode) -- checked *
 *   by dumping raw sample values around a line boundary during           *
 *   development: this edge is far sharper and more precisely located     *
 *   than the same transition's footprint in dem->m_B12 (the tone-        *
 *   detector "sync energy" channel tried first, and rejected: m_B12's    *
 *   own version of this step is a wide, smoothly-filtered ~25ms hump     *
 *   whose flat-topped maximum is poorly conditioned for sample-accurate  *
 *   timing and chased noise even on a zero-drift recording). If          *
 *   CSSTVDEM's own fixed-rate line counter (IncWP()) doesn't exactly     *
 *   match the transmitter's real line rate, this edge slowly drifts      *
 *   away from the sample offset IncWP() currently believes is the start  *
 *   of the line -- that drift *is* slant. So rather than learning each   *
 *   mode's own sync-gate timing constants, this looks for the steepest   *
 *   edge (either direction, so it doesn't need to know whether a given   *
 *   mode's sync sits before or after its picture data within the line)   *
 *   in a window straddling the boundary IncWP() already believes is      *
 *   correct, which works the same way for every supported mode.          *
 *                                                                         *
 *   Correction is applied going forward only (mirrors QSSTV's own real-  *
 *   time checkbox, and matches original MMSSTV's documented distinction  *
 *   between real-time "Automatic" and offline "High-accuracy" slant      *
 *   adjustment) by resampling the raw audio fed to CSSTVDEM::Do() (see   *
 *   MmsstvSstvRx::processSamples()) rather than by varying CSSTVDEM's    *
 *   own line width mid-picture, which would break pixelconv.cpp's and    *
 *   the bridge's fixed-row-stride assumptions.                          *
 ***************************************************************************/
#ifndef MMSSTVSLANT_H
#define MMSSTVSLANT_H

class MmsstvSlantTracker
{
public:
	// Call once per new picture (VIS lock) and on abort -- forgets
	// everything learned so far, same as QSSTV's own per-picture reset
	// (syncProcessor::init() resets slantAdjustLine/modifiedClock).
	void reset();

	// Call once per raw line CSSTVDEM has finished writing (i.e. once per
	// dem->m_rPage advance in the bridge's decode loop) with that line's
	// raw demodulated-sample slice (dem->m_Buf at the row's ring-buffer
	// offset -- the same data the pixel decoders read) and the
	// samples-per-line CSSTVDEM is currently using for this mode
	// (SSTVSET.m_WD). Cheap: one bounded linear scan, not per-sample work
	// inside CSSTVDEM's own hot path.
	void observeLine(const short *bufRow, int samplesPerLine);

	// Multiply the raw audio sample rate fed to dem->Do() by this before
	// demodulation. 1.0 = no correction (always true before enough lines
	// have been observed, or when disabled).
	double correctionRatio() const { return ratio; }

private:
	struct Observation { double line; double offset; };
	static constexpr int kHistory = 64;       // recent lines kept for the regression window
	static constexpr int kWarmupLines = 16;   // MMSSTV's own documented minimum for its high-accuracy mode
	static constexpr int kRecalcInterval = 5; // matches QSSTV's own slantAdjustLine cadence

	Observation history[kHistory];
	int historyCount = 0;
	int nextSlot = 0;
	int lineNumber = 0;
	int linesSinceRecalc = 0;
	int lastSamplesPerLine = 0;
	double ratio = 1.0;

	void recalc();
};

// In-memory override (never persisted, process-local) of whether the
// tracker above runs at all, for the command line's --slant option
// (mirrors videofilterselection.h's setWideVideoFilterOverride() pattern).
// -1 = none (use the shared autoSlantAdjust setting), 0 = off, 1 = on.
void setMmsstvSlantOverride(int override);
bool mmsstvSlantActive();

#endif // MMSSTVSLANT_H
