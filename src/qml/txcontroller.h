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
  Q_PROPERTY(QString remainingText READ remainingText NOTIFY progressChanged)    //!< "0:42"
  Q_PROPERTY(bool useTemplate READ useTemplateOn WRITE setUseTemplate NOTIFY detailsChanged)
  Q_PROPERTY(QString toCall READ toCall WRITE setToCall NOTIFY detailsChanged)
  Q_PROPERTY(QString toOperator READ toOperator WRITE setToOperator NOTIFY detailsChanged)
  Q_PROPERTY(QString report READ report WRITE setReport NOTIFY detailsChanged)
  Q_PROPERTY(QString detailsSummary READ detailsSummary NOTIFY detailsChanged)   //!< "NT0Y · To W1AW · 59"

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
  bool useTemplateOn() const;
  void setUseTemplate(bool on);
  QString toCall() const;
  void setToCall(const QString &s);
  QString toOperator() const;
  void setToOperator(const QString &s);
  QString report() const;
  void setReport(const QString &s);
  QString detailsSummary() const;

  //! every analog mode: [{mode, name, size, time, group}] for the "All modes" sheet
  Q_INVOKABLE QVariantList allModes() const;
  Q_INVOKABLE void pickPhoto();     //!< the picture from the phone's photos
  Q_INVOKABLE void pickFile();      //!< the picture from a file
  Q_INVOKABLE void camera();        //!< a picture from the camera
  //! a picture from outside (shared to the program): load it and show the Transmit screen
  Q_INVOKABLE void showPicture(const QString &file);
  Q_INVOKABLE void send();
  Q_INVOKABLE void stop();

  //! the tests: pretend a transmission is under way at this progress (0 to 1), or none (negative)
  void setTestSending(qreal progress);

signals:
  void pictureChanged();
  void modeChanged();
  void timeChanged();
  void sendingChanged();
  void progressChanged();
  void detailsChanged();

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
  bool m_connected=false;
  bool m_test=false;
  QTimer m_timer;
};

#endif // TXCONTROLLER_H
