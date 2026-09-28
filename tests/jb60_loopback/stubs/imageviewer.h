#pragma once
#include <QImage>
#include <QRgb>
class imageViewer {
public:
  QImage img;
  bool hasValidImage(){return !img.isNull();}
  unsigned int *getScanLineAddress(int l){return (unsigned int*)img.scanLine(l);}
};
