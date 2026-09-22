/***************************************************************************
 *   mmsstv-linux-port: Step 7/8/10/11 -- mode-aware RX bridge to        *
 *   mmsstv-core. See mmsstv_sstv_rx.h for what this is and why.          *
 ***************************************************************************/
#include "mmsstv_sstv_rx.h"

#include "appglobal.h"
#include "rxwidget.h"
#include "imageviewer.h"
#include "dispatcher.h"
#include "dispatchevents.h"
#include "engineselection.h"
#include "mmsstv_sstv_tx.h" // getModeDimensions() -- see its header for why RX shares TX's table

#include <QApplication>
#include <QSize>

#include <vector>

namespace {

// Every mmsstv-core-supported mode's width was 320 (Robot 36's differed
// only in height) until the PD family (Step 12), whose largest variant
// (PD290) is 800 wide -- this is just the RGB row scratch buffers' fixed
// capacity, not a per-mode dimension assumption -- actual width/height
// always come from getModeDimensions().
constexpr int kMaxWidth = 800;

// Maps mmsstv-core's own internal mode constant (SSTVSET.m_Mode, from
// mmsstv-core/src/sstv.h) to QSSTV's esstvMode (sstv/sstvparam.h) -- the
// two are separate enums from separate codebases. Only covers modes this
// class has a decoder for; anything else returns NOTVALID, the same
// "not a mode we handle" sentinel syncProcessor::createModeBase() uses.
esstvMode mapMmsstvCoreMode(int coreMode)
{
	switch (coreMode) {
	case smMRT1: return M1;
	case smMRT2: return M2;
	case smSCT1: return S1;
	case smSCT2: return S2;
	case smSCTDX: return SDX;
	case smR36: return R36;
	case smR72: return R72;
	case smR24: return R24;
	case smPD50: return PD50;
	case smPD90: return PD90;
	case smPD120: return PD120;
	case smPD160: return PD160;
	case smPD180: return PD180;
	case smPD240: return PD240;
	case smPD290: return PD290;
	case smML180: return ML180;
	case smML240: return ML240;
	case smML280: return ML280;
	case smML320: return ML320;
	case smMR73: return MR73;
	case smMR90: return MR90;
	case smMR115: return MR115;
	case smMR140: return MR140;
	case smMR175: return MR175;
	default: return NOTVALID;
	}
}

} // namespace

MmsstvSstvRx::MmsstvSstvRx()
{
	// Matches the TX bridge's choice (mmsstv_sstv_tx.cpp): run at QSSTV's
	// native 48kHz rather than its decimated 12kHz RX rate, since the raw
	// tap (soundBase::rawRxBuffer) is un-decimated. Must happen *before*
	// `new CSSTVDEM` below -- see the member comment in the header for why
	// this can't just be a plain member object.
	//
	// Step 13 finding, deliberately NOT worked around here: a genuinely
	// fresh CSSTVDEM (the very first one constructed in a process) has a
	// real, reproducible chance of failing to lock an extended-VIS signal
	// (the two-byte VIS mechanism the ML/MP families use) on its first
	// attempt, even though the same signal locks reliably once any other
	// mode has already been encoded/decoded in the process. One real,
	// confirmed contributing bug was found and fixed directly in
	// mmsstv-core (CSSTVDEM's constructor read m_SyncRestart, used by
	// CalcBPF() to pick the wideband filter's cutoff, before setting it --
	// see that fix's own comment in mmsstv-core/src/sstv.cpp). Beyond
	// that, extensive live and isolated testing (byte-level diffing of
	// CSSTVDEM/SSTVSET instances, construct/destruct bisection, chunked
	// vs. single-loop sample delivery) traced the *remaining* effect to
	// heap-memory-reuse timing: a throwaway CSSTVMOD+CSSTVDEM pair that
	// processes some real audio and is destructed before the real
	// demodulator is constructed reliably fixed it in every single-loop
	// test -- but the SAME fix, fed the same signal in realistic
	// RXSTRIPE-sized chunks (matching how this bridge actually receives
	// audio), failed just as often as no fix at all. That makes the
	// underlying bug genuinely non-deterministic / heap-layout-dependent
	// rather than a fixed condition: no warm-up strategy tested was ever
	// reliable enough to trust here, and an earlier attempt was live-
	// tested and found to introduce a worse problem (blocking real-time
	// sample delivery long enough to corrupt Robot 72 RX, previously
	// reliable since Step 11). A real fix needs the actual uninitialized
	// read found and eliminated (auditing every CSSTVDEM sub-object's
	// constructor -- CIIRTANK, CIIR, CPLL, CVCO, CSmooz, CFIR2, CSYNCINT,
	// etc.), not a workaround -- deferred; see the Step 13 plan notes.
	SampFreq = 48000.0;
	SampBase = 48000.0;
	sys.m_SampFreq = SampFreq;
	dem = new CSSTVDEM();
}

MmsstvSstvRx::~MmsstvSstvRx()
{
	delete dem;
}

void MmsstvSstvRx::processSamples(const double *samples, int count)
{
	for (int i = 0; i < count; i++) {
		bool wasSync = dem->m_Sync != 0;
		dem->Do(samples[i]);

		if (!wasSync && dem->m_Sync) {
			// Just locked sync -- CSSTVDEM's VIS scanner recognizes any
			// MMSSTV mode, not just the ones this class has a decoder for.
			decodedRows = 0;
			esstvMode lockedMode = mapMmsstvCoreMode(SSTVSET.m_Mode);
			trackingMode = lockedMode;
			trackingImage = (lockedMode != NOTVALID) && (selectedEngine(lockedMode) == ENGINE_MMSSTV_CORE);
			if (trackingImage) {
				int width, height;
				getModeDimensions(lockedMode, width, height); // trackingImage implies this succeeds
				bool done = false;
				startImageRXEvent *ce = new startImageRXEvent(QSize(width, height));
				ce->waitFor(&done);
				QApplication::postEvent(dispatcherPtr, ce);
				while (!done) {
					QApplication::processEvents();
				}
			}
		} else if (wasSync && !dem->m_Sync && trackingImage) {
			// Lost sync mid-picture.
			QApplication::postEvent(dispatcherPtr, new endImageSSTVRXEvent(trackingMode));
			trackingImage = false;
		}

		while (dem->m_rPage != dem->m_wPage) {
			if (trackingImage) {
				int width, height;
				getModeDimensions(trackingMode, width, height);
				if (decodedRows < height) {
					short *ip = &dem->m_Buf[dem->m_rPage * dem->m_BWidth];
					unsigned char rgbRow[kMaxWidth * 3];
					unsigned char rgbRowOdd[kMaxWidth * 3];
					// Robot 24 transmits at half vertical resolution (120 real
					// lines) -- each decode call's row is duplicated into two
					// consecutive output rows, matching Main.cpp's gp/gp2
					// row-doubling exactly (see pixelconv.h's
					// CRobotChromaRxDecoder comment for how this was confirmed).
					// The PD family (Step 12) also produces two output rows per
					// call, but for a genuinely different reason -- both rows
					// carry distinct luma sharing one chroma pair -- so
					// CPDRxDecoder::DecodeLine fills rgbRow/rgbRowOdd directly
					// with two different rows, rather than one row duplicated.
					bool isPD = (trackingMode >= PD50) && (trackingMode <= PD290);
					int rowsThisCall = (trackingMode == R24 || isPD) ? 2 : 1;
					switch (trackingMode) {
					case M1:
				case M2:
					// Both share one decoder instance -- confirmed in
					// Main.cpp both smMRT1/smMRT2 fall into the same
					// generic RGB decode branch. See pixelconv.h/cpp.
					martinDecoder.DecodeLine(ip, width, rgbRow);
					break;
					case S1:
					case S2:
					case SDX:
						// All three Scottie variants share one decoder instance --
						// CScottieRxDecoder is mode-aware internally (SSTVSET.m_Mode
						// picks GetPixelLevel vs GetPictureLevel for SDX), not
						// per-instance. See pixelconv.h/cpp.
						scottieDecoder.DecodeLine(ip, width, rgbRow);
						break;
					case R36:
						robot36Decoder.DecodeLine(ip, width, rgbRow);
						break;
					case R72:
					case R24:
					case ML180:
					case ML240:
					case ML280:
					case ML320:
					case MR73:
					case MR90:
					case MR115:
					case MR140:
					case MR175:
						// All share one decoder instance -- CRobotChromaRxDecoder
						// only ever reads generic SSTVSET.* fields, already correct
						// per-mode. ML/MR share Robot 72/24's exact RX segment case
						// block in Main.cpp (confirmed directly, not assumed from
						// the "ML/MR family" framing -- see pixelconv.h's
						// EncodeMLLine comment for the one place ML/MR's wire format
						// actually differs, which is TX-only). See pixelconv.h/cpp.
						robotChromaDecoder.DecodeLine(ip, width, rgbRow);
						break;
					default:
						if (isPD) {
							pdDecoder.DecodeLine(ip, width, rgbRow, rgbRowOdd);
						}
						break; // otherwise can't happen: trackingImage implies a mapped mode
					}

					QRgb *pixels = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows);
					for (int x = 0; x < width; x++) {
						pixels[x] = qRgb(rgbRow[x * 3 + 0], rgbRow[x * 3 + 1], rgbRow[x * 3 + 2]);
					}
					QApplication::postEvent(dispatcherPtr, new lineDisplayEvent(decodedRows));
					if (rowsThisCall == 2 && decodedRows + 1 < height) {
						const unsigned char *secondRow = isPD ? rgbRowOdd : rgbRow;
						QRgb *pixels2 = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows + 1);
						for (int x = 0; x < width; x++) {
							pixels2[x] = qRgb(secondRow[x * 3 + 0], secondRow[x * 3 + 1], secondRow[x * 3 + 2]);
						}
						QApplication::postEvent(dispatcherPtr, new lineDisplayEvent(decodedRows + 1));
					}
					decodedRows += rowsThisCall;

					if (decodedRows >= height) {
						QApplication::postEvent(dispatcherPtr, new endImageSSTVRXEvent(trackingMode));
						trackingImage = false;
					}
				}
			}
			dem->m_rPage++;
			if (dem->m_rPage >= SSTVDEMBUFMAX) dem->m_rPage = 0;
		}
	}
}
