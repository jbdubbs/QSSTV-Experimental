#ifndef RXCONTROLLER_H
#define RXCONTROLLER_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include "waterfall.h"

class QQmlEngine;
class QJSEngine;

/*!
  The Receive screen's view of the receiver ("RxController" in QML). It reads what rxWidget and the dispatcher already hold (status text,
  the picture being drawn, the meters, the waterfall) and drives the same objects (start, stop, clear); the dispatcher tells it about
  pictures and lines through qmlUi::rx...(), the meters and the waterfall are looked at ten times a second while the app is in front.
*/
class RxController : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)          //!< the receiver is running
  Q_PROPERTY(bool receiving READ receiving NOTIFY stateChanged)          //!< a picture is coming in
  Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)    //!< the receiver's own status line
  Q_PROPERTY(QString modeText READ modeText NOTIFY statusChanged)        //!< "Martin 1" while a picture of that mode is coming in, else empty
  Q_PROPERTY(int line READ line NOTIFY progressChanged)                  //!< lines drawn
  Q_PROPERTY(int lines READ lines NOTIFY progressChanged)                //!< lines of the picture
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)        //!< 0 to 1
  Q_PROPERTY(qreal level READ level NOTIFY meterChanged)                 //!< input level, 0 to 1 on the meter's scale
  Q_PROPERTY(QString levelWord READ levelWord NOTIFY meterChanged)       //!< "Too quiet", "Good", "Too loud"
  Q_PROPERTY(qreal zoneStart READ zoneStart CONSTANT)                    //!< the good zone of the meter, 0 to 1
  Q_PROPERTY(qreal zoneEnd READ zoneEnd CONSTANT)
  Q_PROPERTY(qreal sync READ sync NOTIFY meterChanged)                   //!< sync quality, 0 to 1
  Q_PROPERTY(QString signalText READ signalText NOTIFY meterChanged)     //!< "Signal good" while a picture is coming in
  Q_PROPERTY(bool hasPicture READ hasPicture NOTIFY pictureChanged)      //!< there is a picture on the canvas (coming in or finished)
  Q_PROPERTY(int pictureRevision READ pictureRevision NOTIFY pictureChanged)
  Q_PROPERTY(int waterfallRevision READ waterfallRevision NOTIFY waterfallChanged)
  Q_PROPERTY(int fftLow READ fftLow CONSTANT)                              //!< the waterfall shows fftLow to fftHigh Hz
  Q_PROPERTY(int fftHigh READ fftHigh CONSTANT)
  Q_PROPERTY(bool autoSave READ autoSave WRITE setAutoSave NOTIFY optionsChanged)     //!< save a picture when it is complete
  Q_PROPERTY(bool autoSlant READ autoSlant WRITE setAutoSlant NOTIFY optionsChanged)  //!< straighten pictures from an off clock
  Q_PROPERTY(int sensitivity READ sensitivity WRITE setSensitivity NOTIFY optionsChanged)     //!< 0 low, 1 normal, 2 high, 3 DX: how readily a signal starts a picture
  Q_PROPERTY(int minCompletion READ minCompletion WRITE setMinCompletion NOTIFY optionsChanged)   //!< a picture is saved when this many percent of it came in (0 to 100)
  Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY optionsChanged)     //!< the analog mode the receiver is set to: -1 = Auto (any mode), else an esstvMode
  Q_PROPERTY(QString modeName READ modeName NOTIFY optionsChanged)      //!< "Auto" or "Martin 1": the name of the mode above

public:
  static RxController *instance();
  static RxController *create(QQmlEngine *,QJSEngine *);

  int sensitivity() const;
  void setSensitivity(int s);
  int minCompletion() const;
  void setMinCompletion(int percent);
  int mode() const;
  void setMode(int m);
  QString modeName() const;

  bool listening() const {return m_listening;}
  bool receiving() const {return m_receiving;}
  QString statusText() const {return m_status;}
  QString modeText() const {return m_mode;}
  int line() const {return m_line;}
  int lines() const {return m_lines;}
  qreal progress() const {return m_lines>0 ? qBound(0.0,qreal(m_line)/m_lines,1.0) : 0.0;}
  qreal level() const {return m_level;}
  QString levelWord() const;
  qreal zoneStart() const {return 0.35;}
  qreal zoneEnd() const {return 0.85;}
  qreal sync() const {return m_sync;}
  QString signalText() const;
  bool hasPicture() const {return !m_picture.isNull();}
  int pictureRevision() const {return m_pictureRevision;}
  int waterfallRevision() const {return m_waterfallRevision;}
  int fftLow() const {return Waterfall::lowHz;}
  int fftHigh() const {return Waterfall::highHz;}

  //! the pictures the ImageSurface items paint (GUI thread)
  const QImage &picture() const {return m_picture;}
  const QImage &waterfall() const {return m_waterfall.image();}

  bool autoSave() const;
  void setAutoSave(bool on);
  bool autoSlant() const;
  void setAutoSlant(bool on);

  //! the tests: use this input level (0 to 1) instead of the sound device's; a negative value goes back to the device
  void setTestLevel(qreal level) {m_testLevel=level; poll();}
  //! the tests: pretend the receiver runs (1) or does not (0), without starting it; -1 asks the receiver
  void setTestListening(int on) {m_testListening=on; poll();}

  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void toggle();
  Q_INVOKABLE void clearPicture();
  //! decode a recording of SSTV or DRM from a sound file (the system's file picker)
  Q_INVOKABLE void decodeRecording();

  // from the dispatcher through qmlUi (GUI thread)
  void imageStarted(int width,int height);
  void lineDrawn(int line);
  void imageEnded();
  void imageShown();
  void imageSaved(const QString &file);
  void statusReceived(bool drm,const QString &text);
  void syncReceived(int quality);
  void fftInit(int fftLength,int samplingRate);
  void fftReceived(const double *fft);

signals:
  void stateChanged();
  void statusChanged();
  void progressChanged();
  void meterChanged();
  void pictureChanged();
  void waterfallChanged();
  void optionsChanged();
  void pictureSaved(const QString &file);   //!< a received picture was saved to the gallery

private slots:
  void poll();

private:
  explicit RxController(QObject *parent=nullptr);
  void takePicture();
  bool m_listening=false;
  bool m_receiving=false;
  qreal m_testLevel=-1;
  int m_testListening=-1;
  QString m_status;
  QString m_mode;
  int m_line=0;
  int m_lines=0;
  qreal m_level=0;
  qreal m_sync=0;
  QImage m_picture;
  Waterfall m_waterfall;
  int m_pictureRevision=0;
  int m_waterfallRevision=0;
  QTimer m_timer;
};

#endif // RXCONTROLLER_H
