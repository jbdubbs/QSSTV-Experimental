#ifndef BEACONCAPTURE_H
#define BEACONCAPTURE_H

// Headless one-shot webcam grab for the beacon (issue #83). The camera is only open for the
// duration of a capture: start() opens it, waits for the exposure to settle, then emits
// frameReady() or failed() and closes it again.

#include <QObject>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QTimer>

class beaconCapture : public QObject
{
  Q_OBJECT

public:
  explicit beaconCapture(QObject *parent=nullptr);
  ~beaconCapture();
  void start();
  bool isBusy() const {return cameraPtr!=nullptr;}
  //! stop an unfinished capture without emitting anything
  void abort();

signals:
  void frameReady(const QImage &image);
  void failed(const QString &reason);

private slots:
  void slotVideoFrameChanged(const QVideoFrame &frame);
  void slotWarmupDone();
  void slotTimeout();
  void slotCameraError(QCamera::Error error,const QString &errorString);

private:
  QCamera *cameraPtr;
  QMediaCaptureSession captureSession;
  QVideoSink videoSink;
  QTimer warmupTimer;
  QTimer timeoutTimer;
  QImage lastImage;
  bool warmedUp;

  QCameraDevice chooseDevice() const;
  QCameraFormat chooseFormat(const QCameraDevice &cd) const;
  void finish(const QString &failure);
};

#endif // BEACONCAPTURE_H
