#ifndef DISPATCHEVENT_H
#define DISPATCHEVENT_H
#include <QEvent>
#include "imageviewer.h"
#include "appdefs.h"
#include "hybridcrypt.h"
#include <unistd.h>
#include "sstvparam.h"


/** dispatch events are used to communicate with the different threads */
enum dispatchEventType
{
  displayFFT = QEvent::User,
  displaySync,
  displayDRMStat,
  displayDRMInfo,
  lineDisplay,				//!< display 1 line
  startImageRX,
  endSSTVImageRX,
  endImageTX,
  progressTX,
  rxSSTVStatus,     //! shows message in sstv tab
  rxDRMStatus,     //! shows message in drm tab
  txDRMNotify,  //! shows text in tx notifications box
  txDRMNotifyAppend,
  txPrepareComplete, //!< tx preparations (uploading etc) complete
  callEditor,
  templatesChanged,
  loadRXImage,
  saveDRMImage,
  previewDRMImage,
  prepareFix,
  displayText,
  displayMBox,
  displayProgressFTP,
  moveToTx,
  notifyCheck,
  statusBarMsg
};

class baseEvent: public QEvent
{
public:
  baseEvent(QEvent::Type t):QEvent(t) {doneIt=NULL;}
  void waitFor(bool *d) {doneIt=d;}
  void setDone()
  {
    if(doneIt!=NULL) *doneIt=true;
  }
  QString description;
private:
  bool *doneIt;

};


/**
  this event is send with the sync quality info and the signal volume
*/
class displaySyncEvent : public baseEvent
{
public:
  /** create event */
  displaySyncEvent(uint s):baseEvent( (QEvent::Type) displaySync), sync(s)
  {
    description=" displaySyncEvent";
  }
  /** returns int sync value */
  void getInfo(uint &s)  {s=sync;}

private:
  uint sync;
//  DSPFLOAT vol;
};

class displayDRMStatEvent  : public baseEvent
{
public:
  /** create event */
  displayDRMStatEvent(uint s):baseEvent( (QEvent::Type) displayDRMStat), snr(s)
  {
    description=" displayDRMStatEvent";
  }
  /** returns length and pointer  from the event */
  void getInfo(DSPFLOAT &s)  {s=snr;}

private:
  DSPFLOAT snr;
};





class notifyCheckEvent : public baseEvent
{
public:
  /** create event */
  notifyCheckEvent(QString fn):baseEvent( (QEvent::Type)notifyCheck),filename(fn)
  {
    description="notifyCheckEvent";
  }
  QString getFilename() const { return filename; }
private:
  QString filename;
};



class rxSSTVStatusEvent : public baseEvent
{
public:
  /** create event */
  rxSSTVStatusEvent(QString t):baseEvent( (QEvent::Type)rxSSTVStatus ), str(t)
  {
    description="rxSSTVStatusEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};

class rxDRMStatusEvent : public baseEvent
{
public:
  /** create event */
  rxDRMStatusEvent(QString t):baseEvent( (QEvent::Type)rxDRMStatus ), str(t)
  {
    description="rxDRMStatusEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};



class txDRMNotifyEvent : public baseEvent
{
public:
  /** create event */
  txDRMNotifyEvent(QString t):baseEvent( (QEvent::Type)txDRMNotify ), str(t)
  {
    description="txDRMNotifyEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};

class txDRMNotifyAppendEvent : public baseEvent
{
public:
  /** create event */
  txDRMNotifyAppendEvent(QString t):baseEvent( (QEvent::Type)txDRMNotifyAppend ), str(t)
  {
    description="txDRMNotifyAppendEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};

class txPrepareCompleteEvent : public baseEvent
{
public:
  /** create event */
  txPrepareCompleteEvent(bool tok,quint64 ticket=0):baseEvent( (QEvent::Type)txPrepareComplete ), _ok(tok),_ticket(ticket)
  {
    description="txPrepareCompleteEvent";
  }
  /** returns info string from the event */
  bool ok() const { return _ok; }
  quint64 ticket() const {return _ticket;}
private:
  bool _ok;
  quint64 _ticket;
};

class lineDisplayEvent : public baseEvent
{
public:
  /** create event */
  lineDisplayEvent(uint lineNbr):baseEvent( (QEvent::Type)lineDisplay ), lineNumber(lineNbr)
  {
    description="lineDisplayEvent";
  }
  /** returns length and pointer  from the event */
  void getInfo(uint &lineNbr) const { lineNbr=lineNumber;}

private:
  uint lineNumber;
};



class displayDRMInfoEvent : public baseEvent
{
public:
  /** create event */
  displayDRMInfoEvent():baseEvent( (QEvent::Type)displayDRMInfo)
  {
    description="displayDRMInfo";
  }
};


class loadRXImageEvent : public baseEvent
{
public:
  loadRXImageEvent(QString fn):baseEvent( (QEvent::Type)loadRXImage),fileName(fn)
  {
    description="loadRXImageEvent";
  }
  QString getFilename() {return fileName;}
private:
  QString fileName;
};


class moveToTxEvent : public baseEvent
{
public:
  moveToTxEvent(QString fn,int seg=-1):baseEvent( (QEvent::Type)moveToTx),fileName(fn),segment(seg)
  {
    description="moveToTxEvent";
  }
  QString getFilename() {return fileName;}
  int getSegment() {return segment;}
private:
  QString fileName;
  int segment;
};



class saveDRMImageEvent : public baseEvent
{
public:
  saveDRMImageEvent(QString fn,QString i):baseEvent( (QEvent::Type)saveDRMImage),fileName(fn),info(i)
  {
    description="saveDRMImageEvent";
  }
  void getFilename(QString &fn) {fn=fileName;}
  void getInfo(QString &i) {i=info;}
private:
  QString fileName;
  QString info;
};



/** partially received DRM image (live preview while the transmission is still in progress) */
class previewDRMImageEvent : public baseEvent
{
public:
  previewDRMImageEvent(QImage im):baseEvent( (QEvent::Type)previewDRMImage),image(im)
  {
    description="previewDRMImageEvent";
  }
  QImage getImage() {return image;}
private:
  QImage image;
};



class startImageRXEvent : public baseEvent
{
public:
  /** create event */
  startImageRXEvent(QSize ims):baseEvent( (QEvent::Type)startImageRX ),imSize(ims)
  {
    description="startImageRXEvent";
  }
  QSize getSize()  {return imSize;}
private:
  QSize imSize;

};

class endImageSSTVRXEvent : public baseEvent
{
public:
  /** create event */
  endImageSSTVRXEvent(esstvMode md):baseEvent( (QEvent::Type)endSSTVImageRX ),mode(md)
  {
    description="endImageSSTVRXEvent";
  }
  esstvMode getMode() {return mode;}
private:
  esstvMode mode;
};

class endImageTXEvent : public baseEvent
{
public:
  /** create event */
  endImageTXEvent(quint64 token=0):baseEvent( (QEvent::Type)endImageTX ),_kgstvToken(token)
  {
    description="endImageTXEvent";
  }
  quint64 kgstvToken() const {return _kgstvToken;}
private:
  quint64 _kgstvToken;
};







class progressTXEvent : public baseEvent
{
public:
  /** create event */
  progressTXEvent(double tim,quint64 token=0):baseEvent( (QEvent::Type)progressTX ), txTime(tim),_kgstvToken(token)
  {
    description="progressTXEvent";
  }
  /** returns length and pointer  from the event */
  double getInfo() { return txTime;}
  quint64 kgstvToken() const {return _kgstvToken;}

private:
  double txTime;
  quint64 _kgstvToken;
};



class callEditorEvent : public baseEvent
{
public:
  /** create event; an empty file name asks for a new template */
  callEditorEvent(const QString &fn):baseEvent( (QEvent::Type) callEditor ), filename(fn)
  {
    description="callEditorEvent";
  }
  /** the template to edit */
  QString getFilename() const { return filename; }
private:
  QString filename;
};


class templatesChangedEvent : public baseEvent
{
public:
  /** create event */
  templatesChangedEvent():baseEvent( (QEvent::Type) templatesChanged )
  {
    description="templateChangeEvent";
  }
};



class displayFFTEvent : public baseEvent
{
public:
  /** create event */
  displayFFTEvent(DSPFLOAT *buf):baseEvent( (QEvent::Type)displayFFT),buffer(buf)
  {
    description="displayFFTEvent";
  }
  DSPFLOAT *data() { return buffer;}
private:
  DSPFLOAT *buffer;

};


class prepareFixEvent: public baseEvent
{
public:
  prepareFixEvent(QByteArray ba):baseEvent( (QEvent::Type)prepareFix),data(ba)
  {
    description="filterChangedEvent";
  }
  QByteArray &getData() {return data;}
private:
  QByteArray data;
};


/**
  this event is send when the dspfunc thread stops running
*/
class displayTextEvent : public  baseEvent
{
public:
  /** create event */
  displayTextEvent(QString t):baseEvent( (QEvent::Type) displayText ), str(t)
  {
    description="displayTextEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};

class displayMBoxEvent : public  baseEvent
{
public:
  /** create event */
  displayMBoxEvent(QString title,QString text):baseEvent( (QEvent::Type) displayMBox ), str(text), title(title)
  {
    description="displayMBoxEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
  QString getTitle() const { return title; }

private:
  QString str;
  QString title;
};


class displayProgressFTPEvent : public  baseEvent
{
public:
  /** create event */
  displayProgressFTPEvent(quint64 byts,quint64 tot):baseEvent( (QEvent::Type) displayProgressFTP ),  bytes(byts),total(tot)
  {
    description="displayMBoxEvent";
  }
  /** returns info string from the event */
  quint64 getTotal() const { return total; }
  quint64 getBytes() const { return bytes; }

private:
  quint64 bytes;
  quint64 total;

};

class statusBarMsgEvent : public baseEvent
{
public:
  /** create event */
  statusBarMsgEvent(QString t):baseEvent( (QEvent::Type)statusBarMsg ), str(t)
  {
    description="statusBarMsgEvent";
  }
  /** returns info string from the event */
  QString getStr() const { return str; }
private:
  QString str;
};

#endif
