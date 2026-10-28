#ifndef KGSTVIMAGE_H
#define KGSTVIMAGE_H
#include "kgstvprotocol.h"
#include <QImage>
namespace Kgstv {
// Experimental numerical reconstruction. Not compared against original audio/images.
bool encodeImageBlock(const QImage &image,int quality,Bits *payload,QString *error=nullptr);
bool decodeImageBlock(const Bits &payload,int quality,QImage *image,QString *error=nullptr);
// Exact reference raster only; input images are not silently resized.
// Individual blocks increase quantization when required by the 4095-bit wire bound.
bool prepareImage(const QImage &image,int quality,bool fec,bool fourLevel,QVector<Packet> *packets,QString *error=nullptr);
bool paintImageBlock(const Packet &packet,QImage *canvas,QString *error=nullptr);
}
#endif
