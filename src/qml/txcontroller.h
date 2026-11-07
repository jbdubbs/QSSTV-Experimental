#ifndef TXCONTROLLER_H
#define TXCONTROLLER_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  The Transmit screen's view of the transmitter ("TxController" in QML): the picture that will be sent, the mode and what it takes,
  the details that go on the picture, the progress of a transmission. It reads and drives txWidget and its image viewer (kept built
  and never shown), the way RxController does for the receiver.
*/
class TxController : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool hasPicture READ hasPicture NOTIFY pictureChanged)
  Q_PROPERTY(int pictureRevision READ pictureRevision NOTIFY pictureChanged)
  Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)                //!< the analog mode (esstvMode)
  Q_PROPERTY(QString modeName READ modeName NOTIFY modeChanged)
  Q_PROPERTY(QString pictureSize READ pictureSize NOTIFY modeChanged)            //!< "320 x 256"
  Q_PROPERTY(QVariantList favorites READ favorites CONSTANT)                     //!< [{mode, name}] shown as chips
  Q_PROPERTY(QString timeText READ timeText NOTIFY timeChanged)                  //!< "1 min 54 s", or empty without a picture
  Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)                //!< 0 to 1
  Q_PROPERTY(QString remainingText READ remainingText NOTIFY progressChanged)    //!< "0:42", empty when the time is not known
  Q_PROPERTY(QString totalText READ totalText NOTIFY progressChanged)            //!< what the whole transmission takes
  Q_PROPERTY(QString toolName READ toolName NOTIFY toolChanged)                  //!< while a tone, text or CW is sent: what it is; empty for a picture
  Q_PROPERTY(bool useTemplate READ useTemplateOn WRITE setUseTemplate NOTIFY detailsChanged)
  Q_PROPERTY(QString templateName READ templateName NOTIFY detailsChanged)   //!< the template that goes on the picture; empty when there is none
  Q_PROPERTY(QString templateFile READ templateFile NOTIFY detailsChanged)   //!< its file
  Q_PROPERTY(QString toCall READ toCall WRITE setToCall NOTIFY detailsChanged)
  Q_PROPERTY(QString toOperator READ toOperator WRITE setToOperator NOTIFY detailsChanged)
  Q_PROPERTY(QString report READ report WRITE setReport NOTIFY detailsChanged)
  Q_PROPERTY(QString detailsSummary READ detailsSummary NOTIFY detailsChanged)   //!< "NT0Y · To W1AW · 59"
  Q_PROPERTY(bool cameraOpen READ cameraOpen WRITE setCameraOpen NOTIFY cameraOpenChanged)   //!< the camera screen is showing
  Q_PROPERTY(bool cameraAvailable READ cameraAvailable CONSTANT)                              //!< the device has a camera
  // digital (DRM)
  Q_PROPERTY(QStringList drmProfiles READ drmProfiles NOTIFY drmChanged)         //!< the saved DRM settings (Configuration > DRM), three at most
  Q_PROPERTY(int drmProfile READ drmProfile WRITE setDrmProfile NOTIFY drmChanged)
  Q_PROPERTY(int drmSize READ drmSize WRITE setDrmSize NOTIFY drmChanged)        //!< the compressed picture, in bytes
  Q_PROPERTY(int drmSizeMin READ drmSizeMin NOTIFY drmChanged)
  Q_PROPERTY(int drmSizeMax READ drmSizeMax NOTIFY drmChanged)
  Q_PROPERTY(QString drmSummary READ drmSummary NOTIFY drmChanged)               //!< "Mode B, 16-QAM, 2.3 kHz"
  Q_PROPERTY(int drmRevision READ drmRevision NOTIFY drmChanged)                 //!< changes whenever something of the digital settings does

public:
  static TxController *instance();
  static TxController *create(QQmlEngine *,QJSEngine *);

  bool hasPicture() const {return !m_picture.isNull();}
  int pictureRevision() const {return m_pictureRevision;}
  const QImage &picture() const {return m_picture;}
  int mode() const;
  void setMode(int mode);
  QString modeName() const;
  QString pictureSize() const;
  QVariantList favorites() const;
  QString timeText() const;
  bool sending() const {return m_sending;}
  qreal progress() const {return m_progress;}
  QString remainingText() const;
  QString totalText() const;
  QString toolName() const {return m_toolName;}
  bool useTemplateOn() const;
  QString templateName() const;
  QString templateFile() const;
  void setUseTemplate(bool on);
  QString toCall() const;
  void setToCall(const QString &s);
  QString toOperator() const;
  void setToOperator(const QString &s);
  QString report() const;
  void setReport(const QString &s);
  QString detailsSummary() const;

  bool cameraOpen() const {return m_cameraOpen;}
  void setCameraOpen(bool open);
  bool cameraAvailable() const;
  QStringList drmProfiles() const;
  int drmProfile() const;
  void setDrmProfile(int index);
  int drmSize() const;
  void setDrmSize(int bytes);
  int drmSizeMin() const;
  int drmSizeMax() const;
  QString drmSummary() const;
  int drmRevision() const {return m_drmRevision;}
  //! the choices of the digital settings: [{key, title, items:[text], index}] read from the page's own boxes
  Q_INVOKABLE QVariantList drmOptions() const;
  Q_INVOKABLE void setDrmOption(const QString &key,int index);

  //! every analog mode: [{mode, name, size, time, group}] for the "All modes" sheet
  Q_INVOKABLE QVariantList allModes() const;
  Q_INVOKABLE void pickPhoto();     //!< the picture from the phone's photos
  Q_INVOKABLE void pickFile();      //!< the picture from a file
  // the tools of the Transmit menu (the Widgets versions are dialogs, which cannot be shown over this interface)
  Q_INVOKABLE void testPattern(int which);                 //!< 0 SMPTE, 1 PM5544, 2 Indian head: the pattern becomes the picture to send
  Q_INVOKABLE void sendWaterfallId();                      //!< the call sign as text in the waterfall
  Q_INVOKABLE void sendWaterfallText(const QString &text);
  Q_INVOKABLE void sendCwId();
  Q_INVOKABLE void sendTone(int frequency,int seconds);
  Q_INVOKABLE void sendSweep(int lowerFrequency,int upperFrequency,int seconds);
  //! the values the tone and sweep sheets start from (the ones of the last time): {tone, duration, low, high, sweepDuration}
  Q_INVOKABLE QVariantMap toolDefaults() const;
  Q_INVOKABLE void camera();        //!< a picture from the camera: asks for the permission first, then the camera screen shows
  //! where the camera screen saves the picture it takes (the program's cache)
  Q_INVOKABLE QString tempPicturePath() const;
  //! the camera took this picture: it is what is sent next
  Q_INVOKABLE void usePicture(const QString &file);
  //! a picture from outside (shared to the program): load it and show the Transmit screen
  Q_INVOKABLE void showPicture(const QString &file);
  //! several pictures from the gallery: they fill the grid of the picture to send
  Q_INVOKABLE void showPictures(const QStringList &files);
  //! a template from the gallery: it is the one put on the picture, and the Transmit screen shows
  Q_INVOKABLE void useTemplateFile(const QString &file);
  //! the templates there are, [{name, path, thumb, current}], and the choice of one (the Transmit screen stays)
  Q_INVOKABLE QVariantList templates() const;
  Q_INVOKABLE void chooseTemplate(const QString &file);
  Q_INVOKABLE void send();
  Q_INVOKABLE void stop();

  //! the tests: pretend a transmission is under way at this progress (0 to 1), or none (negative)
  void setTestSending(qreal progress,const QString &tool=QString(),int toolSeconds=0);

signals:
  void pictureChanged();
  void modeChanged();
  void timeChanged();
  void sendingChanged();
  void progressChanged();
  void detailsChanged();
  void drmChanged();
  void cameraOpenChanged();
  void toolChanged();
  void cameraBlocked();   //!< the user refused the camera: the screen says where to allow it

private slots:
  void refreshPicture();
  void poll();
  void progressTick(int percent);

private:
  explicit TxController(QObject *parent=nullptr);
  void connectToPage();
  QImage m_picture;
  int m_pictureRevision=0;
  bool m_sending=false;
  qreal m_progress=0;
  int m_cachedMode=-1;
  int m_drmRevision=0;
  bool m_cameraOpen=false;
  QString m_toolName;
  int m_toolSeconds=0;
  void startTool(const QString &name,int seconds);
  QString m_drmSignature;
  bool m_connected=false;
  bool m_test=false;
  QTimer m_timer;
};

#endif // TXCONTROLLER_H
