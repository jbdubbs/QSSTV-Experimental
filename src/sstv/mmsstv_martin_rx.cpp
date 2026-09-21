/***************************************************************************
 *   mmsstv-linux-port: Step 7 -- Martin 1 RX bridge to mmsstv-core        *
 *   See mmsstv_martin_rx.h for what this is and why.                     *
 ***************************************************************************/
#include "mmsstv_martin_rx.h"

#include "appglobal.h"
#include "rxwidget.h"
#include "imageviewer.h"
#include "dispatcher.h"
#include "dispatchevents.h"
#include "sstvparam.h"

#include <QApplication>
#include <QSize>

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 256;
} // namespace

MmsstvMartinRx::MmsstvMartinRx()
{
	// Matches the TX bridge's choice (mmsstv_martin_tx.cpp): run at
	// QSSTV's native 48kHz rather than its decimated 12kHz RX rate, since
	// the raw tap (soundBase::rawRxBuffer) is un-decimated. Must happen
	// *before* `new CSSTVDEM` below -- see the member comment in the
	// header for why this can't just be a plain member object.
	SampFreq = 48000.0;
	SampBase = 48000.0;
	sys.m_SampFreq = SampFreq;
	dem = new CSSTVDEM();
}

MmsstvMartinRx::~MmsstvMartinRx()
{
	delete dem;
}

void MmsstvMartinRx::processSamples(const double *samples, int count)
{
	for (int i = 0; i < count; i++) {
		bool wasSync = dem->m_Sync != 0;
		dem->Do(samples[i]);

		if (!wasSync && dem->m_Sync) {
			// Just locked sync. Only Martin 1 is implemented here -- any
			// other mode's VIS code is recognized by CSSTVDEM's own
			// scanner (it isn't Martin-1-specific) but simply isn't acted
			// on; see mmsstv_martin_rx.h.
			decodedRows = 0;
			trackingImage = (SSTVSET.m_Mode == smMRT1);
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
			QApplication::postEvent(dispatcherPtr, new endImageSSTVRXEvent(M1));
			trackingImage = false;
		}

		while (dem->m_rPage != dem->m_wPage) {
			if (trackingImage && decodedRows < kHeight) {
				short *ip = &dem->m_Buf[dem->m_rPage * dem->m_BWidth];
				unsigned char rgbRow[kWidth * 3];
				rxDecoder.DecodeLine(ip, kWidth, rgbRow);

				QRgb *pixels = rxWidgetPtr->getImageViewerPtr()->getScanLineAddress(decodedRows);
				for (int x = 0; x < kWidth; x++) {
					pixels[x] = qRgb(rgbRow[x * 3 + 0], rgbRow[x * 3 + 1], rgbRow[x * 3 + 2]);
				}
				QApplication::postEvent(dispatcherPtr, new lineDisplayEvent(decodedRows));
				decodedRows++;

				if (decodedRows >= kHeight) {
					QApplication::postEvent(dispatcherPtr, new endImageSSTVRXEvent(M1));
					trackingImage = false;
				}
			}
			dem->m_rPage++;
			if (dem->m_rPage >= SSTVDEMBUFMAX) dem->m_rPage = 0;
		}
	}
}
