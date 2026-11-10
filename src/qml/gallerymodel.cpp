#include "gallerymodel.h"
#include "appmodel.h"
#include "directoriesconfig.h"
#include "platformfiles.h"
#include "supportfunctions.h"
#include "thumbnailprovider.h"
#include "txcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJSEngine>
#include <QLocale>
#include <QQmlEngine>
#include <QRegularExpression>
#include <algorithm>

GalleryModel *GalleryModel::instance()
{
  static GalleryModel *m=new GalleryModel;
  return m;
}

GalleryModel *GalleryModel::create(QQmlEngine *,QJSEngine *)
{
  GalleryModel *m=instance();
  QJSEngine::setObjectOwnership(m,QJSEngine::CppOwnership);
  return m;
}

GalleryModel::GalleryModel(QObject *parent) : QAbstractListModel(parent)
{
  m_rescan.setSingleShot(true);
  m_rescan.setInterval(400);
  connect(&m_rescan,&QTimer::timeout,this,&GalleryModel::rescan);
  m_commit.setSingleShot(true);
  m_commit.setInterval(6000);
  connect(&m_commit,&QTimer::timeout,this,&GalleryModel::commitDelete);
  connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_rescan.start();});
  rescan();
}

QHash<int,QByteArray> GalleryModel::roleNames() const
{
  return {{KindRole,"kind"},{TitleRole,"title"},{CellsRole,"cells"}};
}

int GalleryModel::rowCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : m_rows.count();
}

QVariantMap GalleryModel::cell(int i) const
{
  const Item &it=m_items.at(i);
  QVariantMap c;
  c["index"]=i;
  c["path"]=it.path;
  c["name"]=it.name;
  c["badge"]=it.badge;
  c["selected"]=it.selected;
  c["isTemplate"]=it.isTemplate;
  c["thumb"]=QStringLiteral("image://thumb/")+ThumbnailProvider::idFor(it.path);
  return c;
}

QVariant GalleryModel::data(const QModelIndex &index,int role) const
{
  if(!index.isValid() || index.row()<0 || index.row()>=m_rows.count()) return QVariant();
  const Row &r=m_rows.at(index.row());
  switch(role)
    {
    case KindRole: return r.header ? 0 : 1;
    case TitleRole: return r.title;
    case CellsRole:
      {
        QVariantList cells;
        for(int i=0;i<r.count;i++) cells.append(cell(r.first+i));
        return cells;
      }
    }
  return QVariant();
}

QStringList GalleryModel::folders() const
{
  switch(m_tab)
    {
    case 0: return {rxSSTVImagesPath,rxDRMImagesPath};
    case 1: return {txSSTVImagesPath,txDRMImagesPath};
    default: return {templatesPath,txStockImagesPath};
    }
}

void GalleryModel::watch()
{
  const QStringList old=m_watcher.directories();
  if(!old.isEmpty()) m_watcher.removePaths(old);
  for(const QString &f:folders()) if(QFileInfo(f).isDir()) m_watcher.addPath(f);
}

QString GalleryModel::dayLabel(const QDateTime &when) const
{
  const QDate today=QDate::currentDate();
  const QDate d=when.toLocalTime().date();
  const int days=d.daysTo(today);
  if(days==0) return tr("Today");
  if(days==1) return tr("Yesterday");
  if(days>1 && days<7) return QLocale().dayName(d.dayOfWeek());
  return QLocale().toString(d,QLocale::LongFormat);
}

void GalleryModel::scan()
{
  static const QStringList pictureSuffixes={"png","jpg","jpeg","webp","bmp","gif","jp2","avif","heic","heif","tif","tiff"};
  static const QStringList templateSuffixes={"qtempl","templ"};
  static const QRegularExpression stamped("^([A-Za-z0-9]+)_\\d{8}_\\d{6}\\.");
  QSet<QString> hiddenPaths;
  for(const Item &p:m_pending) hiddenPaths.insert(p.path);
  QVector<Item> found;
  const QStringList dirs=folders();
  for(int d=0;d<dirs.size();d++)
    {
      const QDir dir(dirs.at(d));
      if(!dir.exists()) continue;
      for(const QFileInfo &fi:dir.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::Readable))
        {
          if(hiddenPaths.contains(fi.absoluteFilePath())) continue;
          const QString suffix=fi.suffix().toLower();
          const bool isTemplate=(m_tab==2 && d==0 && templateSuffixes.contains(suffix));
          if(!isTemplate && !pictureSuffixes.contains(suffix)) continue;
          Item it;
          it.path=fi.absoluteFilePath();
          it.name=fi.completeBaseName();
          it.when=fi.lastModified();
          it.isTemplate=isTemplate;
          if(m_tab==0)
            {
              const QRegularExpressionMatch m=stamped.match(fi.fileName());
              it.badge=(d==1) ? QStringLiteral("DRM") : (m.hasMatch() ? m.captured(1) : QStringLiteral("SSTV"));
            }
          else if(m_tab==1) it.badge=(d==1) ? QStringLiteral("DRM") : QStringLiteral("SSTV");
          if(m_tab==2) it.group=isTemplate ? tr("Templates") : tr("Pictures");
          else it.group=dayLabel(it.when);
          found.append(it);
        }
    }
  if(m_tab==2)
    {
      // templates first, then the pictures; each by name
      std::sort(found.begin(),found.end(),[](const Item &a,const Item &b)
        {
          if(a.isTemplate!=b.isTemplate) return a.isTemplate;
          return QString::compare(a.name,b.name,Qt::CaseInsensitive)<0;
        });
    }
  else
    {
      std::sort(found.begin(),found.end(),[](const Item &a,const Item &b){return a.when>b.when;});
    }
  m_items=found;
}

void GalleryModel::buildRows()
{
  m_rows.clear();
  QString group;
  int i=0;
  while(i<m_items.size())
    {
      if(m_items.at(i).group!=group)
        {
          group=m_items.at(i).group;
          Row h;
          h.header=true;
          h.title=group;
          m_rows.append(h);
        }
      int n=0;
      while(i+n<m_items.size() && n<m_columns && m_items.at(i+n).group==group) n++;
      Row r;
      r.first=i;
      r.count=n;
      m_rows.append(r);
      i+=n;
    }
}

void GalleryModel::rescan()
{
  beginResetModel();
  const QStringList selectedBefore=selectedPaths();
  scan();
  for(Item &it:m_items) it.selected=selectedBefore.contains(it.path);
  buildRows();
  if(m_viewer>=m_items.size()) m_viewer=m_items.isEmpty() ? -1 : m_items.size()-1;
  ++m_revision;
  endResetModel();
  watch();
  if(selectedPaths().isEmpty() && m_selecting) {m_selecting=false;}
  emit itemsChanged();
  emit selectionChanged();
  emit viewerChanged();
}

void GalleryModel::setTab(int t)
{
  if(t==m_tab || t<0 || t>2) return;
  commitDelete();
  m_tab=t;
  m_selecting=false;
  m_viewer=-1;
  emit tabChanged();
  rescan();
}

void GalleryModel::setColumns(int c)
{
  c=qBound(2,c,12);
  if(c==m_columns) return;
  m_columns=c;
  beginResetModel();
  buildRows();
  endResetModel();
  emit columnsChanged();
}

int GalleryModel::selectedCount() const
{
  int n=0;
  for(const Item &it:m_items) if(it.selected) n++;
  return n;
}

QStringList GalleryModel::selectedPaths() const
{
  QStringList l;
  for(const Item &it:m_items) if(it.selected) l<<it.path;
  return l;
}

void GalleryModel::setViewerIndex(int i)
{
  if(i<-1 || i>=m_items.size()) i=-1;
  if(i==m_viewer) return;
  m_viewer=i;
  emit viewerChanged();
}

QVariantList GalleryModel::items() const
{
  QVariantList l;
  for(const Item &it:m_items)
    {
      QVariantMap m;
      m["path"]=it.path;
      m["name"]=it.name;
      m["badge"]=it.badge;
      m["isTemplate"]=it.isTemplate;
      m["when"]=QLocale().toString(it.when.toLocalTime(),QLocale::ShortFormat);
      m["full"]=QStringLiteral("image://full/")+ThumbnailProvider::idFor(it.path);
      l.append(m);
    }
  return l;
}

QVariantList GalleryModel::info(int index) const
{
  QVariantList l;
  if(index<0 || index>=m_items.size()) return l;
  const Item &it=m_items.at(index);
  auto add=[&l](const QString &label,const QString &value){QVariantMap m; m["label"]=label; m["value"]=value; l.append(m);};
  const QFileInfo fi(it.path);
  add(tr("Name"),fi.fileName());
  add(it.isTemplate ? tr("Changed") : (m_tab==0 ? tr("Received") : m_tab==1 ? tr("Sent") : tr("Added")),QLocale().toString(it.when.toLocalTime().date(),QLocale::LongFormat)+QStringLiteral(", ")+QLocale().toString(it.when.toLocalTime().time(),QLocale::ShortFormat));
  if(!it.badge.isEmpty()) add(tr("Mode"),it.badge);
  if(!it.isTemplate)
    {
      QSize s=QImageReader(it.path).size();
      if(!s.isValid())
        {
          QImage im;
          if(loadImageAutoRotate(im,it.path)) s=im.size();   // AVIF without the Qt plugin (Android)
        }
      if(s.isValid()) add(tr("Picture"),tr("%1 x %2 pixels").arg(s.width()).arg(s.height()));
    }
  add(tr("File size"),QLocale().formattedDataSize(fi.size()));
  add(tr("Folder"),fi.absolutePath());
  return l;
}

void GalleryModel::tap(int index)
{
  if(m_selecting) toggle(index);
  else setViewerIndex(index);
}

void GalleryModel::startSelecting(int index)
{
  if(index<0 || index>=m_items.size()) return;
  m_selecting=true;
  m_items[index].selected=true;
  emit dataChanged(this->index(0),this->index(qMax(0,m_rows.count()-1)),{CellsRole});
  emit selectionChanged();
}

void GalleryModel::toggle(int index)
{
  if(index<0 || index>=m_items.size()) return;
  m_items[index].selected=!m_items[index].selected;
  if(selectedCount()==0) m_selecting=false;
  emit dataChanged(this->index(0),this->index(qMax(0,m_rows.count()-1)),{CellsRole});
  emit selectionChanged();
}

void GalleryModel::selectAll()
{
  for(Item &it:m_items) it.selected=true;
  m_selecting=!m_items.isEmpty();
  emit dataChanged(this->index(0),this->index(qMax(0,m_rows.count()-1)),{CellsRole});
  emit selectionChanged();
}

void GalleryModel::clearSelection()
{
  for(Item &it:m_items) it.selected=false;
  m_selecting=false;
  emit dataChanged(this->index(0),this->index(qMax(0,m_rows.count()-1)),{CellsRole});
  emit selectionChanged();
}

void GalleryModel::openFile(const QString &path)
{
  if(m_tab!=0) setTab(0);
  else rescan();
  for(int i=0;i<m_items.size();i++)
    if(m_items.at(i).path==QFileInfo(path).absoluteFilePath()) {setViewerIndex(i); return;}
}

int GalleryModel::deleteSelected()
{
  commitDelete();   // an earlier deletion is final once another one is made
  QVector<Item> keep;
  int n=0;
  for(const Item &it:m_items)
    {
      if(it.selected) {m_pending.append(it); n++;}
      else keep.append(it);
    }
  if(n==0) return 0;
  beginResetModel();
  m_items=keep;
  m_selecting=false;
  buildRows();
  ++m_revision;
  endResetModel();
  m_commit.start();
  emit itemsChanged();
  emit selectionChanged();
  emit deleted(n);
  return n;
}

int GalleryModel::deleteIndex(int index)
{
  if(index<0 || index>=m_items.size()) return 0;
  for(Item &it:m_items) it.selected=false;
  m_items[index].selected=true;
  const int viewer=m_viewer;
  const int n=deleteSelected();
  // the viewer goes on with the picture that took its place (or the one before it when it was the last)
  if(viewer>=0) setViewerIndex(m_items.isEmpty() ? -1 : qMin(viewer,m_items.size()-1));
  return n;
}

void GalleryModel::undoDelete()
{
  if(m_pending.isEmpty()) return;
  m_commit.stop();
  m_pending.clear();
  rescan();   // the files are all still there
}

void GalleryModel::commitDelete()
{
  m_commit.stop();
  if(m_pending.isEmpty()) return;
  for(const Item &it:m_pending) trash(it.path,true);
  m_pending.clear();
}

void GalleryModel::sendSelected()
{
  const QStringList paths=selectedPaths();
  if(paths.isEmpty()) return;
  for(const Item &it:m_items)
    if(it.selected && it.isTemplate) {TxController::instance()->useTemplateFile(it.path); clearSelection(); return;}
  if(paths.count()==1) TxController::instance()->showPicture(paths.first());
  else TxController::instance()->showPictures(paths);
  clearSelection();
}

void GalleryModel::sendIndex(int index)
{
  if(index<0 || index>=m_items.size()) return;
  const Item &it=m_items.at(index);
  m_viewer=-1;
  emit viewerChanged();
  if(it.isTemplate) TxController::instance()->useTemplateFile(it.path);
  else TxController::instance()->showPicture(it.path);
}

void GalleryModel::shareSelected()
{
  const QStringList paths=selectedPaths();
  if(paths.count()==1) platformFiles::shareImage(paths.first());
}

void GalleryModel::shareIndex(int index)
{
  if(index>=0 && index<m_items.size()) platformFiles::shareImage(m_items.at(index).path);
}
