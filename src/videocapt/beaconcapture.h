#ifndef BEACONCAPTURE_H
#define BEACONCAPTURE_H

// Headless webcam for the beacon (issues #83, #150). The camera stays on from start() until stop(), so its auto exposure
// has settled by the time a picture is taken. It delivers a throttled live picture for the Transmit tab and the newest
// frame on demand (grab()). A camera that fails or stops delivering frames is closed and opened again every few seconds.

#include <QObject>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>

class beaconCapture : public QObject
{
  Q_OBJECT

public:
  explicit beaconCapture(QObject *parent=nullptr);
  ~beaconCapture();
  //! switch the camera on and keep it on (no effect when already started)
  void start();
  //! switch the camera off
  void stop();
  bool isRunning() const {return running;}
  //! the newest picture (at most a second old while the live picture is off), or a null image when there is none or it is older than maxAgeMs
  QImage grab(int maxAgeMs=3000);
  //! liveFrame() is only emitted, and frames only converted often, while this is on
  void setPreviewWanted(bool on) {previewWanted=on;}
  //! why the camera is not delivering pictures (empty: it is)
  QString problemText() const {return problem;}

signals:
  void liveFrame(const QImage &image);
  void failed(const QString &reason);     //!< the camera was lost or never came up: it is tried again every few seconds
  void recovered();                       //!< pictures arrive again after failed()

private slots:
  void slotVideoFrameChanged(const QVideoFrame &frame);
  void slotWatchdog();
  void slotCameraError(QCamera::Error error,const QString &errorString);
  void slotDevicesChanged();

private:
  QCamera *cameraPtr;
  QMediaCaptureSession captureSession;
  QVideoSink videoSink;
  QMediaDevices mediaDevices;
  QTimer watchdog;
  QElapsedTimer openedAt;        // the camera was started
  QElapsedTimer lastFrame;       // the last frame arrived (invalid: none since the camera was started)
  QElapsedTimer convertGate;     // the last frame that was converted
  QElapsedTimer lastImageAt;     // when lastImage was converted
  QElapsedTimer lostAt;          // the camera was closed after a failure
  QImage lastImage;              // the newest converted frame
  QString problem;
  bool running;
  bool previewWanted;

  QCameraDevice chooseDevice() const;
  QCameraFormat chooseFormat(const QCameraDevice &cd) const;
  void openCamera();
  void closeCamera();
  void fail(const QString &reason);
};

#endif // BEACONCAPTURE_H
