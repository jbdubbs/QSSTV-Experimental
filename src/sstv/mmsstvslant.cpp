/***************************************************************************
 *   mmsstv-linux-port: real-time slant / clock-drift correction for the  *
 *   MMSSTV Core RX engine. See mmsstvslant.h for what this is and why.   *
 ***************************************************************************/
#include "mmsstvslant.h"

#include "sstvparam.h" // autoSlantAdjust

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
int slantOverride = -1;
}

void setMmsstvSlantOverride(int override)
{
	slantOverride = override;
}

bool mmsstvSlantActive()
{
	if (slantOverride >= 0) return slantOverride == 1;
	return autoSlantAdjust;
}

void MmsstvSlantTracker::reset()
{
	historyCount = 0;
	nextSlot = 0;
	lineNumber = 0;
	linesSinceRecalc = 0;
	lastSamplesPerLine = 0;
	ratio = 1.0;
}

void MmsstvSlantTracker::observeLine(const short *bufRow, int samplesPerLine)
{
	int line = lineNumber++;
	if (!mmsstvSlantActive() || !bufRow || samplesPerLine <= 0) return;
	lastSamplesPerLine = samplesPerLine;

	// Search a window straddling sample offset 0 (where IncWP() currently
	// believes this line's sync/picture boundary should be) for the
	// sharpest edge. +/-5% of the nominal line width is generous for any
	// realistic soundcard clock mismatch (real drift is usually well
	// under 1%) while staying clear of the rest of the line's own
	// picture content.
	int window = samplesPerLine / 20;
	if (window < 8) window = 8;
	if (window * 2 >= samplesPerLine) window = samplesPerLine / 2 - 1;
	if (window < 1) return;

	// Every mmsstv-core-supported mode transitions once per line between
	// its sync/porch tone and real picture data -- a hard frequency
	// discontinuity, so it shows up as a sharp edge in the demodulated
	// value itself (bufRow), not as a sustained plateau. Score each
	// candidate offset by a span-20 finite difference (a small local
	// baseline, not a bare single-sample difference, so isolated-sample
	// noise doesn't win) and take whichever side is steeper, since
	// different mode families place sync before or after their picture
	// data. This replaced an earlier design that searched dem->m_B12 (the
	// tone-detector "sync energy" channel) for its peak: found by dumping
	// raw sample values during development that m_B12's own version of
	// this transition is a wide, smoothly-filtered ~25ms hump whose flat-
	// topped maximum is poorly conditioned for sample-accurate timing --
	// it chased noise even on a zero-drift recording. The same transition
	// in bufRow is far sharper (order of tens of samples).
	int span = 20;
	if (span * 2 >= window) span = window / 2;
	if (span < 1) span = 1;

	auto edgeScore = [&](int center) -> long
		{
			auto at = [&](int idx) -> short
				{
					if (idx < 0) idx += samplesPerLine;
					else if (idx >= samplesPerLine) idx -= samplesPerLine;
					return bufRow[idx];
				};
			return std::labs((long)at(center + span) - (long)at(center - span));
		};

	int bestOffset = 0;
	long bestScore = edgeScore(0);
	// Positive side: the boundary arrived later than expected, at the
	// start of this row's own samples.
	for (int i = 1; i < window; i++)
		{
			long score = edgeScore(i);
			if (score > bestScore) { bestScore = score; bestOffset = i; }
		}
	// Negative side: the boundary arrived earlier than expected, still
	// inside the tail of this same row's sample slot (IncWP() already
	// counted those samples into this row before wrapping).
	for (int i = 1; i < window; i++)
		{
			int idx = samplesPerLine - i;
			long score = edgeScore(idx);
			if (score > bestScore) { bestScore = score; bestOffset = -i; }
		}

	if (getenv("MMSSTV_SLANT_DEBUG")) fprintf(stderr, "[slant] line=%d spl=%d bestOffset=%d bestScore=%ld ratio=%.6f\n", line, samplesPerLine, bestOffset, bestScore, ratio);

	Observation obs{(double)line, (double)bestOffset};
	if (historyCount < kHistory)
		{
			history[historyCount++] = obs;
		}
	else
		{
			history[nextSlot] = obs;
			nextSlot = (nextSlot + 1) % kHistory;
		}

	if (++linesSinceRecalc >= kRecalcInterval && historyCount >= kWarmupLines)
		{
			linesSinceRecalc = 0;
			recalc();
		}
}

void MmsstvSlantTracker::recalc()
{
	// Fit offset = a + slope*relativeLine over the current history window.
	// Offsets already reflect whatever ratio is currently applied, so a
	// converged/correct ratio should show slope ~= 0; a nonzero slope is
	// the *residual* per-line drift the current ratio isn't accounting
	// for yet -- same regression shape as QSSTV's own
	// syncProcessor::regression() (syncprocessor.cpp), adapted to a
	// running correction that's refined incrementally instead of
	// recomputed from an untouched raw signal each time.
	int n = historyCount;
	double baseLine = history[0].line; // relative x-axis, keeps numbers small
	double sumX = 0, sumY = 0, sumYY = 0, sumXY = 0;
	for (int i = 0; i < n; i++)
		{
			double y = history[i].line - baseLine;
			double x = history[i].offset;
			sumX += x; sumY += y; sumYY += y * y; sumXY += x * y;
		}
	double denom = n * sumYY - sumY * sumY;
	if (std::fabs(denom) < 1e-9) return;
	double slope = (n * sumXY - sumX * sumY) / denom; // samples of residual drift per line

	// Sanity guard mirroring syncprocessor.cpp's own bar against a false
	// lock: a real soundcard clock mismatch is never anywhere near this
	// large, so a fit this steep means the edge search is tracking
	// picture content, not the real sync/picture boundary -- ignore it
	// rather than risk a runaway correction.
	if (std::fabs(slope) > lastSamplesPerLine * 0.01)
		{
			if (getenv("MMSSTV_SLANT_DEBUG")) fprintf(stderr, "[slant] recalc REJECTED slope=%.4f (limit %.4f)\n", slope, lastSamplesPerLine * 0.01);
			return;
		}

	// A positive/growing offset means CSSTVDEM's own fixed-count line
	// timer is completing each row *faster* than the real signal's line
	// rate (it "gets there early", so the next real edge shows up later
	// and later within the row it already started) -- correcting that
	// means slowing dem down, i.e. *raising* ratio (see
	// MmsstvSstvRx::processSamples(): ratio>1 means fewer resampled
	// outputs are emitted per raw input sample, so dem's own fixed-count
	// line timer takes longer, in real-input-sample terms, to fill).
	// Empirically confirmed with MMSSTV_SLANT_DEBUG=1 against a zero-drift
	// recording: the opposite sign let the residual grow without bound.
	//
	// kDamping (<1) applies only a fraction of each recalc's fitted slope:
	// real clock drift is *persistent*, so it keeps reappearing in every
	// successive window's fit and the correction still converges to it
	// over several recalcs; a noise-driven false trend from one window
	// mostly cancels against the next window's opposite-signed noise
	// instead of being fully, permanently baked into ratio. Found
	// empirically (MMSSTV_SLANT_DEBUG=1 against a zero-drift recording)
	// that full-strength updates (kDamping=1) let a transient false trend
	// swing ratio by >1% over ~20 recalcs even though no real drift exists.
	constexpr double kDamping = 0.25;
	ratio *= (1.0 + kDamping * slope / lastSamplesPerLine);
	if (ratio < 0.95) ratio = 0.95;
	if (ratio > 1.05) ratio = 1.05;
	if (getenv("MMSSTV_SLANT_DEBUG")) fprintf(stderr, "[slant] recalc slope=%.4f new ratio=%.6f\n", slope, ratio);
}
