#ifndef AVIFENC_H
#define AVIFENC_H

#include <QByteArray>
#include <QImage>

// AVIF through libavif directly (libaom encoder); the Qt image plugin is not involved, so DRM AVIF works without it.
// Without libavif at build time (HAVE_LIBAVIF unset) avifWriteSupported() is false and everything else fails cleanly.
bool avifWriteSupported();

// Encode with the highest quality whose output still fits targetBytes. Quality 100 is lossless and is never used.
// If even the lowest quality is larger than the target, that result is returned (and is over budget).
// decoded (optional) receives the decoded result, qualityUsed (optional) the chosen quality. Empty array on failure.
QByteArray avifEncode(const QImage &image,int targetBytes,QImage *decoded=nullptr,int *qualityUsed=nullptr);

// One encode at a fixed quality (1..99). speed 0 (slowest) .. 10, chroma 420 or 444.
// avifEncode() uses fixed settings; this is for tests/benchmarks.
QByteArray avifEncodeQuality(const QImage &image,int quality,int speed,int chroma);

// Decode an AVIF file held in memory. Null image on failure. If the file carries rotation/mirror transforms, they are
// not applied and *transformed (optional) is set, so a caller can prefer a loader that honours them.
QImage avifDecode(const QByteArray &data,bool *transformed=nullptr);

// True if the data looks like an AVIF/AVIS file (ISO BMFF 'ftyp' box with an avif brand).
bool avifSniff(const QByteArray &data);

#endif
