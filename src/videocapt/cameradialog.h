#ifndef CAMERADIALOG_H
#define CAMERADIALOG_H

// Qt Multimedia camera backend (QCamera/QMediaCaptureSession/QVideoSink), replacing the
// V4L2-only videoCapture wrapper -- see Phase 5 of the cross-platform plan. QCamera
// already is the Qt-native equivalent of that wrapper, so there's no separate capture
// class any more; this dialog owns the QCamera/session/sink directly.

#include <QDialog>
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>

namespace Ui {
class cameraDialog;
}

class cameraDialog : public QDialog
{
  Q_OBJECT

public:
  explicit cameraDialog(QWidget *parent = 0);
  ~cameraDialog();
  int exec();
  QImage *getImage();

private slots:
  void slotSettings();
  void slotDeviceChanged(int idx);
  void slotFormatChanged(int idx);
  void slotSizeChanged(int idx);
  void slotVideoFrameChanged(const QVideoFrame &frame);

private:
  Ui::cameraDialog *ui;
  QList<QCameraDevice> cameraList;
  QCamera *cameraPtr;
  QMediaCaptureSession captureSession;
  QVideoSink videoSink;
  QImage lastImage;

  void listCameraDevices();
  void setupFormatComboBox(const QCameraDevice &cd);
  void setupSizeComboBox(const QCameraDevice &cd, int pixelFormat);
  QCameraFormat selectedFormat() const;
  bool restartCapturing();
};

#endif // CAMERADIALOG_H
