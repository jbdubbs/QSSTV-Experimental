#ifndef GALLERYMODEL_H
#define GALLERYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*!
  The pictures of the gallery for the Qt Quick screens ("GalleryModel" in QML). It lists the files of the gallery folders itself
  (rx_sstv, rx_drm, tx_sstv, tx_drm, tx_stock, templates), newest first, and offers them as rows for a ListView: a heading ("Today",
  "Yesterday", a date) or a row of as many thumbnails as fit the width (set columns). That keeps the list light however many pictures
  there are. Selecting, deleting with a chance to take it back, sending and sharing live here too.
*/
class GalleryModel : public QAbstractListModel
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(int tab READ tab WRITE setTab NOTIFY tabChanged)                       //!< 0 Received, 1 Sent, 2 Templates and pictures
  Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY columnsChanged)       //!< thumbnails in a row
  Q_PROPERTY(int count READ count NOTIFY itemsChanged)                              //!< pictures in this tab
  Q_PROPERTY(bool selecting READ selecting NOTIFY selectionChanged)
  Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
  Q_PROPERTY(int revision READ revision NOTIFY itemsChanged)                        //!< changes whenever the list does
  Q_PROPERTY(int viewerIndex READ viewerIndex WRITE setViewerIndex NOTIFY viewerChanged)   //!< the picture shown full screen, or -1

public:
  enum Role {KindRole=Qt::UserRole+1,TitleRole,CellsRole};
  static GalleryModel *instance();
  static GalleryModel *create(QQmlEngine *,QJSEngine *);

  int rowCount(const QModelIndex &parent=QModelIndex()) const override;
  QVariant data(const QModelIndex &index,int role) const override;
  QHash<int,QByteArray> roleNames() const override;

  int tab() const {return m_tab;}
  void setTab(int t);
  int columns() const {return m_columns;}
  void setColumns(int c);
  int count() const {return m_items.count();}
  bool selecting() const {return m_selecting;}
  int selectedCount() const;
  int revision() const {return m_revision;}
  int viewerIndex() const {return m_viewer;}
  void setViewerIndex(int i);

  //! every picture of this tab, for the viewer: [{path, name, badge, thumb, full, when, template}]
  Q_INVOKABLE QVariantList items() const;
  //! the details of one picture, for the Info sheet: [{label, value}]
  Q_INVOKABLE QVariantList info(int index) const;
  Q_INVOKABLE void tap(int index);                  //!< a thumbnail was tapped: selects while selecting, else opens the viewer
  Q_INVOKABLE void startSelecting(int index);       //!< a thumbnail was held down
  Q_INVOKABLE void toggle(int index);
  Q_INVOKABLE void selectAll();
  Q_INVOKABLE void clearSelection();
  Q_INVOKABLE void closeViewer() {setViewerIndex(-1);}
  Q_INVOKABLE void openFile(const QString &path);   //!< shows this picture (it is in the Received tab), e.g. the one just received
  //! the selected pictures leave the list at once, and are deleted for good after a while unless undoDelete() is called
  Q_INVOKABLE int deleteSelected();
  Q_INVOKABLE int deleteIndex(int index);
  Q_INVOKABLE void undoDelete();
  Q_INVOKABLE void commitDelete();
  Q_INVOKABLE void sendSelected();                  //!< the selected pictures are what is sent next
  Q_INVOKABLE void sendIndex(int index);
  Q_INVOKABLE void shareSelected();
  Q_INVOKABLE void shareIndex(int index);
  Q_INVOKABLE void rescan();

signals:
  void tabChanged();
  void columnsChanged();
  void itemsChanged();
  void selectionChanged();
  void viewerChanged();
  void deleted(int count);                          //!< pictures were taken out of the list: say so, with Undo

private:
  explicit GalleryModel(QObject *parent=nullptr);
  struct Item
  {
    QString path;
    QString name;
    QString badge;
    QDateTime when;
    QString group;       //!< the heading it is listed under
    bool isTemplate=false;
    bool selected=false;
    bool hidden=false;   //!< deleted, waiting for the time to undo it to pass
  };
  struct Row
  {
    bool header=false;
    QString title;
    int first=0;         //!< index of the first picture in m_items
    int count=0;
  };
  void scan();
  void buildRows();
  QVariantMap cell(int itemIndex) const;
  QStringList folders() const;
  void watch();
  QStringList selectedPaths() const;
  QString dayLabel(const QDateTime &when) const;

  int m_tab=0;
  int m_columns=3;
  bool m_selecting=false;
  int m_viewer=-1;
  int m_revision=0;
  QVector<Item> m_items;       //!< the visible ones
  QVector<Item> m_pending;     //!< deleted, not yet gone from the disk
  QVector<Row> m_rows;
  QFileSystemWatcher m_watcher;
  QTimer m_rescan;
  QTimer m_commit;
};

#endif // GALLERYMODEL_H
