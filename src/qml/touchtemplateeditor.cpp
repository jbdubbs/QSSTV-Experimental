#include "touchtemplateeditor.h"
#include <QCoreApplication>
#include "appglobal.h"
#include "directoriesconfig.h"
#include "templatepresets.h"
#include "templatevars.h"
#include "txcontroller.h"
#include "txwidget.h"
#include "gallerywidget.h"
#include "imageviewer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJSEngine>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QtMath>

static TouchTemplateEditor *theEditor=nullptr;

TouchTemplateEditor::TouchTemplateEditor(QObject *parent):QObject(parent)
{
  connect(&m_doc,&EditorDocument::reset,this,[this]{m_selected.clear(); bump(); refreshSelection();});
  connect(&m_doc,&EditorDocument::itemInserted,this,[this]{bump();});
  connect(&m_doc,&EditorDocument::itemRemoved,this,[this](const QString &id){if(id==m_selected) m_selected.clear(); bump(); refreshSelection();});
  connect(&m_doc,&EditorDocument::itemChanged,this,[this](const QString &id){bump(); if(id==m_selected) refreshSelection();});
  connect(&m_doc,&EditorDocument::orderChanged,this,[this]{bump();});
  connect(&m_doc,&EditorDocument::canvasChanged,this,[this]{bump(); refreshSelection();});
  connect(&m_doc,&EditorDocument::fileChanged,this,&TouchTemplateEditor::fileChanged);
  connect(&m_doc,&EditorDocument::selectionHint,this,[this](const QStringList &ids)   // after an undo or a redo
    {
      m_selected=ids.isEmpty() ? QString() : ids.first();
      refreshSelection();
    });
  connect(m_doc.undoStack(),&QUndoStack::canUndoChanged,this,&TouchTemplateEditor::undoChanged);
  connect(m_doc.undoStack(),&QUndoStack::canRedoChanged,this,&TouchTemplateEditor::undoChanged);
  connect(m_doc.undoStack(),&QUndoStack::cleanChanged,this,&TouchTemplateEditor::dirtyChanged);
}

TouchTemplateEditor *TouchTemplateEditor::instance()
{
  if(!theEditor) theEditor=new TouchTemplateEditor;
  return theEditor;
}

TouchTemplateEditor *TouchTemplateEditor::create(QQmlEngine *,QJSEngine *)
{
  QJSEngine::setObjectOwnership(instance(),QJSEngine::CppOwnership);
  return instance();
}

void TouchTemplateEditor::bump()
{
  m_revision++;
  emit changed();
}

QString TouchTemplateEditor::name() const
{
  const QString f=m_doc.fileName();
  return f.isEmpty() ? QString() : QFileInfo(f).completeBaseName();
}

tpl::RenderOptions TouchTemplateEditor::renderOptions() const
{
  tpl::RenderOptions o;
  if(m_sample) o.vars=tpl::sampleVars();
  o.expandMacros=m_sample;
  return o;
}

void TouchTemplateEditor::setSampleData(bool on)
{
  if(m_sample==on) return;
  m_sample=on;
  bump();
  refreshSelection();
}

tpl::Element *TouchTemplateEditor::selectedElement()
{
  return const_cast<tpl::Element *>(m_doc.find(m_selected));
}

const tpl::Element *TouchTemplateEditor::selectedElementConst() const
{
  return m_selected.isEmpty() ? nullptr : m_doc.find(m_selected);
}

QPolygonF TouchTemplateEditor::outlineOf(const tpl::Element &e) const
{
  return tpl::sceneOutline(e,renderOptions());
}

QString TouchTemplateEditor::kindName(tpl::ItemType t) const
{
  switch(t)
    {
    case tpl::ItemType::Text: return QStringLiteral("text");
    case tpl::ItemType::Rect: return QStringLiteral("rect");
    case tpl::ItemType::Ellipse: return QStringLiteral("ellipse");
    case tpl::ItemType::Line: return QStringLiteral("line");
    case tpl::ItemType::Image: return QStringLiteral("image");
    case tpl::ItemType::Replay: return QStringLiteral("replay");
    }
  return QString();
}

QString TouchTemplateEditor::layerLabel(const tpl::Element &e) const
{
  switch(e.type)
    {
    case tpl::ItemType::Text: return e.text.isEmpty() ? tr("Text") : e.text.left(40);
    case tpl::ItemType::Rect: return tr("Rectangle");
    case tpl::ItemType::Ellipse: return tr("Ellipse");
    case tpl::ItemType::Line: return tr("Line");
    case tpl::ItemType::Image: return tr("Picture");
    case tpl::ItemType::Replay: return tr("Replay window");
    }
  return QString();
}

void TouchTemplateEditor::setOpen(bool on)
{
  if(m_open==on) return;
  m_open=on;
  emit openChanged();
}

QString TouchTemplateEditor::pathFor(const QString &name) const
{
  QString n=name.trimmed();
  n.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),QStringLiteral("_"));
  if(n.isEmpty() || n.startsWith('.')) return QString();
  return QDir(templatesPath).filePath(n+QLatin1Char('.')+QLatin1String(tpl::nativeExtension));
}

// ---- opening and closing ----

bool TouchTemplateEditor::openFile(const QString &path)
{
  const tpl::LoadResult r=tpl::loadFile(path);
  if(!r.ok)
    {
      m_error=r.error;
      return false;
    }
  m_doc.load(r.doc,path);
  m_doc.setFile(path,r.doc.fromLegacy);
  m_selected.clear();
  setOpen(true);
  emit fileChanged();
  emit dirtyChanged();
  emit undoChanged();
  refreshSelection();
  return true;
}

QStringList TouchTemplateEditor::starterNames() const {return templatePresets::starterNames();}

void TouchTemplateEditor::newTemplate(int starter,int width,int height)
{
  m_doc.load(templatePresets::makeStarter(starter,QSize(width,height)),QString());
  m_selected.clear();
  setOpen(true);
  emit fileChanged();
  emit dirtyChanged();
  emit undoChanged();
  refreshSelection();
}

bool TouchTemplateEditor::save(const QString &name)
{
  QString path=pathFor(name);
  if(path.isEmpty())
    {
      m_error=tr("The name is empty.");
      return false;
    }
  QDir().mkpath(templatesPath);
  QString err;
  if(!tpl::saveFile(path,m_doc.doc(),&err))
    {
      m_error=err.isEmpty() ? tr("The template could not be saved.") : err;
      return false;
    }
  m_doc.setFile(path,false);
  m_doc.markSaved();
  emit fileChanged();
  emit dirtyChanged();
  // as dispatcher::slotTemplateSaved: the gallery and the template list of the Transmit page (and its picture) follow
  if(galleryWidgetPtr) galleryWidgetPtr->changedMatrix(imageViewer::TEMPLATETHUMB);
  if(txWidgetPtr) txWidgetPtr->setupTemplatesComboBox();
  emit TxController::instance()->detailsChanged();
  return true;
}

void TouchTemplateEditor::close()
{
  m_selected.clear();
  m_guides.clear();
  m_gesture=false;
  setOpen(false);
  refreshSelection();
}

bool TouchTemplateEditor::duplicateFile(const QString &path,const QString &newName)
{
  const tpl::LoadResult r=tpl::loadFile(path);
  if(!r.ok) {m_error=r.error; return false;}
  const QString target=pathFor(newName);
  if(target.isEmpty()) {m_error=tr("The name is empty."); return false;}
  QString err;
  if(!tpl::saveFile(target,r.doc,&err)) {m_error=err; return false;}
  return true;
}

// ---- selecting ----

QString TouchTemplateEditor::itemAt(qreal x,qreal y,qreal tolerance) const
{
  const QVector<tpl::Element> &items=m_doc.doc().items;
  for(int i=items.size()-1;i>=0;i--)   // the one in front first
    {
      const QPolygonF poly=outlineOf(items.at(i));
      if(poly.containsPoint(QPointF(x,y),Qt::WindingFill)) return items.at(i).id;
      const QRectF b=poly.boundingRect().adjusted(-tolerance,-tolerance,tolerance,tolerance);
      if(items.at(i).type==tpl::ItemType::Line && b.contains(QPointF(x,y))) return items.at(i).id;   // a line is thin: a finger cannot hit it
    }
  return QString();
}

void TouchTemplateEditor::select(const QString &id)
{
  if(id==m_selected) return;
  m_selected=id;
  m_doc.breakMerge();
  refreshSelection();
}

bool TouchTemplateEditor::pointInSelection(qreal x,qreal y,qreal tolerance) const
{
  const tpl::Element *e=selectedElementConst();
  if(!e) return false;
  const QPolygonF poly=outlineOf(*e);
  return poly.containsPoint(QPointF(x,y),Qt::WindingFill) || poly.boundingRect().adjusted(-tolerance,-tolerance,tolerance,tolerance).contains(QPointF(x,y));
}

void TouchTemplateEditor::refreshSelection()
{
  emit selectionChanged();
}

QVariantMap TouchTemplateEditor::sel() const
{
  QVariantMap m;
  const tpl::Element *e=selectedElementConst();
  if(!e) return m;
  m["id"]=e->id;
  m["kind"]=kindName(e->type);
  m["isText"]=e->type==tpl::ItemType::Text;
  m["hasFill"]=e->type==tpl::ItemType::Text || e->type==tpl::ItemType::Rect || e->type==tpl::ItemType::Ellipse;
  m["hasOutline"]=e->type!=tpl::ItemType::Image;
  m["hasRadius"]=e->type==tpl::ItemType::Rect;
  m["freeSize"]=e->type!=tpl::ItemType::Text;
  m["text"]=e->text;
  m["fontSize"]=e->font.pointSize;
  m["bold"]=e->font.weight>=600;
  m["italic"]=e->font.italic;
  m["fill"]=tpl::colorToString(e->fill);
  m["opacity"]=e->opacity;
  m["outlineOn"]=e->outline.style!=Qt::NoPen && e->outline.width>0;
  m["outlineWidth"]=e->outline.width;
  m["outlineColor"]=tpl::colorToString(e->outline.color);
  m["radius"]=e->radius;
  m["shadow"]=e->shadow.on;
  m["locked"]=e->locked;
  m["rotation"]=e->rotation;
  const QRectF b=outlineOf(*e).boundingRect();
  m["x"]=b.x();
  m["y"]=b.y();
  m["w"]=b.width();
  m["h"]=b.height();
  QString h,v;
  switch(e->anchorH)
    {
    case tpl::AnchorH::Left: h=tr("left"); break;
    case tpl::AnchorH::Center: h=tr("middle"); break;
    case tpl::AnchorH::Right: h=tr("right"); break;
    case tpl::AnchorH::Span: h=tr("full width"); break;
    case tpl::AnchorH::Scale: h=tr("scaled"); break;
    }
  switch(e->anchorV)
    {
    case tpl::AnchorV::Top: v=tr("top"); break;
    case tpl::AnchorV::Middle: v=tr("middle"); break;
    case tpl::AnchorV::Bottom: v=tr("bottom"); break;
    case tpl::AnchorV::Span: v=tr("full height"); break;
    case tpl::AnchorV::Scale: v=tr("scaled"); break;
    }
  m["anchorText"]=m_doc.doc().layout==tpl::TemplateDocument::Anchored ? tr("Stays at the %1, %2").arg(v,h) : tr("Stretches with the picture");
  return m;
}

QVariantList TouchTemplateEditor::handles() const
{
  QVariantList l;
  const tpl::Element *e=selectedElementConst();
  if(!e) return l;
  const QRectF lb=tpl::localBounds(*e,renderOptions());
  const QTransform t=tpl::sceneTransform(*e);
  const double fx[8]={0,.5,1,1,1,.5,0,0},fy[8]={0,0,0,.5,1,1,1,.5};
  for(int i=0;i<8;i++)
    {
      const QPointF p=t.map(QPointF(lb.x()+fx[i]*lb.width(),lb.y()+fy[i]*lb.height()));
      QVariantMap m;
      m["x"]=p.x();
      m["y"]=p.y();
      l.append(m);
    }
  return l;
}

QPointF TouchTemplateEditor::turnHandle() const
{
  const tpl::Element *e=selectedElementConst();
  if(!e) return QPointF();
  const QRectF lb=tpl::localBounds(*e,renderOptions());
  const QTransform t=tpl::sceneTransform(*e);
  const QPointF top=t.map(QPointF(lb.center().x(),lb.top()));
  const QPointF mid=t.map(lb.center());
  QPointF d=top-mid;
  const double len=qSqrt(d.x()*d.x()+d.y()*d.y());
  if(len<1e-6) return top+QPointF(0,-24);
  d/=len;
  return top+d*24;   // 24 canvas units outside the middle of the top edge
}

QVariantList TouchTemplateEditor::layers() const
{
  QVariantList l;
  const QVector<tpl::Element> &items=m_doc.doc().items;
  for(int i=items.size()-1;i>=0;i--)
    {
      QVariantMap m;
      m["id"]=items.at(i).id;
      m["kind"]=kindName(items.at(i).type);
      m["label"]=layerLabel(items.at(i));
      m["locked"]=items.at(i).locked;
      l.append(m);
    }
  return l;
}

// ---- what the fingers do ----

void TouchTemplateEditor::moveBegin()
{
  const tpl::Element *e=selectedElementConst();
  if(!e || e->locked) return;
  m_before=*e;
  m_gesture=true;
  m_guides.clear();
}

void TouchTemplateEditor::moveBy(qreal dx,qreal dy)
{
  if(!m_gesture || m_before.id.isEmpty()) return;
  const QRectF c=m_doc.doc().canvas;
  QRectF b=outlineOf(m_before).boundingRect().translated(dx,dy);
  const double snap=5;
  QVariantList guides;
  auto best=[&](double a0,double a1,double a2,double t0,double t1,double t2,double &shift,double &at)
  {
    // the left edge, the middle and the right edge of the item against the canvas's: the nearest within the snap distance
    const double items[3]={a0,a1,a2},targets[3]={t0,t1,t2};
    double bestD=snap+1;
    for(int i=0;i<3;i++)
      for(int j=0;j<3;j++)
        {
          const double d=targets[j]-items[i];
          if(qAbs(d)<qAbs(bestD) && qAbs(d)<=snap) {bestD=d; at=targets[j];}
        }
    shift=qAbs(bestD)<=snap ? bestD : 0;
    return qAbs(bestD)<=snap;
  };
  double sx=0,sy=0,atx=0,aty=0;
  if(best(b.left(),b.center().x(),b.right(),c.left(),c.center().x(),c.right(),sx,atx))
    {
      QVariantMap g; g["vertical"]=true; g["at"]=atx; guides.append(g);
    }
  if(best(b.top(),b.center().y(),b.bottom(),c.top(),c.center().y(),c.bottom(),sy,aty))
    {
      QVariantMap g; g["vertical"]=false; g["at"]=aty; guides.append(g);
    }
  tpl::Element e=m_before;
  e.pos=m_before.pos+QPointF(dx+sx,dy+sy);
  m_doc.replaceRaw(e);
  if(guides.size()>m_guides.size()) emit snapped();
  m_guides=guides;
  emit guidesChanged();
}

void TouchTemplateEditor::moveEnd()
{
  if(!m_gesture) return;
  m_gesture=false;
  m_guides.clear();
  emit guidesChanged();
  const tpl::Element *now=m_doc.find(m_before.id);
  if(now && (now->pos!=m_before.pos)) m_doc.commitModify({m_before},{*now},tr("Move"));
  m_before=tpl::Element();
}

void TouchTemplateEditor::resizeBegin(int handle)
{
  const tpl::Element *e=selectedElementConst();
  if(!e || e->locked || handle<0 || handle>7) return;
  m_before=*e;
  m_handle=handle;
  m_gesture=true;
  const QRectF lb=tpl::localBounds(m_before,renderOptions());
  const double fx[8]={0,.5,1,1,1,.5,0,0},fy[8]={0,0,0,.5,1,1,1,.5};
  // the point that stays where it is: the one opposite the handle
  m_anchorLocal=QPointF(lb.x()+(1-fx[handle])*lb.width(),lb.y()+(1-fy[handle])*lb.height());
}

void TouchTemplateEditor::resizeTo(qreal x,qreal y)
{
  if(!m_gesture || m_handle<0) return;
  const tpl::Element &s=m_before;
  const QTransform t=tpl::sceneTransform(s);
  bool ok=false;
  const QTransform inv=t.inverted(&ok);
  if(!ok) return;
  const QPointF lp=inv.map(QPointF(x,y));
  const QRectF lb=tpl::localBounds(s,renderOptions());
  const double fx[8]={0,.5,1,1,1,.5,0,0},fy[8]={0,0,0,.5,1,1,1,.5};
  const bool hasLeft=fx[m_handle]==0,hasRight=fx[m_handle]==1,hasTop=fy[m_handle]==0,hasBottom=fy[m_handle]==1;
  const bool corner=(hasLeft||hasRight) && (hasTop||hasBottom);
  tpl::Element e=s;
  if(s.type==tpl::ItemType::Text && !s.textBox)
    {
      // free text has a size of its own, the font's: a corner scales the letters
      const QPointF a=m_anchorLocal;
      const QPointF h0=QPointF(lb.x()+fx[m_handle]*lb.width(),lb.y()+fy[m_handle]*lb.height());
      const double dx0=h0.x()-a.x(),dy0=h0.y()-a.y();
      double k=1;
      if(qAbs(dx0)>1e-6 && qAbs(dy0)>1e-6) k=qMax((lp.x()-a.x())/dx0,(lp.y()-a.y())/dy0);
      else if(qAbs(dx0)>1e-6) k=(lp.x()-a.x())/dx0;
      else if(qAbs(dy0)>1e-6) k=(lp.y()-a.y())/dy0;
      k=qBound(0.1,k,20.0);
      EditorDocument::scaleElement(e,t.map(a),k,k);
    }
  else
    {
      double l=lb.left(),r=lb.right(),tp=lb.top(),b=lb.bottom();
      if(hasLeft) l=qMin(lp.x(),r-1);
      if(hasRight) r=qMax(lp.x(),l+1);
      if(hasTop) tp=qMin(lp.y(),b-1);
      if(hasBottom) b=qMax(lp.y(),tp+1);
      if(corner && s.type==tpl::ItemType::Image && lb.width()>0 && lb.height()>0)
        {
          const double k=qMax((r-l)/lb.width(),(b-tp)/lb.height());
          const double w=lb.width()*k,h=lb.height()*k;
          if(hasLeft) l=lb.right()-w; else r=lb.left()+w;
          if(hasTop) tp=lb.bottom()-h; else b=lb.top()+h;
        }
      QRectF nr(l,tp,r-l,b-tp);
      if(s.type==tpl::ItemType::Line) nr=QRectF(l,s.rect.y(),r-l,s.rect.height());
      EditorDocument::setRectKeeping(e,nr,m_anchorLocal);
    }
  m_doc.replaceRaw(e);
}

void TouchTemplateEditor::resizeEnd()
{
  if(!m_gesture) return;
  m_gesture=false;
  m_handle=-1;
  const tpl::Element *now=m_doc.find(m_before.id);
  if(now) m_doc.commitModify({m_before},{*now},tr("Resize"));
  m_before=tpl::Element();
}

void TouchTemplateEditor::turnBegin(qreal x,qreal y)
{
  const tpl::Element *e=selectedElementConst();
  if(!e || e->locked) return;
  m_before=*e;
  m_gesture=true;
  m_turnCenter=tpl::sceneTransform(m_before).map(m_before.rect.center());
  m_turnStart=qRadiansToDegrees(qAtan2(y-m_turnCenter.y(),x-m_turnCenter.x()));
}

void TouchTemplateEditor::turnTo(qreal x,qreal y)
{
  if(!m_gesture) return;
  const double now=qRadiansToDegrees(qAtan2(y-m_turnCenter.y(),x-m_turnCenter.x()));
  double rot=m_before.rotation+(now-m_turnStart);
  while(rot>180) rot-=360;
  while(rot<=-180) rot+=360;
  // it comes to rest at 0, 45, 90 ... degrees
  const double nearest=qRound(rot/45.0)*45.0;
  bool snappedNow=false;
  if(qAbs(rot-nearest)<=4) {rot=nearest; snappedNow=true;}
  tpl::Element e=m_before;
  e.rotation=rot;
  m_doc.replaceRaw(e);
  if(snappedNow && !m_guides.isEmpty()==false) {m_guides=QVariantList{QVariantMap()}; emit snapped();}
  if(!snappedNow) m_guides.clear();
}

void TouchTemplateEditor::turnEnd()
{
  if(!m_gesture) return;
  m_gesture=false;
  m_guides.clear();
  const tpl::Element *now=m_doc.find(m_before.id);
  if(now && now->rotation!=m_before.rotation) m_doc.commitModify({m_before},{*now},tr("Turn"));
  m_before=tpl::Element();
}

void TouchTemplateEditor::pinchBegin()
{
  const tpl::Element *e=selectedElementConst();
  if(!e || e->locked) return;
  m_before=*e;
  m_gesture=true;
}

void TouchTemplateEditor::pinchBy(qreal scale,qreal degrees)
{
  if(!m_gesture || m_before.id.isEmpty()) return;
  tpl::Element e=m_before;
  const QPointF center=outlineOf(m_before).boundingRect().center();
  EditorDocument::scaleElement(e,center,qBound(0.1,double(scale),20.0),qBound(0.1,double(scale),20.0));
  e.rotation=m_before.rotation+degrees;
  m_doc.replaceRaw(e);
}

void TouchTemplateEditor::pinchEnd()
{
  if(!m_gesture) return;
  m_gesture=false;
  const tpl::Element *now=m_doc.find(m_before.id);
  if(now) m_doc.commitModify({m_before},{*now},tr("Resize"));
  m_before=tpl::Element();
}

// ---- edits ----

static tpl::Element defaultElement(tpl::ItemType type)
{
  tpl::Element e;
  e.type=type;
  switch(type)
    {
    case tpl::ItemType::Text:
      e.text=QStringLiteral("Text");
      e.fill=Qt::white;
      e.outline.color=Qt::black;
      e.outline.width=2;
      e.outline.join=Qt::RoundJoin;
      e.font.family=QStringLiteral("Roboto");
      e.font.pointSize=24;
      e.font.weight=700;
      break;
    case tpl::ItemType::Rect:
      e.fill=QColor(0,0,0,150);
      e.outline.style=Qt::NoPen;
      break;
    case tpl::ItemType::Ellipse:
      e.fill=QColor(255,255,255,120);
      e.outline.color=Qt::white;
      e.outline.width=2;
      break;
    case tpl::ItemType::Line:
      e.outline.color=Qt::white;
      e.outline.width=3;
      e.rect=QRectF(0,0,100,2);
      break;
    case tpl::ItemType::Image:
      e.outline.style=Qt::NoPen;
      break;
    case tpl::ItemType::Replay:
      e.outline.color=Qt::white;
      e.outline.width=2;
      break;
    }
  return e;
}

void TouchTemplateEditor::addItem(const QString &kind,qreal cx,qreal cy)
{
  const QRectF c=m_doc.doc().canvas;
  tpl::Element e;
  if(kind==QLatin1String("bar"))
    {
      e=templatePresets::makeBar(0,true,c);
    }
  else
    {
      tpl::ItemType type=tpl::ItemType::Rect;
      QSizeF size(c.width()*0.35,c.height()*0.2);
      if(kind==QLatin1String("text")) type=tpl::ItemType::Text;
      else if(kind==QLatin1String("ellipse")) {type=tpl::ItemType::Ellipse; size=QSizeF(c.height()*0.25,c.height()*0.25);}
      else if(kind==QLatin1String("line")) {type=tpl::ItemType::Line; size=QSizeF(c.width()*0.4,2);}
      else if(kind==QLatin1String("replay")) {type=tpl::ItemType::Replay; size=QSizeF(c.width()*0.3,c.width()*0.3*0.75);}
      e=defaultElement(type);
      if(type==tpl::ItemType::Text)
        {
          e.font.pointSize=qMax(10.0,c.height()*0.09);   // a size that reads on a picture of this size
          EditorDocument::fitTextRect(e);
          const QRectF lb=tpl::localBounds(e,renderOptions());
          e.pos=QPointF(cx,cy)-lb.center();
        }
      else
        {
          e.rect=QRectF(0,0,size.width(),size.height());
          e.pos=QPointF(cx-size.width()/2,cy-size.height()/2);
        }
      if(m_doc.doc().layout==tpl::TemplateDocument::Anchored) tpl::autoAnchor(e,c);
    }
  const QStringList ids=m_doc.addItems({e},tr("Add"));
  if(!ids.isEmpty()) m_selected=ids.first();
  refreshSelection();
}

void TouchTemplateEditor::addImage(const QString &file,qreal cx,qreal cy)
{
  QImageReader reader(file);
  reader.setAutoTransform(true);
  QImage img=reader.read();
  if(img.isNull()) {m_error=reader.errorString(); return;}
  const QRectF c=m_doc.doc().canvas;
  tpl::Element e=defaultElement(tpl::ItemType::Image);
  if(img.width()>tpl::limits::maxImageSide || img.height()>tpl::limits::maxImageSide)
    img=img.scaled(tpl::limits::maxImageSide,tpl::limits::maxImageSide,Qt::KeepAspectRatio,Qt::SmoothTransformation);
  e.image=img;
  const double k=qMin(c.width()*0.4/img.width(),c.height()*0.4/img.height());
  e.rect=QRectF(0,0,img.width()*k,img.height()*k);
  e.pos=QPointF(cx-e.rect.width()/2,cy-e.rect.height()/2);
  if(m_doc.doc().layout==tpl::TemplateDocument::Anchored) tpl::autoAnchor(e,c);
  const QStringList ids=m_doc.addItems({e},tr("Add"));
  if(!ids.isEmpty()) m_selected=ids.first();
  refreshSelection();
}

void TouchTemplateEditor::pickImage()
{
  m_pendingImage=true;
  TxController::instance()->pickPhoto();
}

void TouchTemplateEditor::deleteSelected()
{
  if(m_selected.isEmpty()) return;
  const QString id=m_selected;
  m_selected.clear();
  m_doc.removeItems({id},tr("Delete"));
  refreshSelection();
}

void TouchTemplateEditor::duplicateSelected()
{
  const tpl::Element *e=selectedElementConst();
  if(!e) return;
  tpl::Element copy=*e;
  copy.pos+=QPointF(12,12);   // beside the original, so that it can be seen and moved
  copy.locked=false;
  const QStringList ids=m_doc.addItems({copy},tr("Duplicate"));
  if(!ids.isEmpty()) m_selected=ids.first();
  refreshSelection();
}

void TouchTemplateEditor::setLocked(const QString &id,bool locked)
{
  const tpl::Element *e=m_doc.find(id);
  if(!e || e->locked==locked) return;
  tpl::Element c=*e;
  c.locked=locked;
  m_doc.modifyItems({c},locked ? tr("Lock") : tr("Unlock"),QString(),false);
  if(id==m_selected) refreshSelection();
}

void TouchTemplateEditor::setProp(const QString &name,const QVariant &value)
{
  const tpl::Element *cur=selectedElementConst();
  if(!cur || (cur->locked && name!=QLatin1String("locked"))) return;
  tpl::Element e=*cur;
  QString key=name+QLatin1Char(':')+e.id;
  bool followAnchors=true;
  if(name==QLatin1String("text")) e.text=value.toString().left(tpl::limits::maxTextLength);
  else if(name==QLatin1String("fontSize")) e.font.pointSize=qBound(4.0,value.toDouble(),400.0);
  else if(name==QLatin1String("bold")) e.font.weight=value.toBool() ? 700 : 400;
  else if(name==QLatin1String("italic")) e.font.italic=value.toBool();
  else if(name==QLatin1String("fill"))
    {
      const QString v=value.toString();
      QColor c=tpl::colorFromString(v,e.fill);
      if(v.length()<=7) c.setAlpha(e.fill.alpha());   // a swatch has no transparency of its own: the item keeps its own
      e.fill=c;
    }
  else if(name==QLatin1String("opacity")) e.opacity=qBound(0.0,value.toDouble(),1.0);
  else if(name==QLatin1String("outlineColor")) {e.outline.color=tpl::colorFromString(value.toString(),e.outline.color); if(e.outline.style==Qt::NoPen) {e.outline.style=Qt::SolidLine; if(e.outline.width<=0) e.outline.width=2;}}
  else if(name==QLatin1String("outlineWidth"))
    {
      e.outline.width=qBound(0.0,value.toDouble(),50.0);
      e.outline.style=e.outline.width>0 ? Qt::SolidLine : Qt::NoPen;
    }
  else if(name==QLatin1String("outlineOn"))
    {
      if(value.toBool()) {e.outline.style=Qt::SolidLine; if(e.outline.width<=0) e.outline.width=2;}
      else e.outline.style=Qt::NoPen;
    }
  else if(name==QLatin1String("radius")) e.radius=qBound(0.0,value.toDouble(),200.0);
  else if(name==QLatin1String("shadow"))
    {
      e.shadow.on=value.toBool();
      if(e.shadow.on && e.shadow.blur==0) e.shadow.blur=3;
    }
  else if(name==QLatin1String("rotation"))
    {
      double r=value.toDouble();
      while(r>180) r-=360;
      while(r<=-180) r+=360;
      e.rotation=r;
    }
  else if(name==QLatin1String("x") || name==QLatin1String("y"))
    {
      const QRectF b=outlineOf(e).boundingRect();
      if(name==QLatin1String("x")) e.pos.rx()+=value.toDouble()-b.x();
      else e.pos.ry()+=value.toDouble()-b.y();
    }
  else if((name==QLatin1String("w") || name==QLatin1String("h")) && e.type!=tpl::ItemType::Text)
    {
      QRectF r=e.rect;
      if(name==QLatin1String("w")) r.setWidth(qMax(1.0,value.toDouble()));
      else if(e.type!=tpl::ItemType::Line) r.setHeight(qMax(1.0,value.toDouble()));
      EditorDocument::setRectKeeping(e,r,e.rect.topLeft());
    }
  else if(name==QLatin1String("locked")) {e.locked=value.toBool(); key.clear();}
  else return;
  m_doc.modifyItems({e},tr("Change"),key,followAnchors);
  refreshSelection();
}

void TouchTemplateEditor::moveLayer(int from,int to)
{
  QStringList order=m_doc.ids();            // the one at the back first
  const int n=order.size();
  if(from<0 || from>=n || to<0 || to>=n || from==to) return;
  const int f=n-1-from,t=n-1-to;
  order.move(f,t);
  m_doc.reorder(order,tr("Reorder"));
}

void TouchTemplateEditor::undo()
{
  m_doc.undoStack()->undo();
  refreshSelection();
}

void TouchTemplateEditor::redo()
{
  m_doc.undoStack()->redo();
  refreshSelection();
}

void TouchTemplateEditor::clearAll()
{
  m_selected.clear();
  m_doc.clearItems(tr("Clear everything"));
  refreshSelection();
}

// The descriptions of the macro table (src/template) name the places of the desktop: the TX tab, Configuration > GPS. On the phone the same things are in
// the Details of the Transmit screen and under Settings > Position, and the three comment lines have no box at all.
static QString phoneDescription(QChar tag,const QString &generic)
{
  auto t=[](const char *s) {return QCoreApplication::translate("TouchTemplateEditor",s);};
  switch(tag.unicode())
    {
    case 'c': return t("Call sign of the station you are sending to (Details on the Transmit screen)");
    case 'r': return t("RSV report (Details on the Transmit screen)");
    case 'o': return t("Name of the operator you are sending to (Details on the Transmit screen)");
    case 'x': return t("First comment line (the phone has no box for it)");
    case 'y': return t("Second comment line (the phone has no box for it)");
    case 'z': return t("Third comment line (the phone has no box for it)");
    case 'a': return t("Latitude of the phone's position, in decimal degrees (north positive) or as 47 36.37N, see Settings > Position");
    case 'b': return t("Longitude of the phone's position, in decimal degrees (east positive) or as 122 19.93W, see Settings > Position");
    case 'g': return t("City and state of the position (looked up on the internet from the phone's position, see Settings > Position)");
    default: return generic;
    }
}

QVariantList TouchTemplateEditor::macros() const
{
  QVariantList l;
  for(const tpl::MacroInfo &m:tpl::macroTable())
    {
      QVariantMap v;
      v["tag"]=QString(m.tag);
      v["name"]=m.name;
      v["description"]=phoneDescription(m.tag,m.description);
      v["sample"]=m.sample;
      l.append(v);
    }
  return l;
}
