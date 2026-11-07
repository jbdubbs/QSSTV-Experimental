#ifndef DRMPROFILESCONTROLLER_H
#define DRMPROFILESCONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  The three saved DRM settings (Configuration > DRM on the desktop) for the Qt Quick pages: a name, and the six settings (robustness mode, QAM,
  bandwidth, interleaving, protection, Reed-Solomon) each. The values are the ones of drmPFArray, so the Transmit screen and the classic dialog see the
  same profiles; they are written to the settings file at once, as the classic page does.
*/
class DrmProfilesController : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(DrmProfiles)
  QML_SINGLETON
  Q_PROPERTY(QVariantList profiles READ profiles NOTIFY changed)       //!< [{index, name, line1, line2}]
  Q_PROPERTY(QString summary READ summary NOTIFY changed)              //!< "Robust, Balanced, Fast"
  Q_PROPERTY(bool factory READ factory NOTIFY changed)                 //!< all three are still the ones the program starts with
  Q_PROPERTY(bool showPartial READ showPartial WRITE setShowPartial NOTIFY changed)   //!< show a DRM picture while it is still arriving
public:
  static DrmProfilesController *instance();
  static DrmProfilesController *create(QQmlEngine *,QJSEngine *);
  QVariantList profiles() const;
  QString summary() const;
  bool factory() const;
  bool showPartial() const;
  void setShowPartial(bool on);

  Q_INVOKABLE QString name(int index) const;
  Q_INVOKABLE void setName(int index,const QString &name);
  //! the six settings of a profile: [{key, title, items, hint, index}]
  Q_INVOKABLE QVariantList options(int index) const;
  Q_INVOKABLE void setOption(int index,const QString &key,int choice);
  //! Robust, Balanced and Fast in place of the three profiles (only while they are the factory ones)
  Q_INVOKABLE void useTypical();
  //! the screenshots of the test harness: the three profiles as the program starts with them, or as typical ones
  Q_INVOKABLE void setTestProfiles(bool typical);

signals:
  void changed();

private:
  explicit DrmProfilesController(QObject *parent=nullptr) : QObject(parent) {}
  void commit(bool profilesChanged);
};

#endif // DRMPROFILESCONTROLLER_H
