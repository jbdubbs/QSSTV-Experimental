/***************************************************************************
 *   mmsstv-linux-port: Step 7/8 -- mode-aware RX bridge to mmsstv-core    *
 *   See mmsstv_sstv_rx.h for what this is and why.                       *
 ***************************************************************************/
#include "mmsstv_sstv_rx.h"

#include "appglobal.h"
#include "rxwidget.h"
#include "imageviewer.h"
#include "dispatcher.h"
#include "dispatchevents.h"
#include "engineselection.h"

#include <QApplication>
#include <QSize>

namespace {

// Both mmsstv-core-supported modes today are 320x256; this'll need to
// become per-mode data once a differently-sized mode is added.
constexpr int kWidth = 320;
constexpr int kHeight = 256;

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
				bool done = false;
				startImageRXEvent *ce = new startImageRXEvent(QSize(kWidth, kHeight));
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
			if (trackingImage && decodedRows < kHeight) {
				short *ip = &dem->m_Buf[dem->m_rPage * dem->m_BWidth];
				unsigned char rgbRow[kWidth * 3];
				switch (trackingMode) {
				case M1: martinDecoder.DecodeLine(ip, kWidth, rgbRow); break;
				case S1:
				case S2:
				case SDX:
					// All three Scottie variants share one decoder instance --
					// CScottieRxDecoder is mode-aware internally (SSTVSET.m_Mode
					// picks GetPixelLevel vs GetPictureLevel for SDX), not
					// per-instance. See pixelconv.h/cpp.
					scottieDecoder.DecodeLine(ip, kWidth, rgbRow);
					break;
				default: break; // can't happen: trackingImage implies a mapped mode
				}

				QRgb *pixels = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows);
				for (int x = 0; x < kWidth; x++) {
					pixels[x] = qRgb(rgbRow[x * 3 + 0], rgbRow[x * 3 + 1], rgbRow[x * 3 + 2]);
				}
				QApplication::postEvent(dispatcherPtr, new lineDisplayEvent(decodedRows));
				decodedRows++;

				if (decodedRows >= kHeight) {
					QApplication::postEvent(dispatcherPtr, new endImageSSTVRXEvent(trackingMode));
					trackingImage = false;
				}
			}
			dem->m_rPage++;
			if (dem->m_rPage >= SSTVDEMBUFMAX) dem->m_rPage = 0;
		}
	}
}
