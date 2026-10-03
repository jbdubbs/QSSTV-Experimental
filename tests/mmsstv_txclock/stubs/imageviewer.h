#pragma once
#include <QImage>
#include <QRgb>
// Just the parts of imageViewer that mmsstv_sstv_tx.cpp reads.
class imageViewer {
public:
  QImage img;
  QImage *getDisplayedImage() {return &img;}
  QImage *getImagePtr() {return &img;}
  QRgb *getScanLineAddress(int l) {return reinterpret_cast<QRgb *>(img.scanLine(l));}
};
