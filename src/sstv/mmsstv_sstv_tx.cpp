/***************************************************************************
 *   mmsstv-linux-port: Step 5/8/10/11 -- mode-aware TX bridge to         *
 *   mmsstv-core. See mmsstv_sstv_tx.h for what this is and why.          *
 ***************************************************************************/
#include "mmsstv_sstv_tx.h"

#include "imageviewer.h"
#include "synthes.h"

// mmsstv-core headers (see qsstv.pro's INCLUDEPATH additions).
#include "pixelconv.h"
#include "sstv.h"

#include <QImage>
#include <QRgb>

#include <cmath>
#include <vector>

namespace {

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

// The RGB family (Martin/Scottie) all share one encode signature (a whole-
// line duration in ms, the library divides by width itself). Robot 36
// (YUV family) has fixed per-segment ms literals instead of one scalable
// line-time, and needs the row index for its chroma-channel parity
// decision. Robot 72/24 (also YUV, but with no chroma-select parity
// decision at all -- see pixelconv.h's CRobotChromaRxDecoder comment)
// share a third, simpler signature (fixed literals, no row argument) --
// three genuinely different shapes, not worth forcing into one signature.
typedef void (*EncodeRgbLineFn)(CSSTVMOD *mod, double tw, const unsigned char *rgbRow, int width);
typedef void (*EncodeRobotChromaLineFn)(CSSTVMOD *mod, const unsigned char *rgbRow, int width);
enum PixelFamily { FAMILY_RGB, FAMILY_ROBOT36, FAMILY_ROBOT_CHROMA };

// One row per mmsstv-core-supported mode (see engineselection.h's
// mmsstvCoreSupports(), which must stay in sync with this table). VIS
// codes and line-time-ms constants match Main.cpp's SendSSTV()/VIS-code
// dispatch tables exactly (see mmsstv_martin_tx.cpp's original Step 5
// comment, pixelconv.h's EncodeScottieLine comment for Scottie's, and
// EncodeRobot36Line/EncodeRobot72Line/EncodeRobot24Line's comments for
// the Robot family's). width/height are this table's single source of
// truth (see getModeDimensions() below) -- every Robot mode is 320x240,
// every mode before Robot 36 was 320x256. `rowStep` is 2 for Robot 24
// (only even source rows are ever transmitted -- see pixelconv.h's
// CRobotChromaRxDecoder comment for exactly how this was confirmed in
// Main.cpp), 1 for every other mode.
struct ModeTxInfo
{
	esstvMode mode;
	int width;
	int height;
	int rowStep;
	PixelFamily family;
	int visCode;
	double lineTimeMs; // FAMILY_RGB only
	EncodeRgbLineFn encodeRgbLine; // FAMILY_RGB only
	EncodeRobotChromaLineFn encodeRobotChromaLine; // FAMILY_ROBOT_CHROMA only
};

const ModeTxInfo kModeTable[] = {
	{ M1, 320, 256, 1, FAMILY_RGB, 0xAC, 146.432, &EncodeMartinLine, nullptr },
	{ S1, 320, 256, 1, FAMILY_RGB, 0x3c, 138.24, &EncodeScottieLine, nullptr },
	{ S2, 320, 256, 1, FAMILY_RGB, 0xb8, 88.064, &EncodeScottieLine, nullptr },
	{ SDX, 320, 256, 1, FAMILY_RGB, 0xcc, 345.6, &EncodeScottieLine, nullptr },
	{ R36, 320, 240, 1, FAMILY_ROBOT36, 0x88, 0.0, nullptr, nullptr },
	{ R72, 320, 240, 1, FAMILY_ROBOT_CHROMA, 0x0c, 0.0, nullptr, &EncodeRobot72Line },
	{ R24, 320, 240, 2, FAMILY_ROBOT_CHROMA, 0x84, 0.0, nullptr, &EncodeRobot24Line },
};

const ModeTxInfo *findModeInfo(esstvMode mode)
{
	for (const auto &info : kModeTable) {
		if (info.mode == mode) return &info;
	}
	return nullptr;
}

} // namespace

bool getModeDimensions(esstvMode mode, int &width, int &height)
{
	const ModeTxInfo *info = findModeInfo(mode);
	if (!info) return false;
	width = info->width;
	height = info->height;
	return true;
}

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

	// QSSTV's own sstvparam.cpp table drives ivPtr's own scaling
	// (applyTemplate() -> setParam(numberOfPixels, numberOfDisplayLines)),
	// and for every mode except Robot 24 that already matches this
	// table's width/height exactly, so getScanLineAddress() below reads
	// a buffer of exactly the size we expect. Robot 24 is the one
	// exception: QSSTV's own table declares it 160x120 (its own,
	// different, 120-real-line-only convention -- see mmsstv-core's
	// pixelconv.h comment and the Step 11 plan's width-discrepancy
	// discussion), while this port matches MMSSTV's real 320-wide, 240-
	// tall (120 real lines, each duplicated) convention instead. Reading
	// getScanLineAddress(y) for y up to 238 against QSSTV's actual
	// 120-row buffer is an out-of-bounds heap read, not just a
	// wrong-looking picture. Detect any such mismatch generically (so a
	// future mode with the same kind of table disagreement is also
	// caught) and fall back to an independent scale of the original,
	// unscaled source image to exactly the dimensions this port needs.
	QImage localScaled;
	QImage *displayed = ivPtr->getDisplayedImage();
	bool sizeMismatch = displayed->isNull()
		|| displayed->width() != info->width
		|| displayed->height() != info->height;
	if (sizeMismatch) {
		localScaled = ivPtr->getImagePtr()->scaled(
			info->width, info->height,
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
			.convertToFormat(QImage::Format_RGB32);
	}

	std::vector<unsigned char> row(info->width * 3);
	for (int y = 0; y < info->height; y += info->rowStep) {
		QRgb *pixels = sizeMismatch
			? reinterpret_cast<QRgb *>(localScaled.scanLine(y))
			: ivPtr->getScanLineAddress(y);
		for (int x = 0; x < info->width; x++) {
			QRgb t = pixels[x];
			row[x * 3 + 0] = static_cast<unsigned char>(qRed(t));
			row[x * 3 + 1] = static_cast<unsigned char>(qGreen(t));
			row[x * 3 + 2] = static_cast<unsigned char>(qBlue(t));
		}
		switch (info->family) {
		case FAMILY_RGB:
			info->encodeRgbLine(&mod, info->lineTimeMs, row.data(), info->width);
			break;
		case FAMILY_ROBOT36:
			EncodeRobot36Line(&mod, row.data(), info->width, y);
			break;
		case FAMILY_ROBOT_CHROMA:
			info->encodeRobotChromaLine(&mod, row.data(), info->width);
			break;
		}
		drainToTx(mod);
	}

	return true;
}
