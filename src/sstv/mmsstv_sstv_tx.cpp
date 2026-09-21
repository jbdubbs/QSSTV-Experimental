/***************************************************************************
 *   mmsstv-linux-port: Step 5/8 -- mode-aware TX bridge to mmsstv-core    *
 *   See mmsstv_sstv_tx.h for what this is and why.                       *
 ***************************************************************************/
#include "mmsstv_sstv_tx.h"

#include "imageviewer.h"
#include "synthes.h"

// mmsstv-core headers (see qsstv.pro's INCLUDEPATH additions).
#include "pixelconv.h"
#include "sstv.h"

#include <QRgb>

#include <cmath>
#include <vector>

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 256;

// Drains whatever CSSTVMOD::Write() calls have queued so far into real
// audio via the public bulk API (synthesizer::writeBuffer() -- write()/
// filter() are private). Packing matches synthesizer::filter(): SOUNDFRAME
// is quint32, low 16 bits = left-channel int16 sample, high 16 bits unused
// here (that's only for VOX-tone keying on the other channel, which this
// bridge doesn't need -- QSSTV's normal TX start already handles PTT).
void drainToTx(CSSTVMOD &mod)
{
	std::vector<quint32> frames;
	frames.reserve(4096);
	while (mod.GetBufCnt() > 0) {
		double sample = mod.Do();
		frames.push_back(static_cast<quint32>(std::lround(sample)) & 0xFFFFu);
	}
	if (!frames.empty()) {
		synthesPtr->writeBuffer(frames.data(), int(frames.size()));
	}
}

typedef void (*EncodeLineFn)(CSSTVMOD *mod, double tw, const unsigned char *rgbRow, int width);

// One row per mmsstv-core-supported mode (see engineselection.h's
// mmsstvCoreSupports(), which must stay in sync with this table). VIS
// codes and line-time-ms constants match Main.cpp's SendSSTV()/VIS-code
// dispatch tables exactly (see mmsstv_martin_tx.cpp's original Step 5
// comment and pixelconv.h's EncodeScottieLine comment for Scottie's).
struct ModeTxInfo
{
	esstvMode mode;
	int visCode;
	double lineTimeMs;
	EncodeLineFn encodeLine;
};

const ModeTxInfo kModeTable[] = {
	{ M1, 0xAC, 146.432, &EncodeMartinLine },
	{ S1, 0x3c, 138.24, &EncodeScottieLine },
};

const ModeTxInfo *findModeInfo(esstvMode mode)
{
	for (const auto &info : kModeTable) {
		if (info.mode == mode) return &info;
	}
	return nullptr;
}

} // namespace

bool sendImageViaMmsstv(imageViewer *ivPtr, esstvMode mode)
{
	const ModeTxInfo *info = findModeInfo(mode);
	if (!info) return false; // caller should have checked mmsstvCoreSupports() first

	// Run at QSSTV's native TX rate -- resolves the RX/TX shared-rate
	// requirement flagged in Step 4 without any resampling, since TX here
	// needs no new audio tap (unlike RX).
	SampFreq = 48000.0;
	SampBase = 48000.0;
	sys.m_SampFreq = SampFreq;

	SSTVSET.SetTxMode(mode);

	CSSTVMOD mod;
	mod.OpenTXBuf(10);
	mod.InitTXBuf();

	SendVisHeader(&mod, info->visCode);
	drainToTx(mod);

	unsigned char row[kWidth * 3];
	for (int y = 0; y < kHeight; y++) {
		QRgb *pixels = ivPtr->getScanLineAddress(y);
		for (int x = 0; x < kWidth; x++) {
			QRgb t = pixels[x];
			row[x * 3 + 0] = static_cast<unsigned char>(qRed(t));
			row[x * 3 + 1] = static_cast<unsigned char>(qGreen(t));
			row[x * 3 + 2] = static_cast<unsigned char>(qBlue(t));
		}
		info->encodeLine(&mod, info->lineTimeMs, row, kWidth);
		drainToTx(mod);
	}

	return true;
}
