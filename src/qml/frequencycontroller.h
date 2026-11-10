#ifndef FREQUENCYCONTROLLER_H
#define FREQUENCYCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  The list of frequencies (Configuration > Frequencies on the desktop) and the use of it: the radio's frequency as a chip on the Receive and
  Transmit screens, and a picker that tunes the radio to an entry (what the frequency box in the desktop's status bar does). An entry is a frequency
  in MHz, for analog SSTV or for DRM, with the mode and the passband the radio is set to. The lists are the globals of the classic page
  (freqList, modeList, sbModeList, passBandList), written to the settings file at once.
*/
class FrequencyController : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(Frequencies)
  QML_SINGLETON
  Q_PROPERTY(QVariantList sstvEntries READ sstvEntries NOTIFY listChanged)     //!< [{index, freq, mod, pass, text}] for analog SSTV
  Q_PROPERTY(QVariantList drmEntries READ drmEntries NOTIFY listChanged)       //!< and for digital
  Q_PROPERTY(int count READ count NOTIFY listChanged)
  Q_PROPERTY(QString summary READ summary NOTIFY listChanged)                  //!< "5 saved: 14.230, 7.171 ..."
  Q_PROPERTY(QString command READ command WRITE setCommand NOTIFY commandChanged)   //!< a raw CAT command sent to the radio after tuning
  Q_PROPERTY(bool commandHex READ commandHex WRITE setCommandHex NOTIFY commandChanged)
  Q_PROPERTY(bool radioReady READ radioReady NOTIFY radioChanged)              //!< a radio is set up for CAT: the chip is shown
  Q_PROPERTY(QString chipText READ chipText NOTIFY radioChanged)               //!< "14.230 MHz USB"
  Q_PROPERTY(bool pickerOpen READ pickerOpen WRITE setPickerOpen NOTIFY pickerChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY listChanged)
public:
  static FrequencyController *instance();
  static FrequencyController *create(QQmlEngine *,QJSEngine *);

  QVariantList sstvEntries() const {return entries(false);}
  QVariantList drmEntries() const {return entries(true);}
  int count() const;
  QString summary() const;
  QString command() const;
  void setCommand(const QString &c);
  bool commandHex() const;
  void setCommandHex(bool on);
  bool radioReady() const;
  QString chipText() const {return m_chip;}
  bool pickerOpen() const {return m_pickerOpen;}
  void setPickerOpen(bool on);
  bool canUndo() const {return m_removedValid;}

  //! the entries for the mode the program is in (analog or digital), the one the radio is on marked: [{index, freq, text, current}]
  Q_INVOKABLE QVariantList pickerEntries() const;
  Q_INVOKABLE QVariantMap entry(int index) const;
  //! add an entry (frequency in MHz, a mode of the radio such as USB, a passband); returns its index, -1 if the frequency is not a number
  Q_INVOKABLE int add(const QString &freq,bool drm,const QString &mod,const QString &pass);
  Q_INVOKABLE bool update(int index,const QString &freq,bool drm,const QString &mod,const QString &pass);
  Q_INVOKABLE void remove(int index);
  Q_INVOKABLE void undoRemove();
  //! one place up or down among the entries for the same mode
  Q_INVOKABLE int move(int index,int delta);   //!< returns where the entry is now
  Q_INVOKABLE void addUsual();
  //! tune the radio to an entry: frequency, mode and passband, then the extra command
  Q_INVOKABLE void tune(int index);
  Q_INVOKABLE QStringList modulations() const;
  Q_INVOKABLE QStringList passbands() const;
  //! the screenshots of the test harness: a radio that is set up, and what it says
  Q_INVOKABLE void setTestRadio(bool ready,const QString &chip);
  Q_INVOKABLE void setTestList(bool filled);

signals:
  void listChanged();
  void commandChanged();
  void radioChanged();
  void pickerChanged();
  void removed(const QString &text);   //!< an entry was removed (for the snackbar with Undo)

private slots:
  void poll();

private:
  explicit FrequencyController(QObject *parent=nullptr);
  QVariantList entries(bool drm) const;
  static QString describe(int i);
  void commit();
  QString normalized(const QString &freq,bool *ok) const;

  QTimer m_timer;
  QString m_chip;
  bool m_pickerOpen=false;
  bool m_test=false;
  bool m_testReady=false;
  // the last entry that was removed, for Undo
  bool m_removedValid=false;
  int m_removedAt=0;
  QString m_rf,m_rm,m_rs,m_rp;
};

#endif // FREQUENCYCONTROLLER_H
