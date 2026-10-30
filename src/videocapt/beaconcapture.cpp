#include "beaconcapture.h"

#include "appglobal.h"

#include <QMediaDevices>
#include <QCameraDevice>
#include <QSettings>
#include <QVideoFrameFormat>

#define BEACONPREVIEWMS 250      // about 4 live pictures per second: plenty for a preview, little work for hours on end
#define BEACONIDLEMS 1000        // a frame is kept for grab() this often while nobody watches the live picture
#define BEACONFIRSTFRAME 10000   // a camera that was started has this long to deliver its first picture
#define BEACONSTALL 5000         // and this long between pictures afterwards
#define BEACONRETRY 5000         // a lost camera is opened again after this long
#define BEACONMAXWIDTH 1280

beaconCapture::beaconCapture(QObject *parent) :
  QObject(parent),
  cameraPtr(nullptr),
  running(false),
  previewWanted(false)
{
  captureSession.setVideoSink(&videoSink);
  connect(&videoSink,&QVideoSink::videoFrameChanged,this,&beaconCapture::slotVideoFrameChanged);
  connect(&mediaDevices,&QMediaDevices::videoInputsChanged,this,&beaconCapture::slotDevicesChanged);
  watchdog.setInterval(1000);
  connect(&watchdog,&QTimer::timeout,this,&beaconCapture::slotWatchdog);
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
  if(running) return;
  running=true;
  problem.clear();
  watchdog.start();
  openCamera();
}

void beaconCapture::stop()
{
  running=false;
  watchdog.stop();
  closeCamera();
  problem.clear();
}

void beaconCapture::openCamera()
{
  if(cameraPtr) return;
  QCameraDevice dev=chooseDevice();
  if(dev.isNull())
    {
      fail("no camera found");
      return;
    }
  cameraPtr=new QCamera(dev,this);
  captureSession.setCamera(cameraPtr);
  connect(cameraPtr,&QCamera::errorOccurred,this,&beaconCapture::slotCameraError);
  QCameraFormat fmt=chooseFormat(dev);
  if(!fmt.isNull()) cameraPtr->setCameraFormat(fmt);
  lastFrame.invalidate();
  openedAt.start();
  cameraPtr->start();
}

void beaconCapture::closeCamera()
{
  if(!cameraPtr) return;
  cameraPtr->stop();
  captureSession.setCamera(nullptr);
  cameraPtr->deleteLater();
  cameraPtr=nullptr;
  lastFrame.invalidate();
  lastImage=QImage();
  lastImageAt.invalidate();
}

// the camera is lost: close it, say so (once per reason) and let the watchdog open it again
void beaconCapture::fail(const QString &reason)
{
  closeCamera();
  lostAt.start();
  if(reason==problem) return;
  problem=reason;
  addToLog(QString("beacon camera: %1").arg(reason),LOGCAM);
  emit failed(reason);
}

void beaconCapture::slotVideoFrameChanged(const QVideoFrame &frame)
{
  if(!cameraPtr || !frame.isValid()) return;
  lastFrame.start();
  if(!problem.isEmpty())
    {
      problem.clear();
      emit recovered();
    }
  // A frame can only be converted while the camera hands it over (later it is gone), and converting every frame of a
  // webcam takes a good part of a core: so only every so often, and grab() gives the last picture converted.
  if(convertGate.isValid() && !convertGate.hasExpired(previewWanted ? BEACONPREVIEWMS : BEACONIDLEMS)) return;
  convertGate.start();
  QImage img=frame.toImage();
  if(img.isNull())
    {
      addToLog(QString("beacon camera: toImage() failed, pixelFormat=%1")
               .arg(QVideoFrameFormat::pixelFormatToString(frame.pixelFormat())),LOGCAM);
      return;
    }
  lastImage=img;
  lastImageAt.start();
  if(previewWanted) emit liveFrame(img);
}

QImage beaconCapture::grab(int maxAgeMs)
{
  if(!cameraPtr || lastImage.isNull() || !lastImageAt.isValid() || lastImageAt.elapsed()>maxAgeMs) return QImage();
  return lastImage;
}

// reopens a lost camera, and closes one that stopped delivering pictures (unplugged, USB reset, back from sleep)
void beaconCapture::slotWatchdog()
{
  if(!running) return;
  if(!cameraPtr)
    {
      if(lostAt.isValid() && lostAt.hasExpired(BEACONRETRY)) openCamera();
      return;
    }
  if(lastFrame.isValid())
    {
      if(lastFrame.hasExpired(BEACONSTALL)) fail("camera stopped sending pictures");
    }
  else if(openedAt.hasExpired(BEACONFIRSTFRAME))
    {
      fail("camera gave no picture");
    }
}

void beaconCapture::slotCameraError(QCamera::Error error,const QString &errorString)
{
  if(error==QCamera::NoError) return;
  fail(errorString.isEmpty() ? QString("camera error") : QString("camera error: %1").arg(errorString));
}

// a camera was plugged in: do not wait for the next try
void beaconCapture::slotDevicesChanged()
{
  if(running && !cameraPtr) openCamera();
}
