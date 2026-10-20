#include "beaconcapture.h"

#include "appglobal.h"

#include <QMediaDevices>
#include <QCameraDevice>
#include <QSettings>
#include <QVideoFrameFormat>

#define BEACONWARMUP 1500
#define BEACONTIMEOUT 10000
#define BEACONMAXWIDTH 1280

beaconCapture::beaconCapture(QObject *parent) :
  QObject(parent),
  cameraPtr(nullptr),
  warmedUp(false)
{
  captureSession.setVideoSink(&videoSink);
  connect(&videoSink,&QVideoSink::videoFrameChanged,this,&beaconCapture::slotVideoFrameChanged);
  warmupTimer.setSingleShot(true);
  timeoutTimer.setSingleShot(true);
  connect(&warmupTimer,&QTimer::timeout,this,&beaconCapture::slotWarmupDone);
  connect(&timeoutTimer,&QTimer::timeout,this,&beaconCapture::slotTimeout);
}

beaconCapture::~beaconCapture()
{
  delete cameraPtr;
}

// the camera last used in the snapshot dialog, else the system default
QCameraDevice beaconCapture::chooseDevice() const
{
  QSettings qSettings;
  qSettings.beginGroup("CAMERA");
  QString wanted=qSettings.value("device").toString();
  qSettings.endGroup();
  const QList<QCameraDevice> list=QMediaDevices::videoInputs();
  if(!wanted.isEmpty())
    {
      for(const QCameraDevice &d : list)
        if(d.description()==wanted) return d;
    }
  QCameraDevice def=QMediaDevices::defaultVideoInput();
  if(!def.isNull()) return def;
  return list.isEmpty() ? QCameraDevice() : list.first();
}

// the pixel format and resolution last chosen in the snapshot dialog; else the largest resolution up to
// BEACONMAXWIDTH (plenty for SSTV/DRM, and avoids huge MJPEG decodes)
QCameraFormat beaconCapture::chooseFormat(const QCameraDevice &cd) const
{
  QSettings qSettings;
  qSettings.beginGroup("CAMERA");
  QString wantedDevice=qSettings.value("device").toString();
  QString wantedFormat=qSettings.value("format").toString();
  QString wantedSize=qSettings.value("size").toString();
  qSettings.endGroup();
  QCameraFormat best;
  for(const QCameraFormat &f : cd.videoFormats())
    {
      if(cd.description()==wantedDevice
         && QVideoFrameFormat::pixelFormatToString(f.pixelFormat())==wantedFormat
         && QString("%1x%2").arg(f.resolution().width()).arg(f.resolution().height())==wantedSize)
        return f;
    }
  for(const QCameraFormat &f : cd.videoFormats())
    {
      if(f.resolution().width()>BEACONMAXWIDTH) continue;
      if(best.isNull() || f.resolution().width()>best.resolution().width()) best=f;
    }
  return best;
}

void beaconCapture::start()
{
  if(cameraPtr) return;
  QCameraDevice dev=chooseDevice();
  if(dev.isNull())
    {
      emit failed("no camera found");
      return;
    }
  lastImage=QImage();
  warmedUp=false;
  cameraPtr=new QCamera(dev,this);
  captureSession.setCamera(cameraPtr);
  connect(cameraPtr,&QCamera::errorOccurred,this,&beaconCapture::slotCameraError);
  QCameraFormat fmt=chooseFormat(dev);
  if(!fmt.isNull()) cameraPtr->setCameraFormat(fmt);
  warmupTimer.start(BEACONWARMUP);
  timeoutTimer.start(BEACONTIMEOUT);
  cameraPtr->start();
}

void beaconCapture::slotVideoFrameChanged(const QVideoFrame &frame)
{
  if(!cameraPtr || !frame.isValid()) return;
  QImage img=frame.toImage();
  if(img.isNull())
    {
      addToLog(QString("beacon camera: toImage() failed, pixelFormat=%1")
               .arg(QVideoFrameFormat::pixelFormatToString(frame.pixelFormat())),LOGCAM);
      return;
    }
  lastImage=img;
  if(warmedUp) finish(QString());
}

void beaconCapture::slotWarmupDone()
{
  warmedUp=true;
  if(!lastImage.isNull()) finish(QString());
}

void beaconCapture::slotTimeout()
{
  finish("camera gave no picture (timed out)");
}

void beaconCapture::slotCameraError(QCamera::Error error,const QString &errorString)
{
  if(error==QCamera::NoError) return;
  addToLog(QString("beacon camera error: %1").arg(errorString),LOGCAM);
  finish(errorString.isEmpty() ? QString("camera error") : QString("camera error: %1").arg(errorString));
}

void beaconCapture::abort()
{
  if(!cameraPtr) return;
  warmupTimer.stop();
  timeoutTimer.stop();
  cameraPtr->stop();
  captureSession.setCamera(nullptr);
  cameraPtr->deleteLater();
  cameraPtr=nullptr;
  lastImage=QImage();
}

void beaconCapture::finish(const QString &failure)
{
  if(!cameraPtr) return;
  warmupTimer.stop();
  timeoutTimer.stop();
  QImage img=lastImage;
  cameraPtr->stop();
  captureSession.setCamera(nullptr);
  cameraPtr->deleteLater();
  cameraPtr=nullptr;
  lastImage=QImage();
  if(failure.isEmpty()) emit frameReady(img);
  else emit failed(failure);
}
