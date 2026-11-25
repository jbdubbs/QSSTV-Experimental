// The GPIO PTT of a CM108 USB sound-card interface. The way the HID report reaches the chip is ported from the CM108 backend of
// Hamlib (src/cm108.c, LGPL, with the native Windows and macOS backends of the Hamlib-RCW branch), so it works on all three systems
// without Hamlib and without hidapi.
#include "cm108lines.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <algorithm>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#define CM108_LINUX 1
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#define CM108_WIN32 1
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#elif defined(Q_OS_MACOS)
#define CM108_MACOS 1
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDManager.h>
#endif

namespace
{
const int reportLen=5;   // the report number and the 4 bytes of the report
}

bool cm108Ptt::compatible(unsigned vendor,unsigned product)
{
  // CM108/108B/109/119/119A/119B and SSS1621/23, as Hamlib has them (detection by Thomas Sailer's soundmodem)
  return (vendor==0x0d8c
          && ((product>=0x0008 && product<=0x000f) || product==0x0012 || product==0x013a || product==0x013c || product==0x0013))
         || (vendor==0x0c76 && (product==0x1605 || product==0x1607 || product==0x160b));
}

bool cm108Ptt::isAuto(const QString &device)
{
  const QString d=device.trimmed();
  return d.isEmpty() || d.compare(QStringLiteral("auto"),Qt::CaseInsensitive)==0;
}

QString cm108Ptt::pick(const QList<cm108Device> &devices,QString &why)
{
  if(devices.count()==1) return devices.first().path;
  if(devices.isEmpty())
    {
      why=QStringLiteral("no CM108 device is connected");
      return QString();
    }
  QStringList names;
  for(const cm108Device &dev : devices) names << dev.path;
  why=QString("%1 CM108 devices are connected, choose one: %2").arg(devices.count()).arg(names.join(", "));
  return QString();
}

QByteArray cm108Ptt::report(int gpio,bool on)
{
  const int bit=std::min(std::max(gpio,1),4)-1;
  QByteArray rep(reportLen,'\0');
  // byte 0 the report number, byte 1 "write GPIO" (00xx xxxx), byte 2 the output values (1 = high), byte 3 the data direction
  // (1 = output), byte 4 SPDIF (unused)
  rep[2]=on ? char(1<<bit) : char(0);
  rep[3]=char(1<<bit);
  return rep;
}

bool cm108Ptt::parseUevent(const QString &text,unsigned &vendor,unsigned &product,QString &name)
{
  static const QRegularExpression idRe("^HID_ID=[0-9A-Fa-f]+:([0-9A-Fa-f]+):([0-9A-Fa-f]+)\\s*$",QRegularExpression::MultilineOption);
  static const QRegularExpression nameRe("^HID_NAME=(.*)$",QRegularExpression::MultilineOption);
  const QRegularExpressionMatch id=idRe.match(text);
  if(!id.hasMatch()) return false;
  bool ok1=false,ok2=false;
  vendor=id.captured(1).toUInt(&ok1,16);
  product=id.captured(2).toUInt(&ok2,16);
  if(!ok1 || !ok2) return false;
  const QRegularExpressionMatch nm=nameRe.match(text);
  name=nm.hasMatch() ? nm.captured(1).simplified() : QString();
  return true;
}

QString cm108Ptt::advice()
{
#if defined(CM108_LINUX)
  return "Check that the interface is plugged in and that you have read and write permission on its /dev/hidraw device "
         "(a udev rule gives it; see the User's Guide), or switch CM108 PTT off.";
#else
  return "Check that the interface is plugged in and that no other program is using it, or switch CM108 PTT off.";
#endif
}

#if defined(CM108_LINUX)

struct cm108Ptt::impl
{
  int fd=-1;
};

static QString hex4(unsigned v)
{
  return QString("%1").arg(v,4,16,QLatin1Char('0'));
}

QList<cm108Device> cm108Ptt::list(const QString &sysRoot)
{
  QList<cm108Device> devices;
  static const QRegularExpression nodeRe("^hidraw(\\d+)$");
  QStringList names;
  for(const QString &n : QDir(sysRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) if(nodeRe.match(n).hasMatch()) names << n;
  std::sort(names.begin(),names.end(),[](const QString &a,const QString &b)
  {
    return nodeRe.match(a).captured(1).toInt()<nodeRe.match(b).captured(1).toInt();
  });
  for(const QString &n : names)
    {
      QFile f(sysRoot+"/"+n+"/device/uevent");
      if(!f.open(QIODevice::ReadOnly)) continue;
      unsigned vendor,product;
      QString name;
      if(!parseUevent(QString::fromUtf8(f.readAll()),vendor,product,name) || !compatible(vendor,product)) continue;
      cm108Device dev;
      dev.path="/dev/"+n;
      dev.label=QString("%1 (%2:%3) [%4]").arg(name.isEmpty() ? QStringLiteral("CM108") : name,hex4(vendor),hex4(product),n);
      devices << dev;
    }
  return devices;
}

static bool platformOpen(cm108Ptt::impl *d,const QString &path,QString &why)
{
  d->fd=::open(path.toLocal8Bit().constData(),O_RDWR);
  if(d->fd<0)
    {
      why=QString("%1: %2").arg(path,QString::fromLocal8Bit(strerror(errno)));
      return false;
    }
  struct hidraw_devinfo info;
  if(ioctl(d->fd,HIDIOCGRAWINFO,&info)!=0
     || !cm108Ptt::compatible((unsigned)info.vendor & 0xffff,(unsigned)info.product & 0xffff))
    {
      ::close(d->fd);
      d->fd=-1;
      why=QString("%1 is not a CM108 (or compatible) device").arg(path);
      return false;
    }
  return true;
}

static void platformClose(cm108Ptt::impl *d)
{
  if(d->fd>=0) ::close(d->fd);
  d->fd=-1;
}

static bool platformSend(cm108Ptt::impl *d,const QByteArray &rep,QString &why)
{
  if(d->fd<0)
    {
      why=QStringLiteral("the device is not open");
      return false;
    }
  if(::write(d->fd,rep.constData(),rep.size())<0)
    {
      why=QString::fromLocal8Bit(strerror(errno));
      return false;
    }
  return true;
}

#elif defined(CM108_WIN32)

struct cm108Ptt::impl
{
  HANDLE h=INVALID_HANDLE_VALUE;
  DWORD outLen=0;   // OutputReportByteLength, with the report number
};

static HANDLE winOpen(const char *path,DWORD access)
{
  return CreateFileA(path,access,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
}

// is this handle a CM108 with a GPIO output report?
static bool winCheck(HANDLE h,DWORD *outLen,QString *name,unsigned *vid,unsigned *pid)
{
  HIDD_ATTRIBUTES attr;
  attr.Size=sizeof(attr);
  if(!HidD_GetAttributes(h,&attr) || !cm108Ptt::compatible(attr.VendorID,attr.ProductID)) return false;
  bool ok=false;
  PHIDP_PREPARSED_DATA pp=NULL;
  if(HidD_GetPreparsedData(h,&pp))
    {
      HIDP_CAPS caps;
      if(HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS && caps.OutputReportByteLength>=reportLen)
        {
          if(outLen) *outLen=caps.OutputReportByteLength;
          ok=true;
        }
      HidD_FreePreparsedData(pp);
    }
  if(ok)
    {
      if(vid) *vid=attr.VendorID;
      if(pid) *pid=attr.ProductID;
      if(name)
        {
          wchar_t product[127]={0};
          if(HidD_GetProductString(h,product,sizeof(product))) *name=QString::fromWCharArray(product).simplified();
        }
    }
  return ok;
}

QList<cm108Device> cm108Ptt::list(const QString &)
{
  QList<cm108Device> devices;
  GUID guid;
  HidD_GetHidGuid(&guid);
  HDEVINFO info=SetupDiGetClassDevsA(&guid,NULL,NULL,DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if(info==INVALID_HANDLE_VALUE) return devices;
  SP_DEVICE_INTERFACE_DATA ifd;
  ifd.cbSize=sizeof(ifd);
  for(DWORD idx=0;SetupDiEnumDeviceInterfaces(info,NULL,&guid,idx,&ifd);idx++)
    {
      DWORD need=0;
      SetupDiGetDeviceInterfaceDetailA(info,&ifd,NULL,0,&need,NULL);
      if(need==0) continue;
      QByteArray buf(int(need),'\0');
      SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A *>(buf.data());
      detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
      if(!SetupDiGetDeviceInterfaceDetailA(info,&ifd,detail,need,NULL,NULL)) continue;
      // access 0 is enough to ask for the attributes and never gets in the way of another program
      HANDLE h=winOpen(detail->DevicePath,0);
      if(h==INVALID_HANDLE_VALUE) continue;
      QString name;
      unsigned vid=0,pid=0;
      if(winCheck(h,NULL,&name,&vid,&pid))
        {
          cm108Device dev;
          dev.path=QString::fromLocal8Bit(detail->DevicePath);
          dev.label=QString("%1 (%2:%3)").arg(name.isEmpty() ? QStringLiteral("CM108") : name,
                                              QString("%1").arg(vid,4,16,QLatin1Char('0')),QString("%1").arg(pid,4,16,QLatin1Char('0')));
          devices << dev;
        }
      CloseHandle(h);
    }
  SetupDiDestroyDeviceInfoList(info);
  return devices;
}

static bool platformOpen(cm108Ptt::impl *d,const QString &path,QString &why)
{
  d->h=winOpen(path.toLocal8Bit().constData(),GENERIC_READ | GENERIC_WRITE);
  if(d->h==INVALID_HANDLE_VALUE)
    {
      why=QString("%1: Windows error %2").arg(path).arg(GetLastError());
      return false;
    }
  if(!winCheck(d->h,&d->outLen,NULL,NULL,NULL))
    {
      CloseHandle(d->h);
      d->h=INVALID_HANDLE_VALUE;
      why=QString("%1 is not a CM108 (or compatible) device").arg(path);
      return false;
    }
  return true;
}

static void platformClose(cm108Ptt::impl *d)
{
  if(d->h!=INVALID_HANDLE_VALUE) CloseHandle(d->h);
  d->h=INVALID_HANDLE_VALUE;
}

static bool platformSend(cm108Ptt::impl *d,const QByteArray &rep,QString &why)
{
  if(d->h==INVALID_HANDLE_VALUE || d->outLen==0 || d->outLen>256)
    {
      why=QStringLiteral("the device is not open");
      return false;
    }
  // the report must be exactly OutputReportByteLength bytes long
  QByteArray buf(int(d->outLen),'\0');
  buf.replace(0,rep.size(),rep);
  DWORD written=0;
  if(WriteFile(d->h,buf.constData(),d->outLen,&written,NULL)) return true;
  // a control transfer when the interrupt endpoint refuses
  if(HidD_SetOutputReport(d->h,(PVOID)buf.constData(),d->outLen)) return true;
  why=QString("Windows error %1").arg(GetLastError());
  return false;
}

#elif defined(CM108_MACOS)

struct cm108Ptt::impl
{
  IOHIDManagerRef mgr=NULL;
  IOHIDDeviceRef dev=NULL;
};

static int32_t macInt(IOHIDDeviceRef dev,CFStringRef key)
{
  CFTypeRef ref=IOHIDDeviceGetProperty(dev,key);
  int32_t value=0;
  if(ref && CFGetTypeID(ref)==CFNumberGetTypeID()) CFNumberGetValue((CFNumberRef)ref,kCFNumberSInt32Type,&value);
  return value;
}

static QString macString(IOHIDDeviceRef dev,CFStringRef key)
{
  CFTypeRef ref=IOHIDDeviceGetProperty(dev,key);
  if(!ref || CFGetTypeID(ref)!=CFStringGetTypeID()) return QString();
  char buf[256];
  if(!CFStringGetCString((CFStringRef)ref,buf,sizeof(buf),kCFStringEncodingUTF8)) return QString();
  return QString::fromUtf8(buf).simplified();
}

// the path hidapi uses: the registry entry ID of the device
static bool macPath(IOHIDDeviceRef dev,QString &path)
{
  io_service_t svc=IOHIDDeviceGetService(dev);
  uint64_t id;
  if(svc==MACH_PORT_NULL || IORegistryEntryGetRegistryEntryID(svc,&id)!=KERN_SUCCESS) return false;
  path=QString("DevSrvsID:%1").arg((qulonglong)id);
  return true;
}

// a manager that matches only the two vendors of the CM108 family, so no keyboard or mouse is ever touched
static IOHIDManagerRef macManager()
{
  static const int vendors[]={0x0d8c,0x0c76};
  IOHIDManagerRef mgr=IOHIDManagerCreate(kCFAllocatorDefault,kIOHIDOptionsTypeNone);
  if(!mgr) return NULL;
  CFMutableArrayRef arr=CFArrayCreateMutable(kCFAllocatorDefault,0,&kCFTypeArrayCallBacks);
  for(int i=0;i<2;i++)
    {
      CFMutableDictionaryRef d=CFDictionaryCreateMutable(kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
      CFNumberRef v=CFNumberCreate(kCFAllocatorDefault,kCFNumberIntType,&vendors[i]);
      CFDictionarySetValue(d,CFSTR(kIOHIDVendorIDKey),v);
      CFArrayAppendValue(arr,d);
      CFRelease(v);
      CFRelease(d);
    }
  IOHIDManagerSetDeviceMatchingMultiple(mgr,arr);
  CFRelease(arr);
  IOHIDManagerScheduleWithRunLoop(mgr,CFRunLoopGetCurrent(),kCFRunLoopDefaultMode);
  // a moment for the manager to find the devices
  int i=0;
  SInt32 res;
  do
    {
      res=CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.001,false);
    }
  while(res!=kCFRunLoopRunFinished && res!=kCFRunLoopRunTimedOut && ++i<100);
  return mgr;
}

static void macRelease(IOHIDManagerRef mgr)
{
  if(!mgr) return;
  IOHIDManagerUnscheduleFromRunLoop(mgr,CFRunLoopGetCurrent(),kCFRunLoopDefaultMode);
  CFRelease(mgr);
}

// calls \a each for every CM108 the manager has (the device and its path)
template<typename F> static void macForEach(IOHIDManagerRef mgr,F each)
{
  CFSetRef set=IOHIDManagerCopyDevices(mgr);
  if(!set) return;
  const CFIndex n=CFSetGetCount(set);
  QList<IOHIDDeviceRef> devs;
  devs.resize(int(n));
  if(n>0) CFSetGetValues(set,(const void **)devs.data());
  for(CFIndex i=0;i<n;i++)
    {
      QString path;
      if(!cm108Ptt::compatible(macInt(devs[int(i)],CFSTR(kIOHIDVendorIDKey)),macInt(devs[int(i)],CFSTR(kIOHIDProductIDKey)))
         || !macPath(devs[int(i)],path)) continue;
      each(devs[int(i)],path);
    }
  CFRelease(set);
}

QList<cm108Device> cm108Ptt::list(const QString &)
{
  QList<cm108Device> devices;
  IOHIDManagerRef mgr=macManager();
  if(!mgr) return devices;
  macForEach(mgr,[&devices](IOHIDDeviceRef dev,const QString &path)
  {
    cm108Device d;
    d.path=path;
    const QString name=macString(dev,CFSTR(kIOHIDProductKey));
    d.label=QString("%1 (%2:%3)").arg(name.isEmpty() ? QStringLiteral("CM108") : name,
                                      QString("%1").arg(macInt(dev,CFSTR(kIOHIDVendorIDKey)),4,16,QLatin1Char('0')),
                                      QString("%1").arg(macInt(dev,CFSTR(kIOHIDProductIDKey)),4,16,QLatin1Char('0')));
    devices << d;
  });
  macRelease(mgr);
  return devices;
}

static void macDrop(cm108Ptt::impl *d)
{
  if(d->dev) CFRelease(d->dev);
  d->dev=NULL;
  macRelease(d->mgr);
  d->mgr=NULL;
}

static bool platformOpen(cm108Ptt::impl *d,const QString &path,QString &why)
{
  d->mgr=macManager();
  if(!d->mgr)
    {
      why=QStringLiteral("the HID manager of macOS cannot be created");
      return false;
    }
  IOHIDDeviceRef found=NULL;
  macForEach(d->mgr,[&found,&path](IOHIDDeviceRef dev,const QString &p)
  {
    if(p==path) found=dev;
  });
  if(!found)
    {
      macDrop(d);
      why=QString("%1 is not a CM108 (or compatible) device").arg(path);
      return false;
    }
  CFRetain(found);
  d->dev=found;
  // not seized: macOS and other programs keep the HID interface of the device
  const IOReturn ret=IOHIDDeviceOpen(d->dev,kIOHIDOptionsTypeNone);
  if(ret!=kIOReturnSuccess)
    {
      macDrop(d);
      why=QString("IOHIDDeviceOpen failed (0x%1)").arg((unsigned)ret,8,16,QLatin1Char('0'));
      return false;
    }
  return true;
}

static void platformClose(cm108Ptt::impl *d)
{
  if(d->dev) IOHIDDeviceClose(d->dev,kIOHIDOptionsTypeNone);
  macDrop(d);
}

static bool platformSend(cm108Ptt::impl *d,const QByteArray &rep,QString &why)
{
  if(!d->dev)
    {
      why=QStringLiteral("the device is not open");
      return false;
    }
  // report number 0 means unnumbered reports: it is passed as the report ID and is not part of the buffer
  const IOReturn ret=IOHIDDeviceSetReport(d->dev,kIOHIDReportTypeOutput,(unsigned char)rep[0],
                                          (const uint8_t *)rep.constData()+1,rep.size()-1);
  if(ret!=kIOReturnSuccess)
    {
      why=QString("IOHIDDeviceSetReport failed (0x%1)").arg((unsigned)ret,8,16,QLatin1Char('0'));
      return false;
    }
  return true;
}

#else   // Android, the BSDs: no way to reach the chip (yet)

struct cm108Ptt::impl
{
};

QList<cm108Device> cm108Ptt::list(const QString &)
{
  return QList<cm108Device>();
}

static bool platformOpen(cm108Ptt::impl *,const QString &,QString &why)
{
  why=QStringLiteral("CM108 PTT is not supported on this system");
  return false;
}

static void platformClose(cm108Ptt::impl *)
{
}

static bool platformSend(cm108Ptt::impl *,const QByteArray &,QString &why)
{
  why=QStringLiteral("CM108 PTT is not supported on this system");
  return false;
}

#endif

cm108Ptt::cm108Ptt() : d(new impl)
{
}

cm108Ptt::~cm108Ptt()
{
  close();
  delete d;
}

bool cm108Ptt::open(const QString &device,QString &why)
{
  close();
  QString path=device.trimmed();
  if(isAuto(path))
    {
      path=pick(list(),why);
      if(path.isEmpty()) return false;
    }
  opened=platformOpen(d,path,why);
  return opened;
}

bool cm108Ptt::isOpen() const
{
  return opened;
}

bool cm108Ptt::setGpio(int gpio,bool on,QString &why)
{
  if(!opened)
    {
      why=QStringLiteral("the device is not open");
      return false;
    }
  return platformSend(d,report(gpio,on),why);
}

void cm108Ptt::close()
{
  if(opened) platformClose(d);
  opened=false;
}
