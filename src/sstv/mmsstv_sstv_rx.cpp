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

namespace {

// Every mmsstv-core-supported mode's width is 320 (Robot 36's differs only
// in height); this is just the RGB row scratch buffer's fixed capacity,
// not a per-mode dimension assumption -- actual width/height always come
// from getModeDimensions().
constexpr int kMaxWidth = 320;

// Maps mmsstv-core's own internal mode constant (SSTVSET.m_Mode, from
// mmsstv-core/src/sstv.h) to QSSTV's esstvMode (sstv/sstvparam.h) -- the
// two are separate enums from separate codebases. Only covers modes this
// class has a decoder for; anything else returns NOTVALID, the same
// "not a mode we handle" sentinel syncProcessor::createModeBase() uses.
esstvMode mapMmsstvCoreMode(int coreMode)
{
	switch (coreMode) {
	case smMRT1: return M1;
	case smSCT1: return S1;
	case smSCT2: return S2;
	case smSCTDX: return SDX;
	case smR36: return R36;
	case smR72: return R72;
	case smR24: return R24;
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
					// Robot 24 transmits at half vertical resolution (120 real
					// lines) -- each decode call's row is duplicated into two
					// consecutive output rows, matching Main.cpp's gp/gp2
					// row-doubling exactly (see pixelconv.h's
					// CRobotChromaRxDecoder comment for how this was confirmed).
					int rowsThisCall = (trackingMode == R24) ? 2 : 1;
					switch (trackingMode) {
					case M1: martinDecoder.DecodeLine(ip, width, rgbRow); break;
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
						// Both share one decoder instance -- CRobotChromaRxDecoder
						// only ever reads generic SSTVSET.* fields, already correct
						// per-mode. See pixelconv.h/cpp.
						robotChromaDecoder.DecodeLine(ip, width, rgbRow);
						break;
					default: break; // can't happen: trackingImage implies a mapped mode
					}

					QRgb *pixels = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows);
					for (int x = 0; x < width; x++) {
						pixels[x] = qRgb(rgbRow[x * 3 + 0], rgbRow[x * 3 + 1], rgbRow[x * 3 + 2]);
					}
					QApplication::postEvent(dispatcherPtr, new lineDisplayEvent(decodedRows));
					if (rowsThisCall == 2 && decodedRows + 1 < height) {
						QRgb *pixels2 = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows + 1);
						for (int x = 0; x < width; x++) {
							pixels2[x] = qRgb(rgbRow[x * 3 + 0], rgbRow[x * 3 + 1], rgbRow[x * 3 + 2]);
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
