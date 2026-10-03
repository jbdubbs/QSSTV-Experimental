/***************************************************************************
 *   mmsstv-linux-port: Step 5/8/10/11 -- mode-aware TX bridge to         *
 *   mmsstv-core. See mmsstv_sstv_tx.h for what this is and why.          *
 ***************************************************************************/
#include "mmsstv_sstv_tx.h"

#include "imageviewer.h"
#include "soundconfig.h"   // txClock
#include "synthes.h"

// mmsstv-core headers (see qsstv.pro's INCLUDEPATH additions).
#include "pixelconv.h"
#include "sstv.h"

#include <QImage>
#include <QRgb>

#include <atomic>
#include <cmath>
#include <vector>

namespace {

// Step 18: set from the GUI thread (requestMmsstvTxAbort(), called by
// txFunctions::stopAndWait()), polled from the TX thread inside
// sendImageViaMmsstv()'s row loop -- atomic since nothing else
// synchronizes the two threads here (unlike modeBase::abortRun's plain
// bool, which relies on the surrounding per-iteration function calls
// happening to act as compiler barriers).
std::atomic<bool> abortRequested{false};

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
// share a third, simpler signature (fixed literals, no row argument).
// The PD family (Step 12) needs a fourth shape entirely: one call encodes
// TWO source rows at once (sharing one chroma sample pair between them --
// see pixelconv.h's CPDRxDecoder/EncodePDLine comment), so it takes two
// row pointers and, like the RGB family, a scalable per-mode `tw`. The ML
// family (Step 13) shares Robot 72/24's RX decoder exactly, but its own
// TX encode function (EncodeMLLine) happens to need the exact same
// signature as the RGB family's (a scalable `tw`, no row-doubling) --
// reuses EncodeRgbLineFn's typedef rather than inventing a fifth one,
// since a genuinely identical function-pointer shape doesn't need its own
// name; `family` alone (not the typedef) is what a reader should look at
// to know ML's RX side actually dispatches through robotChromaDecoder,
// not a special ML decoder -- see mmsstv_sstv_rx.cpp.
typedef void (*EncodeRgbLineFn)(CSSTVMOD *mod, double tw, const unsigned char *rgbRow, int width);
typedef void (*EncodeRobotChromaLineFn)(CSSTVMOD *mod, const unsigned char *rgbRow, int width);
typedef void (*EncodePDLineFn)(CSSTVMOD *mod, double tw, const unsigned char *rgbRowEven, const unsigned char *rgbRowOdd, int width);
enum PixelFamily { FAMILY_RGB, FAMILY_ROBOT36, FAMILY_ROBOT_CHROMA, FAMILY_INTERLACED_YUV, FAMILY_ML_CHROMA };

// One row per mmsstv-core-supported mode (see engineselection.h's
// mmsstvCoreSupports(), which must stay in sync with this table). VIS
// codes and line-time-ms constants match Main.cpp's SendSSTV()/VIS-code
// dispatch tables exactly (see mmsstv_martin_tx.cpp's original Step 5
// comment, pixelconv.h's EncodeScottieLine comment for Scottie's, and
// EncodeRobot36Line/EncodeRobot72Line/EncodeRobot24Line's comments for
// the Robot family's). width/height are this table's single source of
// truth (see getModeDimensions() below) -- every Robot mode is 320x240,
// every mode before Robot 36 was 320x256, PD varies per-variant (see
// below). `rowStep` is 2 for Robot 24 (only even source rows are ever
// transmitted -- see pixelconv.h's CRobotChromaRxDecoder comment for
// exactly how this was confirmed in Main.cpp) and 2 for the PD family
// (every call reads/advances by TWO source rows, not one -- a different
// meaning of the same field, disambiguated by `family` at the call site),
// 1 for every other mode.
struct ModeTxInfo
{
	esstvMode mode;
	int width;
	int height;
	int rowStep;
	PixelFamily family;
	int visCode;
	double lineTimeMs; // FAMILY_RGB, FAMILY_INTERLACED_YUV, FAMILY_ML_CHROMA
	EncodeRgbLineFn encodeRgbLine; // FAMILY_RGB, FAMILY_ML_CHROMA (same fn-pointer shape)
	EncodeRobotChromaLineFn encodeRobotChromaLine; // FAMILY_ROBOT_CHROMA only
	EncodePDLineFn encodePDLine; // FAMILY_INTERLACED_YUV only
};

const ModeTxInfo kModeTable[] = {
	{ M1, 320, 256, 1, FAMILY_RGB, 0xAC, 146.432, &EncodeMartinLine, nullptr, nullptr },
	{ M2, 320, 256, 1, FAMILY_RGB, 0x28, 73.216, &EncodeMartinLine, nullptr, nullptr },
	{ S1, 320, 256, 1, FAMILY_RGB, 0x3c, 138.24, &EncodeScottieLine, nullptr, nullptr },
	{ S2, 320, 256, 1, FAMILY_RGB, 0xb8, 88.064, &EncodeScottieLine, nullptr, nullptr },
	{ SDX, 320, 256, 1, FAMILY_RGB, 0xcc, 345.6, &EncodeScottieLine, nullptr, nullptr },
	{ R36, 320, 240, 1, FAMILY_ROBOT36, 0x88, 0.0, nullptr, nullptr, nullptr },
	{ R72, 320, 240, 1, FAMILY_ROBOT_CHROMA, 0x0c, 0.0, nullptr, &EncodeRobot72Line, nullptr },
	{ R24, 320, 240, 2, FAMILY_ROBOT_CHROMA, 0x84, 0.0, nullptr, &EncodeRobot24Line, nullptr },
	{ PD50, 320, 256, 2, FAMILY_INTERLACED_YUV, 0xdd, 91.520, nullptr, nullptr, &EncodePDLine },
	{ PD90, 320, 256, 2, FAMILY_INTERLACED_YUV, 0x63, 170.240, nullptr, nullptr, &EncodePDLine },
	{ PD120, 640, 496, 2, FAMILY_INTERLACED_YUV, 0x5f, 121.600, nullptr, nullptr, &EncodePDLine },
	{ PD160, 512, 400, 2, FAMILY_INTERLACED_YUV, 0xe2, 195.584, nullptr, nullptr, &EncodePDLine },
	{ PD180, 640, 496, 2, FAMILY_INTERLACED_YUV, 0x60, 183.040, nullptr, nullptr, &EncodePDLine },
	{ PD240, 640, 496, 2, FAMILY_INTERLACED_YUV, 0xe1, 244.480, nullptr, nullptr, &EncodePDLine },
	{ PD290, 800, 616, 2, FAMILY_INTERLACED_YUV, 0xde, 228.800, nullptr, nullptr, &EncodePDLine },
	{ ML180, 640, 496, 1, FAMILY_ML_CHROMA, 0x8523, 176.5, &EncodeMLLine, nullptr, nullptr },
	{ ML240, 640, 496, 1, FAMILY_ML_CHROMA, 0x8623, 236.5, &EncodeMLLine, nullptr, nullptr },
	{ ML280, 640, 496, 1, FAMILY_ML_CHROMA, 0x8923, 277.5, &EncodeMLLine, nullptr, nullptr },
	{ ML320, 640, 496, 1, FAMILY_ML_CHROMA, 0x8a23, 317.5, &EncodeMLLine, nullptr, nullptr },
	// MR73-175 (Step 15): shares EncodeMLLine/FAMILY_ML_CHROMA exactly with
	// ML -- LineMR is fully parameterized by width, and MR's dimensions
	// (320x256) are just a different table row, not different code.
	{ MR73, 320, 256, 1, FAMILY_ML_CHROMA, 0x4523, 138.0, &EncodeMLLine, nullptr, nullptr },
	{ MR90, 320, 256, 1, FAMILY_ML_CHROMA, 0x4623, 171.0, &EncodeMLLine, nullptr, nullptr },
	{ MR115, 320, 256, 1, FAMILY_ML_CHROMA, 0x4923, 220.0, &EncodeMLLine, nullptr, nullptr },
	{ MR140, 320, 256, 1, FAMILY_ML_CHROMA, 0x4a23, 269.0, &EncodeMLLine, nullptr, nullptr },
	{ MR175, 320, 256, 1, FAMILY_ML_CHROMA, 0x4c23, 337.0, &EncodeMLLine, nullptr, nullptr },
	// MP73-175 (Step 16): shares PD's exact FAMILY_INTERLACED_YUV shape
	// and CPDRxDecoder for RX (confirmed by direct read of Main.cpp: MP
	// falls into PD's identical RX segment block) -- only TX differs, via
	// a new EncodeMPLine (a copy of EncodePDLine with different sync/porch
	// literals, see pixelconv.h's comment).
	{ MP73, 320, 256, 2, FAMILY_INTERLACED_YUV, 0x2523, 140.0, nullptr, nullptr, &EncodeMPLine },
	{ MP115, 320, 256, 2, FAMILY_INTERLACED_YUV, 0x2923, 223.0, nullptr, nullptr, &EncodeMPLine },
	{ MP140, 320, 256, 2, FAMILY_INTERLACED_YUV, 0x2a23, 270.0, nullptr, nullptr, &EncodeMPLine },
	{ MP175, 320, 256, 2, FAMILY_INTERLACED_YUV, 0x2c23, 340.0, nullptr, nullptr, &EncodeMPLine },
};

const ModeTxInfo *findModeInfo(esstvMode mode)
{
	for (const auto &info : kModeTable) {
		if (info.mode == mode) return &info;
	}
	return nullptr;
}

} // namespace

void requestMmsstvTxAbort()
{
	abortRequested.store(true, std::memory_order_relaxed);
}

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
	// The slot timing uses the calibrated transmit clock (Options > Calibrate); SampBase stays the nominal rate.
	// The globals are shared with the receive bridge, which sets its own again, but put them back anyway.
	struct clockRestore
	{
		double freq=SampFreq;
		double sysFreq=sys.m_SampFreq;
		double txOff=sys.m_TxSampOff;
		~clockRestore() {SampFreq=freq; sys.m_SampFreq=sysFreq; sys.m_TxSampOff=txOff;}
	} restoreClocks;
	SampBase = 48000.0;
	SampFreq = txClock;
	sys.m_SampFreq = SampFreq;
	sys.m_TxSampOff = 0.0;

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

	auto readRow = [&](int y, unsigned char *dst) {
		QRgb *pixels = sizeMismatch
			? reinterpret_cast<QRgb *>(localScaled.scanLine(y))
			: ivPtr->getScanLineAddress(y);
		for (int x = 0; x < info->width; x++) {
			QRgb t = pixels[x];
			dst[x * 3 + 0] = static_cast<unsigned char>(qRed(t));
			dst[x * 3 + 1] = static_cast<unsigned char>(qGreen(t));
			dst[x * 3 + 2] = static_cast<unsigned char>(qBlue(t));
		}
	};

	std::vector<unsigned char> row(info->width * 3);
	std::vector<unsigned char> rowOdd(info->family == FAMILY_INTERLACED_YUV ? info->width * 3 : 0);
	// Step 18: cleared here (not e.g. at function entry) to mirror
	// modeBase::transmitImage()'s own abortRun=false placed immediately
	// before its loop -- same small, accepted race window as that
	// existing pattern (a Stop click landing in the gap between this line
	// and the loop starting isn't realistically triggerable by a human).
	abortRequested.store(false, std::memory_order_relaxed);
	for (int y = 0; y < info->height; y += info->rowStep) {
		if (abortRequested.load(std::memory_order_relaxed)) return false;
		readRow(y, row.data());
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
		case FAMILY_INTERLACED_YUV:
			readRow(y + 1, rowOdd.data());
			info->encodePDLine(&mod, info->lineTimeMs, row.data(), rowOdd.data(), info->width);
			break;
		case FAMILY_ML_CHROMA:
			info->encodeRgbLine(&mod, info->lineTimeMs, row.data(), info->width);
			break;
		}
		drainToTx(mod);
	}

	return true;
}
