#ifndef FRAMEIMAGE_H
#define FRAMEIMAGE_H

#include <QImage>

class QVideoFrame;

/*!
  A camera frame as the picture the user sees. QVideoFrame::toImage() applies the transformation of the surface format
  (QVideoFrameFormat) but not the one of the frame itself, and that is the one a phone camera sets: its sensor is mounted
  sideways (rotation()), and the front camera is mirrored (mirrored()). A webcam on a PC reports neither, so there this
  gives what toImage() gives.
*/
QImage videoFrameImage(const QVideoFrame &frame);

/*!
  img turned clockwise by clockwiseDegrees (a multiple of 90, anything else is taken as the nearest one below), then mirrored
  around its vertical axis (left and right swapped) if mirrored: the order Qt documents for QVideoFrame.
*/
QImage orientedImage(const QImage &img,int clockwiseDegrees,bool mirrored);

#endif // FRAMEIMAGE_H
