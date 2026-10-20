#ifndef AVIFENC_H
#define AVIFENC_H

#include <QByteArray>
#include <QImage>

// AVIF through the Qt image plugin (kimageformats kimg_avif); no direct libavif dependency.
bool avifWriteSupported();

// Encode with the highest quality whose output still fits targetBytes. Quality 100 is lossless in the plugin and is
// never used. If even the lowest quality is larger than the target, that result is returned (and is over budget).
// decoded (optional) receives the decoded result, qualityUsed (optional) the chosen quality. Empty array on failure.
QByteArray avifEncode(const QImage &image,int targetBytes,QImage *decoded=nullptr,int *qualityUsed=nullptr);

#endif
