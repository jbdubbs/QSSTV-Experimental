#include "editorcanvas.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImageReader>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QUrl>
#include <QtMath>
#include <algorithm>
#include <functional>
#include "editorcommands.h"

namespace
{

const double handlePx = 8;          //!< drawn size of a resize handle (viewport pixels)
const double handleHitPx = 7;       //!< pick distance
const double rotateOffsetPx = 26;   //!< rotate handle distance above the top edge
const double snapPx = 6;            //!< snapping distance
const double clickSlopPx = 4;       //!< movement below this is a click, not a drag
const int maxImportSide = 1600;     //!< images are shrunk to this when inserted (keeps template files small)

QPointF localPoint(const QRectF &r, int handle)
{
  switch (handle)
    {
    case 0: return r.topLeft();
    case 1: return QPointF(r.center().x(), r.top());
    case 2: return r.topRight();
    case 3: return QPointF(r.right(), r.center().y());
    case 4: return r.bottomRight();
    case 5: return QPointF(r.center().x(), r.bottom());
    case 6: return r.bottomLeft();
    case 7: return QPointF(r.left(), r.center().y());
    }
  return r.center();
}

double distToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
  const QPointF ab = b - a;
  const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
  double t = len2 > 0 ? ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2 : 0;
  t = qBound(0.0, t, 1.0);
  return QLineF(p, a + ab * t).length();
}

QRectF unite(const QRectF &a, const QRectF &b)
{
  return a.isNull() ? b : a.united(b);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// in-place text editor, a child of the viewport
namespace
{
class InlineTextEdit : public QPlainTextEdit
{
public:
  using QPlainTextEdit::QPlainTextEdit;
  std::function<void(const QString &)> onCommit;
  bool done = false;
protected:
  void keyPressEvent(QKeyEvent *e) override
  {
    if (e->key() == Qt::Key_Escape)
      {
        finish(false);
        return;
      }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && (e->modifiers() & Qt::ControlModifier))
      {
        finish(true);
        return;
      }
    QPlainTextEdit::keyPressEvent(e);
  }
  void focusOutEvent(QFocusEvent *e) override
  {
    QPlainTextEdit::focusOutEvent(e);
    // the context menu (paste) and a switch to another window take the focus but do not end the editing
    if (e->reason() == Qt::PopupFocusReason || e->reason() == Qt::ActiveWindowFocusReason) return;
    finish(true);
  }
public:
  /*! drop the editor without committing (the document it belonged to is gone) */
  void abandon()
  {
    onCommit = nullptr;
    done = true;
    hide();
    deleteLater();
  }
private:
  void finish(bool commit)
  {
    if (done) return;
    done = true;
    const QString text = toPlainText();
    hide();
    if (commit && onCommit) onCommit(text);
    deleteLater();
  }
};
}  // namespace

// ---------------------------------------------------------------------------------------------------------------
ElementItem::ElementItem(const tpl::Element &e, const tpl::RenderOptions *opt) : elem(e), opt(opt)
{
  setElement(e);
}

void ElementItem::setElement(const tpl::Element &e)
{
  elem = e;
  setPos(e.pos);
  refresh();
}

void ElementItem::optionsChanged()
{
  refresh();
}

void ElementItem::refresh()
{
  tpl::Element local = elem;
  local.pos = QPointF();
  const QRectF b = tpl::sceneTransform(local).mapRect(tpl::localBounds(local, *opt));
  const double m = elem.outline.width * 2 + 4;
  prepareGeometryChange();
  bounds = b.adjusted(-m, -m, m, m);
  if (elem.shadow.on)
    {
      const double sm = m + elem.shadow.blur * 2 + 4;
      bounds = bounds.united(b.translated(elem.shadow.dx, elem.shadow.dy).adjusted(-sm, -sm, sm, sm));
    }
  update();
}

void ElementItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
  tpl::Element local = elem;
  local.pos = QPointF();
  if (elem.type == tpl::ItemType::Image && !elem.image.isNull())
    {
      // scaled when painted, not when the element changes: a drag changes it on every mouse move but paints less often
      const QSize size(static_cast<int>(elem.rect.width()), static_cast<int>(elem.rect.height()));
      if (size.width() > 0 && size.height() > 0 && qint64(size.width()) * size.height() <= 16LL * 1024 * 1024)
        {
          if (elem.image.cacheKey() != scaledKey || size != scaledSize || scaled.isNull())
            {
              scaled = elem.image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
              scaledKey = elem.image.cacheKey();
              scaledSize = size;
            }
          local.image = scaled;
        }
    }
  else if (!scaled.isNull())
    {
      scaled = QImage();
    }
  p->setRenderHint(QPainter::Antialiasing);   // smooth pixmap sampling is the view's choice (on screen); renderItems() draws like the transmit path
  tpl::paintElement(*p, local, *opt);
}

// ---------------------------------------------------------------------------------------------------------------
EditorCanvas::EditorCanvas(EditorDocument *document, QWidget *parent) : QGraphicsView(parent), doc(document)
{
  setScene(&scene);
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);
  setTransformationAnchor(QGraphicsView::NoAnchor);
  setResizeAnchor(QGraphicsView::NoAnchor);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);   // the overlay (handles, guides) is painted in the foreground
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAcceptDrops(true);
  setBackgroundRole(QPalette::Window);
  renderOpt.expandMacros = false;

  connect(doc, &EditorDocument::reset, this, &EditorCanvas::docReset);
  connect(doc, &EditorDocument::itemInserted, this, &EditorCanvas::docItemInserted);
  connect(doc, &EditorDocument::itemRemoved, this, &EditorCanvas::docItemRemoved);
  connect(doc, &EditorDocument::itemChanged, this, &EditorCanvas::docItemChanged);
  connect(doc, &EditorDocument::orderChanged, this, &EditorCanvas::docOrderChanged);
  connect(doc, &EditorDocument::canvasChanged, this, &EditorCanvas::docCanvasChanged);
  connect(doc, &EditorDocument::selectionHint, this, &EditorCanvas::setSelection);
  docReset();
}

EditorCanvas::~EditorCanvas()
{
  // the scene is a member: drop the items while the render options (also a member) are still alive
  setScene(nullptr);
  scene.clear();
  items.clear();
}

// --- document -> scene ---------------------------------------------------------------------------------------
void EditorCanvas::docReset()
{
  if (textEdit) static_cast<InlineTextEdit *>(textEdit.data())->abandon();
  op = OpNone;
  scene.clear();
  items.clear();
  selected.clear();
  for (const tpl::Element &e : doc->doc().items)
    {
      ElementItem *it = new ElementItem(e, &renderOpt);
      scene.addItem(it);
      items.insert(e.id, it);
    }
  syncZ();
  updateSceneRect();
  autoFit = true;
  zoomFit();
  emit selectionChanged();
  viewport()->update();
}

void EditorCanvas::docItemInserted(const QString &id)
{
  const tpl::Element *e = doc->find(id);
  if (!e || items.contains(id)) return;
  ElementItem *it = new ElementItem(*e, &renderOpt);
  scene.addItem(it);
  items.insert(id, it);
  syncZ();
  viewport()->update();
}

void EditorCanvas::docItemRemoved(const QString &id)
{
  ElementItem *it = items.take(id);
  if (it)
    {
      scene.removeItem(it);
      delete it;
    }
  if (selected.remove(id)) emit selectionChanged();
  viewport()->update();
}

void EditorCanvas::docItemChanged(const QString &id)
{
  const tpl::Element *e = doc->find(id);
  ElementItem *it = items.value(id);
  if (e && it) it->setElement(*e);
  viewport()->update();
}

void EditorCanvas::docOrderChanged()
{
  syncZ();
  viewport()->update();
}

void EditorCanvas::docCanvasChanged()
{
  updateSceneRect();
  if (autoFit) zoomFit();
  viewport()->update();
}

void EditorCanvas::syncZ()
{
  int z = 0;
  for (const tpl::Element &e : doc->doc().items)
    {
      ElementItem *it = items.value(e.id);
      if (it) it->setZValue(z++);
    }
}

void EditorCanvas::updateSceneRect()
{
  const QRectF c = doc->doc().canvas;
  const double m = qMax(c.width(), c.height()) * 0.75;
  scene.setSceneRect(c.adjusted(-m, -m, m, m));
}

// --- selection -----------------------------------------------------------------------------------------------
QStringList EditorCanvas::selection() const
{
  QStringList l;
  for (const tpl::Element &e : doc->doc().items)
    if (selected.contains(e.id)) l.append(e.id);
  return l;
}

QVector<tpl::Element> EditorCanvas::selectedElements() const
{
  return doc->elements(selection());
}

void EditorCanvas::setSelectionInternal(const QSet<QString> &ids)
{
  QSet<QString> valid;
  for (const QString &id : ids)
    if (doc->indexOf(id) >= 0) valid.insert(id);
  if (valid == selected) return;
  selected = valid;
  doc->breakMerge();
  emit selectionChanged();
  viewport()->update();
}

void EditorCanvas::setSelection(const QStringList &ids)
{
  setSelectionInternal(QSet<QString>(ids.begin(), ids.end()));
}

void EditorCanvas::selectAll()
{
  setSelection(doc->ids());
}

QRectF EditorCanvas::selectionBounds() const
{
  QRectF r;
  for (const tpl::Element &e : selectedElements()) r = unite(r, sceneBounds(e));
  return r;
}

QRectF EditorCanvas::localBoundsOf(const tpl::Element &e) const
{
  if (e.type != tpl::ItemType::Text || e.textBox) return tpl::localBounds(e, renderOpt);
  // laying out a text is the expensive part and the hit tests, handles and guides ask for it all the time: remember it
  const QString key = e.text + QLatin1Char('\x1f') + e.font.family + QLatin1Char('\x1f') + QString::number(e.font.pointSize) + QLatin1Char('|')
                      + QString::number(e.font.weight) + QLatin1Char('|') + QString::number(e.font.stretch) + QLatin1Char('|')
                      + QString::number(e.font.letterSpacing) + QLatin1Char('|') + QString::number(int(e.font.italic) | int(e.font.underline) << 1 | int(e.font.strikeOut) << 2)
                      + QLatin1Char('|') + (renderOpt.expandMacros ? QString::number(varsGeneration) : QStringLiteral("-"));
  auto it = boundsCache.constFind(key);
  if (it != boundsCache.constEnd()) return it.value();
  if (boundsCache.size() > 4000) boundsCache.clear();
  const QRectF r = tpl::localBounds(e, renderOpt);
  boundsCache.insert(key, r);
  return r;
}

QPolygonF EditorCanvas::outlineOf(const tpl::Element &e) const
{
  return tpl::sceneTransform(e).map(QPolygonF(localBoundsOf(e)));
}

QRectF EditorCanvas::sceneBounds(const tpl::Element &e) const
{
  return outlineOf(e).boundingRect();
}

QImage EditorCanvas::renderItems(double scale)
{
  const QRectF c = doc->doc().canvas;
  QImage img(qMax(1, qRound(c.width() * scale)), qMax(1, qRound(c.height() * scale)), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  scene.render(&p, QRectF(QPointF(0, 0), QSizeF(img.size())), c, Qt::IgnoreAspectRatio);
  return img;
}

QString EditorCanvas::describe(const tpl::Element &e) const
{
  switch (e.type)
    {
    case tpl::ItemType::Text:
      {
        QString t = e.text.section(QLatin1Char('\n'), 0, 0);
        if (t.size() > 28) t = t.left(27) + QStringLiteral("…");
        return tr("Text: %1").arg(t);
      }
    case tpl::ItemType::Rect: return tr("Rectangle");
    case tpl::ItemType::Ellipse: return tr("Ellipse");
    case tpl::ItemType::Line: return tr("Line");
    case tpl::ItemType::Image: return tr("Image");
    case tpl::ItemType::Replay: return tr("Replay picture");
    }
  return QString();
}

// --- view ----------------------------------------------------------------------------------------------------
void EditorCanvas::setZoom(double z, const QPointF &anchorView)
{
  z = qBound(0.05, z, 16.0);
  const QPointF anchor = anchorView.x() < 0 ? QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0) : anchorView;
  const QPointF sceneAnchor = mapToScene(anchor.toPoint());
  zoomFactor = z;
  setTransform(QTransform::fromScale(z, z));
  const QPointF now = viewportTransform().map(sceneAnchor);
  horizontalScrollBar()->setValue(horizontalScrollBar()->value() + qRound(now.x() - anchor.x()));
  verticalScrollBar()->setValue(verticalScrollBar()->value() + qRound(now.y() - anchor.y()));
  autoFit = false;
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  emit zoomChanged(zoomFactor);
  viewport()->update();
}

void EditorCanvas::zoomFit()
{
  const QRectF c = doc->doc().canvas;
  const double margin = 24;
  const double w = qMax(50, viewport()->width()) - 2 * margin;
  const double h = qMax(50, viewport()->height()) - 2 * margin;
  const double z = qBound(0.05, qMin(w / c.width(), h / c.height()), 16.0);
  zoomFactor = z;
  setTransform(QTransform::fromScale(z, z));
  centerOn(c.center());
  autoFit = true;
  // the whole canvas is in view: no scroll bars (zooming brings them back)
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  emit zoomChanged(zoomFactor);
  viewport()->update();
}

void EditorCanvas::resizeEvent(QResizeEvent *e)
{
  QGraphicsView::resizeEvent(e);
  if (autoFit) zoomFit();
}

void EditorCanvas::showEvent(QShowEvent *e)
{
  QGraphicsView::showEvent(e);
  if (autoFit) zoomFit();
}

void EditorCanvas::setShowGrid(bool on)
{
  gridVisible = on;
  viewport()->update();
}

void EditorCanvas::setGridSize(int px)
{
  gridPx = qMax(2, px);
  viewport()->update();
}

void EditorCanvas::setExpandMacros(bool on)
{
  renderOpt.expandMacros = on;
  for (ElementItem *it : items) it->optionsChanged();
  viewport()->update();
}

void EditorCanvas::setMacroVars(const QHash<QChar, QString> &vars)
{
  renderOpt.vars = vars;
  varsGeneration++;
  for (ElementItem *it : items) it->optionsChanged();
  viewport()->update();
}

void EditorCanvas::setGuides(bool center, bool margin5, bool margin10)
{
  guideCenter = center;
  guideMargin5 = margin5;
  guideMargin10 = margin10;
  viewport()->update();
}

void EditorCanvas::setPreviewImage(const QImage &img)
{
  preview = img;
  viewport()->update();
}

void EditorCanvas::setShowPreview(bool on)
{
  previewVisible = on;
  viewport()->update();
}

void EditorCanvas::drawBackground(QPainter *p, const QRectF &rect)
{
  p->fillRect(rect, palette().color(QPalette::Window).darker(120));
  const QRectF c = doc->doc().canvas;
  const double px = 1.0 / zoomFactor;
  p->fillRect(c.translated(3 * px, 3 * px), QColor(0, 0, 0, 70));
  if (previewVisible && !preview.isNull())
    {
      p->setRenderHint(QPainter::SmoothPixmapTransform);
      p->drawImage(c, preview);
    }
  else
    {
      static QPixmap checker;
      if (checker.isNull())
        {
          checker = QPixmap(16, 16);
          checker.fill(QColor(0xf4, 0xf4, 0xf4));
          QPainter cp(&checker);
          cp.fillRect(0, 0, 8, 8, QColor(0xdc, 0xdc, 0xdc));
          cp.fillRect(8, 8, 8, 8, QColor(0xdc, 0xdc, 0xdc));
        }
      QBrush b(checker);
      b.setTransform(QTransform::fromScale(px, px));
      p->fillRect(c, b);
    }
  if (doc->doc().sampledTransparency) p->fillRect(c, doc->doc().background);
  if (gridVisible && gridPx * zoomFactor >= 4)
    {
      p->save();
      p->setClipRect(c);
      QPen pen(QColor(0, 0, 0, 55), 0);
      p->setPen(pen);
      for (double x = c.left() + gridPx; x < c.right(); x += gridPx) p->drawLine(QPointF(x, c.top()), QPointF(x, c.bottom()));
      for (double y = c.top() + gridPx; y < c.bottom(); y += gridPx) p->drawLine(QPointF(c.left(), y), QPointF(c.right(), y));
      p->restore();
    }
}

void EditorCanvas::drawForeground(QPainter *p, const QRectF &rect)
{
  const QRectF c = doc->doc().canvas;
  // what lies outside the canvas is not transmitted: dim it
  {
    QPainterPath outer;
    outer.addRect(rect);
    QPainterPath inner;
    inner.addRect(c);
    QColor dim = palette().color(QPalette::Window).darker(120);
    dim.setAlpha(150);
    p->fillPath(outer.subtracted(inner), dim);
  }
  p->setPen(QPen(QColor(0, 0, 0, 160), 0));
  p->setBrush(Qt::NoBrush);
  p->drawRect(c);

  // guides to place things by: dashed, light blue, over the template
  if (guideCenter || guideMargin5 || guideMargin10)
    {
      p->setPen(QPen(QColor(0, 150, 215, 210), 0, Qt::DashLine));
      p->setBrush(Qt::NoBrush);
      if (guideCenter)
        {
          p->drawLine(QPointF(c.center().x(), c.top()), QPointF(c.center().x(), c.bottom()));
          p->drawLine(QPointF(c.left(), c.center().y()), QPointF(c.right(), c.center().y()));
        }
      for (const auto &m : {qMakePair(guideMargin5, 0.05), qMakePair(guideMargin10, 0.10)})
        if (m.first) p->drawRect(c.adjusted(c.width() * m.second, c.height() * m.second, -c.width() * m.second, -c.height() * m.second));
    }

  // snap guides (canvas coordinates)
  if (!guideXs.isEmpty() || !guideYs.isEmpty())
    {
      p->setPen(QPen(QColor(255, 0, 160), 0));
      for (double x : guideXs) p->drawLine(QPointF(x, c.top() - 4), QPointF(x, c.bottom() + 4));
      for (double y : guideYs) p->drawLine(QPointF(c.left() - 4, y), QPointF(c.right() + 4, y));
    }

  // selection and handles are drawn in viewport pixels so they keep their size at any zoom
  p->save();
  p->setWorldTransform(QTransform());
  const QColor accent = palette().color(QPalette::Highlight);
  const QVector<tpl::Element> sel = selectedElements();
  for (const tpl::Element &e : sel)
    {
      const QPolygonF poly = viewportTransform().map(outlineOf(e));
      QPen pen(accent, 1);
      if (e.locked) pen.setStyle(Qt::DashLine);
      p->setPen(pen);
      p->setBrush(Qt::NoBrush);
      p->setRenderHint(QPainter::Antialiasing, false);
      p->drawPolygon(poly);
    }
  if (sel.size() > 1)
    {
      const QPolygonF poly = viewportTransform().map(QPolygonF(selectionBounds()));
      p->setPen(QPen(accent, 1, Qt::DotLine));
      p->drawPolygon(poly);
    }
  if (sel.size() > 1 && op != OpMove && op != OpBand && op != OpCreate && !groupFrame().isNull())
    {
      // the frame of the unlocked items has handles of its own: they are resized and turned together
      const QRectF frame = groupFrame();
      if (!frame.isNull())
        {
          p->setPen(QPen(accent, 1));
          p->setBrush(Qt::NoBrush);
          p->setRenderHint(QPainter::Antialiasing, false);
          p->drawPolygon(viewportTransform().map(QPolygonF(frame)));
          p->setRenderHint(QPainter::Antialiasing, true);
          for (int h = 0; h <= HRotate; h++)
            {
              const QPointF pt = groupHandleViewPoint(Handle(h));
              p->setPen(QPen(accent, 1));
              p->setBrush(palette().color(QPalette::Base));
              if (h == HRotate)
                {
                  p->drawLine(groupHandleViewPoint(HTop), pt);
                  p->drawEllipse(pt, handlePx / 2 + 1, handlePx / 2 + 1);
                }
              else p->drawRect(QRectF(pt.x() - handlePx / 2, pt.y() - handlePx / 2, handlePx, handlePx));
            }
        }
    }
  if (sel.size() == 1 && !sel.first().locked && op != OpMove)
    {
      const tpl::Element &e = sel.first();
      p->setRenderHint(QPainter::Antialiasing, true);
      for (int h = 0; h <= HRotate; h++)
        {
          if (!handleAllowed(e, Handle(h))) continue;
          const QPointF pt = handleViewPoint(e, Handle(h));
          if (h == HRotate)
            {
              const QPointF top = handleViewPoint(e, HTop);
              p->setPen(QPen(accent, 1));
              p->drawLine(top, pt);
              p->setBrush(palette().color(QPalette::Base));
              p->drawEllipse(pt, handlePx / 2 + 1, handlePx / 2 + 1);
            }
          else
            {
              p->setPen(QPen(accent, 1));
              p->setBrush(palette().color(QPalette::Base));
              p->drawRect(QRectF(pt.x() - handlePx / 2, pt.y() - handlePx / 2, handlePx, handlePx));
            }
        }
    }
  if (op == OpBand || op == OpCreate)
    {
      const QPolygonF poly = viewportTransform().map(QPolygonF(createRect));
      p->setRenderHint(QPainter::Antialiasing, false);
      if (op == OpBand)
        {
          p->setPen(QPen(accent, 1));
          p->setBrush(QColor(accent.red(), accent.green(), accent.blue(), 40));
        }
      else
        {
          p->setPen(QPen(accent, 1, Qt::DashLine));
          p->setBrush(Qt::NoBrush);
        }
      p->drawPolygon(poly);
    }
  p->restore();
}

// --- handles -------------------------------------------------------------------------------------------------
bool EditorCanvas::handleAllowed(const tpl::Element &e, Handle h) const
{
  if (h == HRotate) return true;
  switch (e.type)
    {
    case tpl::ItemType::Text: return e.textBox || h == HTopLeft || h == HTopRight || h == HBottomRight || h == HBottomLeft;
    case tpl::ItemType::Line: return h == HLeft || h == HRight;
    default: return true;
    }
}

QPointF EditorCanvas::handlePoint(const tpl::Element &e, Handle h) const
{
  return tpl::sceneTransform(e).map(localPoint(localBoundsOf(e), h));
}

QPointF EditorCanvas::handleViewPoint(const tpl::Element &e, Handle h) const
{
  if (h != HRotate) return toView(handlePoint(e, h));
  const QPointF top = toView(handlePoint(e, HTop));
  const QPointF bottom = toView(handlePoint(e, HBottom));
  QPointF up = top - bottom;
  const double len = qSqrt(up.x() * up.x() + up.y() * up.y());
  up = len > 0.5 ? up / len : QPointF(0, -1);
  return top + up * rotateOffsetPx;
}

QRectF EditorCanvas::groupFrame() const
{
  if (selected.size() < 2) return QRectF();
  QRectF r;
  for (const tpl::Element &e : selectedElements())
    if (!e.locked) r = unite(r, sceneBounds(e));
  return r;
}

QPointF EditorCanvas::groupHandleViewPoint(Handle h) const
{
  const QRectF r = groupFrame();
  if (h != HRotate) return toView(localPoint(r, h));
  return toView(localPoint(r, HTop)) + QPointF(0, -rotateOffsetPx);
}

EditorCanvas::Handle EditorCanvas::handleAt(const QPoint &viewPt) const
{
  if (selected.size() >= 2)
    {
      if (groupFrame().isNull()) return HNone;
      for (int h = HRotate; h >= 0; h--)
        if (QLineF(QPointF(viewPt), groupHandleViewPoint(Handle(h))).length() <= handleHitPx) return Handle(h);
      return HNone;
    }
  if (selected.size() != 1) return HNone;
  const tpl::Element *e = doc->find(*selected.begin());
  if (!e || e->locked) return HNone;
  for (int h = HRotate; h >= 0; h--)   // the rotate handle first: it can sit over a small item
    {
      if (!handleAllowed(*e, Handle(h))) continue;
      if (QLineF(QPointF(viewPt), handleViewPoint(*e, Handle(h))).length() <= handleHitPx) return Handle(h);
    }
  return HNone;
}

QString EditorCanvas::hitTest(const QPointF &pt, bool includeLocked) const
{
  const double tol = 3.0 / zoomFactor;
  const QVector<tpl::Element> &all = doc->doc().items;
  for (int i = all.size() - 1; i >= 0; i--)
    {
      const tpl::Element &e = all[i];
      if (e.locked && !includeLocked) continue;
      const QPolygonF poly = outlineOf(e);
      if (poly.containsPoint(pt, Qt::OddEvenFill)) return e.id;
      for (int k = 0; k < poly.size(); k++)
        if (distToSegment(pt, poly[k], poly[(k + 1) % poly.size()]) <= tol) return e.id;
    }
  return QString();
}

// --- snapping ------------------------------------------------------------------------------------------------
void EditorCanvas::collectSnapLines()
{
  snapXs.clear();
  snapYs.clear();
  if (!snapObjects) return;
  const QRectF c = doc->doc().canvas;
  snapXs << c.left() << c.center().x() << c.right();
  snapYs << c.top() << c.center().y() << c.bottom();
  for (const tpl::Element &e : doc->doc().items)
    {
      if (selected.contains(e.id)) continue;
      const QRectF b = sceneBounds(e);
      snapXs << b.left() << b.center().x() << b.right();
      snapYs << b.top() << b.center().y() << b.bottom();
    }
}

double EditorCanvas::snapDelta(const QVector<double> &candidates, const double (&edges)[3], QVector<double> &guides) const
{
  const double threshold = snapPx / zoomFactor;
  double best = 0;
  double bestAbs = threshold + 1;
  for (double c : candidates)
    for (double ed : edges)
      {
        const double d = c - ed;
        if (qAbs(d) < bestAbs)
          {
            bestAbs = qAbs(d);
            best = d;
          }
      }
  guides.clear();
  if (bestAbs > threshold) return 0;
  for (double c : candidates)
    for (double ed : edges)
      if (qAbs(c - (ed + best)) < 0.01 && !guides.contains(c)) guides << c;
  return best;
}

double EditorCanvas::snapEdge(const QVector<double> &candidates, double value, QVector<double> &guides) const
{
  const double edges[3] = {value, value, value};
  return snapDelta(candidates, edges, guides);
}

QPointF EditorCanvas::snapPoint(const QPointF &p, Qt::KeyboardModifiers mods) const
{
  if (!snapGrid || (mods & Qt::AltModifier)) return p;
  const QRectF c = doc->doc().canvas;
  return QPointF(c.left() + qRound((p.x() - c.left()) / gridPx) * double(gridPx), c.top() + qRound((p.y() - c.top()) / gridPx) * double(gridPx));
}

// --- mouse ---------------------------------------------------------------------------------------------------
void EditorCanvas::mousePressEvent(QMouseEvent *e)
{
  setFocus();
  const QPointF scenePt = mapToScene(e->pos());
  if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && spaceDown))
    {
      op = OpPan;
      panLast = e->pos();
      viewport()->setCursor(Qt::ClosedHandCursor);
      return;
    }
  if (e->button() != Qt::LeftButton) return;
  if (textEdit) textEdit->clearFocus();   // commits

  const Qt::KeyboardModifiers mods = e->modifiers();
  opStartView = e->pos();
  opStartScene = scenePt;
  opMoved = false;
  opShiftToggled = false;

  if (currentTool != ToolSelect)
    {
      op = OpCreate;
      opStartScene = snapPoint(scenePt, mods);
      createRect = QRectF(opStartScene, QSizeF(0, 0));
      return;
    }

  const Handle h = handleAt(e->pos());
  if (h != HNone && selected.size() >= 2)
    {
      // a handle of the frame around several items: they are resized or turned together
      opBefore.clear();
      for (const tpl::Element &el : selectedElements())
        if (!el.locked) opBefore.append(el);
      opGroup = true;
      opGroupRect = groupFrame();
      opHandle = h;
      if (h == HRotate)
        {
          op = OpRotate;
          opAnchorScene = opGroupRect.center();
          opStartAngle = qRadiansToDegrees(qAtan2(scenePt.y() - opAnchorScene.y(), scenePt.x() - opAnchorScene.x()));
        }
      else
        {
          op = OpResize;
          opAnchorScene = localPoint(opGroupRect, (h + 4) % 8);
          collectSnapLines();
        }
      return;
    }
  opGroup = false;
  if (h != HNone)
    {
      opBefore = selectedElements();
      const tpl::Element &s = opBefore.first();
      opHandle = h;
      if (h == HRotate)
        {
          op = OpRotate;
          opAnchorScene = s.pos + s.rect.center();
          opStartAngle = qRadiansToDegrees(qAtan2(scenePt.y() - opAnchorScene.y(), scenePt.x() - opAnchorScene.x()));
        }
      else
        {
          op = OpResize;
          const Handle opp = Handle((h + 4) % 8);
          const QRectF lb = localBoundsOf(s);
          opAnchorLocal = localPoint(lb, opp);
          opAnchorScene = tpl::sceneTransform(s).map(opAnchorLocal);
          collectSnapLines();
        }
      return;
    }

  const QString hit = hitTest(scenePt);
  if (!hit.isEmpty())
    {
      opPressedId = hit;
      if (mods & (Qt::ShiftModifier | Qt::ControlModifier))
        {
          QSet<QString> s = selected;
          if (s.contains(hit)) s.remove(hit);
          else s.insert(hit);
          setSelectionInternal(s);
          opShiftToggled = true;
          if (!selected.contains(hit)) return;
        }
      else if (!selected.contains(hit))
        {
          setSelection(QStringList(hit));
        }
      beginMove(scenePt);
      return;
    }

  // empty space: rubber band
  op = OpBand;
  createRect = QRectF(scenePt, QSizeF(0, 0));
  opBefore.clear();
  if (!(mods & (Qt::ShiftModifier | Qt::ControlModifier))) clearSelection();
  opBefore = selectedElements();   // what a band with Shift/Ctrl adds to
}

void EditorCanvas::beginMove(const QPointF &scenePt)
{
  opBefore.clear();
  for (const tpl::Element &e : selectedElements())
    if (!e.locked) opBefore.append(e);
  if (opBefore.isEmpty())
    {
      op = OpNone;
      return;
    }
  op = OpMove;
  opStartScene = scenePt;
  collectSnapLines();
}

void EditorCanvas::mouseMoveEvent(QMouseEvent *e)
{
  const QPointF scenePt = mapToScene(e->pos());
  emit cursorMoved(scenePt, doc->doc().canvas.contains(scenePt));
  switch (op)
    {
    case OpPan:
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (e->pos().x() - panLast.x()));
      verticalScrollBar()->setValue(verticalScrollBar()->value() - (e->pos().y() - panLast.y()));
      panLast = e->pos();
      break;
    case OpMove:
      if (!opMoved && (e->pos() - opStartView).manhattanLength() < clickSlopPx) break;
      if (opShiftToggled && !opMoved) break;
      opMoved = true;
      updateMove(scenePt, e->modifiers());
      break;
    case OpResize:
      opMoved = true;
      if (opGroup) updateGroupResize(scenePt, e->modifiers());
      else updateResize(scenePt, e->modifiers());
      break;
    case OpRotate:
      opMoved = true;
      if (opGroup) updateGroupRotate(scenePt, e->modifiers());
      else updateRotate(scenePt, e->modifiers());
      break;
    case OpBand:
      {
        createRect = QRectF(opStartScene, scenePt).normalized();
        QSet<QString> s;
        for (const tpl::Element &b : opBefore) s.insert(b.id);
        for (const tpl::Element &el : doc->doc().items)
          {
            if (el.locked) continue;
            if (!QPolygonF(createRect).intersected(outlineOf(el)).isEmpty()) s.insert(el.id);
          }
        setSelectionInternal(s);
        viewport()->update();
        break;
      }
    case OpCreate:
      createRect = QRectF(opStartScene, snapPoint(scenePt, e->modifiers())).normalized();
      viewport()->update();
      break;
    case OpNone:
      updateCursor(e->pos());
      break;
    }
}

void EditorCanvas::mouseReleaseEvent(QMouseEvent *e)
{
  if (e->button() == Qt::MiddleButton || op == OpPan)
    {
      if (op == OpPan) op = OpNone;
      updateCursor(e->pos());
      return;
    }
  if (e->button() != Qt::LeftButton) return;
  switch (op)
    {
    case OpMove:
      finishOp(tr("Move"));
      break;
    case OpResize:
      finishOp(tr("Resize"));
      break;
    case OpRotate:
      finishOp(tr("Rotate"));
      break;
    case OpCreate:
      {
        const QRectF r = createRect;
        op = OpNone;
        createShape(r);
        break;
      }
    case OpBand:
      op = OpNone;
      viewport()->update();
      break;
    default:
      break;
    }
  op = OpNone;
  guideXs.clear();
  guideYs.clear();
  viewport()->update();
  updateCursor(e->pos());
}

void EditorCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton || currentTool != ToolSelect) return;
  const QString hit = hitTest(mapToScene(e->pos()));
  if (hit.isEmpty()) return;
  const tpl::Element *el = doc->find(hit);
  if (el && el->type == tpl::ItemType::Text) editText(hit);
}

void EditorCanvas::updateMove(const QPointF &scenePt, Qt::KeyboardModifiers mods)
{
  QPointF delta = scenePt - opStartScene;
  if (mods & Qt::ShiftModifier)
    {
      if (qAbs(delta.x()) >= qAbs(delta.y())) delta.setY(0);
      else delta.setX(0);
    }
  guideXs.clear();
  guideYs.clear();
  if (!(mods & Qt::AltModifier))
    {
      QRectF bb;
      for (const tpl::Element &el : opBefore) bb = unite(bb, sceneBounds(el));
      bb.translate(delta);
      const double xe[3] = {bb.left(), bb.center().x(), bb.right()};
      const double ye[3] = {bb.top(), bb.center().y(), bb.bottom()};
      double ax = snapDelta(snapXs, xe, guideXs);
      double ay = snapDelta(snapYs, ye, guideYs);
      if (snapGrid)
        {
          const QRectF c = doc->doc().canvas;
          if (guideXs.isEmpty()) ax = c.left() + qRound((bb.left() - c.left()) / gridPx) * double(gridPx) - bb.left();
          if (guideYs.isEmpty()) ay = c.top() + qRound((bb.top() - c.top()) / gridPx) * double(gridPx) - bb.top();
        }
      delta += QPointF(ax, ay);
    }
  for (const tpl::Element &b : opBefore)
    {
      tpl::Element el = b;
      el.pos += delta;
      doc->replaceRaw(el);
    }
  viewport()->update();
}

void EditorCanvas::updateResize(const QPointF &scenePt, Qt::KeyboardModifiers mods)
{
  if (opBefore.isEmpty()) return;
  const tpl::Element &s = opBefore.first();
  tpl::Element e = s;
  const bool corner = opHandle == HTopLeft || opHandle == HTopRight || opHandle == HBottomRight || opHandle == HBottomLeft;
  const Handle opp = Handle((opHandle + 4) % 8);

  if (s.type == tpl::ItemType::Text && !s.textBox)
    {
      // dragging a corner scales the text about the opposite corner
      const QPointF startHandle = tpl::sceneTransform(s).map(localPoint(localBoundsOf(s), opHandle));
      const double d0 = QLineF(opAnchorScene, startHandle).length();
      const double d1 = QLineF(opAnchorScene, scenePt).length();
      if (d0 < 1) return;
      e.font.pointSize = qBound(4.0, s.font.pointSize * (d1 / d0), 1000.0);
      EditorDocument::fitTextRect(e);
      const QPointF now = tpl::sceneTransform(e).map(localPoint(localBoundsOf(e), opp));
      e.pos += opAnchorScene - now;
      doc->replaceRaw(e);
      viewport()->update();
      return;
    }

  QPointF lp = tpl::sceneTransform(s).inverted().map(scenePt);
  const QRectF lb = localBoundsOf(s);
  double l = lb.left(), t = lb.top(), r = lb.right(), b = lb.bottom();
  const bool hasLeft = opHandle == HTopLeft || opHandle == HLeft || opHandle == HBottomLeft;
  const bool hasRight = opHandle == HTopRight || opHandle == HRight || opHandle == HBottomRight;
  const bool hasTop = opHandle == HTopLeft || opHandle == HTop || opHandle == HTopRight;
  const bool hasBottom = opHandle == HBottomLeft || opHandle == HBottom || opHandle == HBottomRight;
  guideXs.clear();
  guideYs.clear();
  if (!(mods & Qt::AltModifier) && s.rotation == 0 && s.skewH == 0 && s.skewV == 0)
    {
      // the edge being dragged meets the canvas, other items or the grid (only for an item that is not turned: its edges are then horizontal and vertical)
      const QPointF sp = s.pos + lp;
      const QRectF c = doc->doc().canvas;
      if (hasLeft || hasRight)
        {
          double d = snapEdge(snapXs, sp.x(), guideXs);
          if (guideXs.isEmpty() && snapGrid) d = c.left() + qRound((sp.x() - c.left()) / gridPx) * double(gridPx) - sp.x();
          lp.rx() += d;
        }
      if (hasTop || hasBottom)
        {
          double d = snapEdge(snapYs, sp.y(), guideYs);
          if (guideYs.isEmpty() && snapGrid) d = c.top() + qRound((sp.y() - c.top()) / gridPx) * double(gridPx) - sp.y();
          lp.ry() += d;
        }
    }
  if (hasLeft) l = qMin(lp.x(), r - 1);
  if (hasRight) r = qMax(lp.x(), l + 1);
  if (hasTop) t = qMin(lp.y(), b - 1);
  if (hasBottom) b = qMax(lp.y(), t + 1);

  const bool lockAspect = corner && ((s.type == tpl::ItemType::Image) != bool(mods & Qt::ShiftModifier));
  if (lockAspect && lb.width() > 0 && lb.height() > 0)
    {
      const double k = qMax((r - l) / lb.width(), (b - t) / lb.height());
      const double w = lb.width() * k, h = lb.height() * k;
      if (hasLeft) l = lb.right() - w;
      else r = lb.left() + w;
      if (hasTop) t = lb.bottom() - h;
      else b = lb.top() + h;
      if (hasLeft) r = lb.right();
      else l = lb.left();
      if (hasTop) b = lb.bottom();
      else t = lb.top();
    }
  QRectF nr(l, t, r - l, b - t);
  if (s.type == tpl::ItemType::Line) nr = QRectF(l, s.rect.y(), r - l, s.rect.height());
  EditorDocument::setRectKeeping(e, nr, opAnchorLocal);
  doc->replaceRaw(e);
  viewport()->update();
}

void EditorCanvas::updateRotate(const QPointF &scenePt, Qt::KeyboardModifiers mods)
{
  if (opBefore.isEmpty()) return;
  const tpl::Element &s = opBefore.first();
  const double now = qRadiansToDegrees(qAtan2(scenePt.y() - opAnchorScene.y(), scenePt.x() - opAnchorScene.x()));
  double rot = s.rotation + (now - opStartAngle);
  if (mods & Qt::ShiftModifier) rot = qRound(rot / 15.0) * 15.0;
  while (rot > 180) rot -= 360;
  while (rot <= -180) rot += 360;
  tpl::Element e = s;
  e.rotation = rot;
  doc->replaceRaw(e);
  viewport()->update();
}

void EditorCanvas::updateGroupResize(const QPointF &scenePt, Qt::KeyboardModifiers mods)
{
  if (opBefore.isEmpty()) return;
  const QPointF startHandle = localPoint(opGroupRect, opHandle);
  const bool hasX = opHandle != HTop && opHandle != HBottom;
  const bool hasY = opHandle != HLeft && opHandle != HRight;
  QPointF t = scenePt;
  guideXs.clear();
  guideYs.clear();
  if (!(mods & Qt::AltModifier))
    {
      const QRectF c = doc->doc().canvas;
      if (hasX)
        {
          double d = snapEdge(snapXs, t.x(), guideXs);
          if (guideXs.isEmpty() && snapGrid) d = c.left() + qRound((t.x() - c.left()) / gridPx) * double(gridPx) - t.x();
          t.rx() += d;
        }
      if (hasY)
        {
          double d = snapEdge(snapYs, t.y(), guideYs);
          if (guideYs.isEmpty() && snapGrid) d = c.top() + qRound((t.y() - c.top()) / gridPx) * double(gridPx) - t.y();
          t.ry() += d;
        }
    }
  double sx = 1, sy = 1;
  const double dx = startHandle.x() - opAnchorScene.x(), dy = startHandle.y() - opAnchorScene.y();
  if (hasX && qAbs(dx) > 1e-6) sx = (t.x() - opAnchorScene.x()) / dx;
  if (hasY && qAbs(dy) > 1e-6) sy = (t.y() - opAnchorScene.y()) / dy;
  sx = qMax(sx, 0.02);
  sy = qMax(sy, 0.02);
  const bool corner = hasX && hasY;
  if (corner && (mods & Qt::ShiftModifier)) sx = sy = qMax(sx, sy);   // keep the proportions of the group
  for (const tpl::Element &b : opBefore)
    {
      tpl::Element el = b;
      EditorDocument::scaleElement(el, opAnchorScene, sx, sy);
      EditorDocument::fitTextRect(el);
      doc->replaceRaw(el);
    }
  viewport()->update();
}

void EditorCanvas::updateGroupRotate(const QPointF &scenePt, Qt::KeyboardModifiers mods)
{
  if (opBefore.isEmpty()) return;
  double delta = qRadiansToDegrees(qAtan2(scenePt.y() - opAnchorScene.y(), scenePt.x() - opAnchorScene.x())) - opStartAngle;
  if (mods & Qt::ShiftModifier) delta = qRound(delta / 15.0) * 15.0;
  QTransform turn;
  turn.translate(opAnchorScene.x(), opAnchorScene.y());
  turn.rotate(delta);
  turn.translate(-opAnchorScene.x(), -opAnchorScene.y());
  for (const tpl::Element &b : opBefore)
    {
      tpl::Element el = b;
      // every item turns about its own center by the same angle, and the centers move about the middle of the group
      const QPointF center = b.pos + b.rect.center();
      const QPointF moved = turn.map(center);
      double rot = b.rotation + delta;
      while (rot > 180) rot -= 360;
      while (rot <= -180) rot += 360;
      el.rotation = rot;
      el.pos = b.pos + (moved - center);
      doc->replaceRaw(el);
    }
  viewport()->update();
}

void EditorCanvas::finishOp(const QString &undoText)
{
  QStringList ids;
  for (const tpl::Element &b : opBefore) ids << b.id;
  const QVector<tpl::Element> after = doc->elements(ids);
  const QVector<tpl::Element> before = opBefore;
  op = OpNone;
  guideXs.clear();
  guideYs.clear();
  if (opMoved) doc->commitModify(before, after, undoText);
  else
    for (const tpl::Element &b : before) doc->replaceRaw(b);
  opBefore.clear();
  viewport()->update();
}

void EditorCanvas::cancelOp()
{
  if (op == OpMove || op == OpResize || op == OpRotate)
    for (const tpl::Element &b : opBefore) doc->replaceRaw(b);
  op = OpNone;
  guideXs.clear();
  guideYs.clear();
  opBefore.clear();
  viewport()->update();
}

void EditorCanvas::updateCursor(const QPoint &viewPt)
{
  if (spaceDown)
    {
      viewport()->setCursor(Qt::OpenHandCursor);
      return;
    }
  if (currentTool != ToolSelect)
    {
      viewport()->setCursor(Qt::CrossCursor);
      return;
    }
  const Handle h = handleAt(viewPt);
  if (h == HRotate)
    {
      viewport()->setCursor(Qt::PointingHandCursor);
      return;
    }
  if (h != HNone)
    {
      QPointF c, d;
      if (selected.size() >= 2)
        {
          c = toView(groupFrame().center());
          d = groupHandleViewPoint(h) - c;
        }
      else
        {
          const tpl::Element *e = doc->find(*selected.begin());
          c = toView(tpl::sceneTransform(*e).map(localBoundsOf(*e).center()));
          d = handleViewPoint(*e, h) - c;
        }
      double a = qRadiansToDegrees(qAtan2(d.y(), d.x()));
      if (a < 0) a += 180;
      if (a >= 180) a -= 180;
      Qt::CursorShape shape = Qt::SizeHorCursor;
      if (a >= 22.5 && a < 67.5) shape = Qt::SizeFDiagCursor;
      else if (a >= 67.5 && a < 112.5) shape = Qt::SizeVerCursor;
      else if (a >= 112.5 && a < 157.5) shape = Qt::SizeBDiagCursor;
      viewport()->setCursor(shape);
      return;
    }
  const QString hit = hitTest(mapToScene(viewPt));
  viewport()->setCursor(!hit.isEmpty() && selected.contains(hit) ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void EditorCanvas::focusOutEvent(QFocusEvent *e)
{
  QGraphicsView::focusOutEvent(e);
  if (spaceDown)
    {
      // the key release may go elsewhere: do not stay in pan mode
      spaceDown = false;
      updateCursor(mapFromGlobal(QCursor::pos()));
    }
}

void EditorCanvas::leaveEvent(QEvent *e)
{
  QGraphicsView::leaveEvent(e);
  emit cursorMoved(QPointF(), false);
}

void EditorCanvas::wheelEvent(QWheelEvent *e)
{
  if (e->modifiers() & Qt::ControlModifier)
    {
      const double steps = e->angleDelta().y() / 120.0;
      if (steps != 0) setZoom(zoomFactor * qPow(1.15, steps), e->position());
      e->accept();
      return;
    }
  QGraphicsView::wheelEvent(e);
}

void EditorCanvas::keyPressEvent(QKeyEvent *e)
{
  const bool shift = e->modifiers() & Qt::ShiftModifier;
  switch (e->key())
    {
    case Qt::Key_Space:
      if (!e->isAutoRepeat())
        {
          spaceDown = true;
          viewport()->setCursor(Qt::OpenHandCursor);
        }
      return;
    case Qt::Key_Left: nudge(shift ? -10 : -1, 0); return;
    case Qt::Key_Right: nudge(shift ? 10 : 1, 0); return;
    case Qt::Key_Up: nudge(0, shift ? -10 : -1); return;
    case Qt::Key_Down: nudge(0, shift ? 10 : 1); return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
      deleteSelection();
      return;
    case Qt::Key_Escape:
      if (op != OpNone) cancelOp();
      else if (currentTool != ToolSelect) setTool(ToolSelect);
      else clearSelection();
      return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
      if (selected.size() == 1)
        {
          const tpl::Element *el = doc->find(*selected.begin());
          if (el && el->type == tpl::ItemType::Text && !el->locked) editText(el->id);
        }
      return;
    default:
      break;
    }
  QGraphicsView::keyPressEvent(e);
}

void EditorCanvas::keyReleaseEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat())
    {
      spaceDown = false;
      updateCursor(mapFromGlobal(QCursor::pos()));
      return;
    }
  QGraphicsView::keyReleaseEvent(e);
}

void EditorCanvas::contextMenuEvent(QContextMenuEvent *e)
{
  // an unlocked item wins, as with a click; a locked one can only be reached this way, to unlock it
  const QPointF scenePt = mapToScene(e->pos());
  QString hit = hitTest(scenePt);
  if (hit.isEmpty()) hit = hitTest(scenePt, true);
  if (!hit.isEmpty() && !selected.contains(hit)) setSelection(QStringList(hit));
  emit contextMenuRequested(e->globalPos());
}

void EditorCanvas::setTool(Tool t)
{
  if (t == currentTool) return;
  currentTool = t;
  if (t != ToolSelect) clearSelection();
  updateCursor(mapFromGlobal(QCursor::pos()));
  emit toolChanged(int(t));
}

// --- creating items --------------------------------------------------------------------------------------------
tpl::Element EditorCanvas::defaultElement(tpl::ItemType type)
{
  tpl::Element e;
  e.type = type;
  switch (type)
    {
    case tpl::ItemType::Text:
      e.text = QStringLiteral("Text");
      e.fill = Qt::white;
      e.outline.color = Qt::black;
      e.outline.width = 2;
      e.outline.join = Qt::RoundJoin;
      e.font.family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
      e.font.pointSize = 24;
      e.font.weight = 700;
      break;
    case tpl::ItemType::Rect:
      e.fill = QColor(0, 0, 0, 150);
      e.outline.style = Qt::NoPen;
      break;
    case tpl::ItemType::Ellipse:
      e.fill = QColor(255, 255, 255, 120);
      e.outline.color = Qt::white;
      e.outline.width = 2;
      break;
    case tpl::ItemType::Line:
      e.outline.color = Qt::white;
      e.outline.width = 3;
      e.rect = QRectF(0, 0, 100, 2);
      break;
    case tpl::ItemType::Image:
      e.outline.style = Qt::NoPen;
      break;
    case tpl::ItemType::Replay:
      e.outline.color = Qt::white;
      e.outline.width = 2;
      break;
    }
  return e;
}

void EditorCanvas::anchorNew(tpl::Element &e) const
{
  if (doc->doc().layout == tpl::TemplateDocument::Anchored) tpl::autoAnchor(e, doc->doc().canvas);
}

void EditorCanvas::createShape(const QRectF &rIn)
{
  const Tool t = currentTool;
  setTool(ToolSelect);
  const QRectF c = doc->doc().canvas;
  QRectF r = rIn;
  const bool click = r.width() < clickSlopPx / zoomFactor && r.height() < clickSlopPx / zoomFactor;
  if (t == ToolText)
    {
      if (click)
        {
          addTextAt(rIn.topLeft(), QStringLiteral("Text"));
          return;
        }
      // dragging out a rectangle with the Text tool makes a text box: the text is laid out inside it and shrinks to fit
      tpl::Element tb = defaultElement(tpl::ItemType::Text);
      tb.text = QStringLiteral("Text");
      tb.textBox = true;
      tb.shrinkToFit = true;
      tb.wrap = true;
      tb.valign = tpl::TextVAlign::Middle;
      tb.align = tpl::TextAlign::Center;
      tb.rect = QRectF(0, 0, qMax(r.width(), 20.0), qMax(r.height(), 14.0));
      tb.pos = r.topLeft();
      anchorNew(tb);
      const QStringList ids = doc->addItems(QVector<tpl::Element>() << tb, tr("Add text box"));
      if (!ids.isEmpty()) editText(ids.first(), true);
      return;
    }
  tpl::ItemType type = tpl::ItemType::Rect;
  QSizeF def(c.width() * 0.4, c.height() * 0.12);
  switch (t)
    {
    case ToolEllipse: type = tpl::ItemType::Ellipse; def = QSizeF(c.height() * 0.25, c.height() * 0.25); break;
    case ToolLine: type = tpl::ItemType::Line; def = QSizeF(c.width() * 0.4, 2); break;
    case ToolReplay: type = tpl::ItemType::Replay; def = QSizeF(c.width() * 0.3, c.width() * 0.3 * 0.75); break;
    default: break;
    }
  if (click) r = QRectF(rIn.topLeft() - QPointF(def.width() / 2, def.height() / 2), def);
  tpl::Element e = defaultElement(type);
  if (type == tpl::ItemType::Line)
    {
      // a line is drawn along the middle of its rectangle: use the drag's width and put it at the drag's middle
      const double w = click ? def.width() : qMax(r.width(), 4.0);
      const double cy = click ? rIn.topLeft().y() : r.center().y();
      e.rect = QRectF(0, 0, w, 2);
      e.pos = QPointF(click ? rIn.topLeft().x() - w / 2 : r.left(), cy - 1);
    }
  else
    {
      e.rect = QRectF(0, 0, qMax(r.width(), 4.0), qMax(r.height(), 4.0));
      e.pos = r.topLeft();
    }
  anchorNew(e);
  doc->addItems(QVector<tpl::Element>() << e, tr("Add %1").arg(describe(e).toLower()));
}

QString EditorCanvas::addTextAt(const QPointF &pos, const QString &text)
{
  tpl::Element e = defaultElement(tpl::ItemType::Text);
  e.text = text;
  // the text is drawn from its baseline: lower it by the text height so the click point is the top-left corner
  tpl::RenderOptions literal;
  literal.expandMacros = false;
  const QRectF tb = tpl::textBounds(e, literal);
  e.pos = pos - tb.topLeft();
  anchorNew(e);
  const QStringList ids = doc->addItems(QVector<tpl::Element>() << e, tr("Add text"));
  if (ids.isEmpty()) return QString();
  editText(ids.first(), true);
  return ids.first();
}

void EditorCanvas::addMacro(const QString &macro)
{
  const QRectF c = doc->doc().canvas;
  tpl::Element e = defaultElement(tpl::ItemType::Text);
  e.text = macro;
  tpl::RenderOptions literal;
  literal.expandMacros = false;
  const QRectF tb = tpl::textBounds(e, literal);
  e.pos = QPointF(c.center().x() - tb.width() / 2, c.center().y()) - tb.topLeft() - QPointF(0, tb.height() / 2);
  anchorNew(e);
  doc->addItems(QVector<tpl::Element>() << e, tr("Add text"));
}

QImage EditorCanvas::loadPicture(const QString &fileName, QString *error)
{
  QImageReader reader(fileName);
  reader.setAutoTransform(true);   // EXIF orientation
  QImage img = reader.read();
  if (img.isNull())
    {
      if (error) *error = tr("Cannot read %1: %2").arg(QFileInfo(fileName).fileName(), reader.errorString());
      return QImage();
    }
  if (img.width() > maxImportSide || img.height() > maxImportSide)
    img = img.scaled(maxImportSide, maxImportSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  return img.convertToFormat(QImage::Format_ARGB32);
}

bool EditorCanvas::insertImageFile(const QString &fileName, const QPointF &center)
{
  QString error;
  const QImage img = loadPicture(fileName, &error);
  if (img.isNull())
    {
      emit statusMessage(error);
      return false;
    }
  return insertImage(img, center);
}

bool EditorCanvas::insertImage(const QImage &imgIn, const QPointF &centerIn)
{
  if (imgIn.isNull()) return false;
  QImage img = imgIn;
  if (img.width() > maxImportSide || img.height() > maxImportSide)
    img = img.scaled(maxImportSide, maxImportSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  img = img.convertToFormat(QImage::Format_ARGB32);
  const QRectF c = doc->doc().canvas;
  // natural size, but no more than half the canvas, aspect kept
  const double k = qMin(1.0, qMin(c.width() * 0.5 / img.width(), c.height() * 0.5 / img.height()));
  const double w = qMax(4.0, img.width() * k), h = qMax(4.0, img.height() * k);
  const QPointF center = qIsNaN(centerIn.x()) ? c.center() : centerIn;
  tpl::Element e = defaultElement(tpl::ItemType::Image);
  e.image = img;
  e.rect = QRectF(0, 0, w, h);
  e.pos = center - QPointF(w / 2, h / 2);
  anchorNew(e);
  doc->addItems(QVector<tpl::Element>() << e, tr("Add image"));
  return true;
}

void EditorCanvas::commitTextEdit()
{
  if (textEdit) textEdit->clearFocus();   // the editor commits when it loses the focus
}

void EditorCanvas::editText(const QString &id, bool isNew)
{
  const tpl::Element *el = doc->find(id);
  if (!el || el->type != tpl::ItemType::Text) return;
  if (textEdit) textEdit->clearFocus();
  const QRectF vb = viewportTransform().map(outlineOf(*el)).boundingRect();
  InlineTextEdit *ed = new InlineTextEdit(viewport());
  QFont f = el->font.toFont();
  f.setPixelSize(qBound(10, int(el->font.pointSize * 96.0 / 72.0 * zoomFactor), 72));
  ed->setFont(f);
  QPalette pal = ed->palette();
  const bool light = el->fill.lightness() > 128;
  pal.setColor(QPalette::Base, light ? QColor(0x30, 0x30, 0x30) : QColor(0xf0, 0xf0, 0xf0));
  pal.setColor(QPalette::Text, light ? QColor(0xf0, 0xf0, 0xf0) : QColor(0x20, 0x20, 0x20));
  ed->setPalette(pal);
  ed->setLineWrapMode(el->textBox && el->wrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
  ed->setPlainText(el->text);
  ed->setToolTip(tr("Enter: new line. Ctrl+Enter or click outside: done. Esc: cancel."));
  ed->setGeometry(QRectF(vb.left() - 4, vb.top() - 4, qMax(140.0, vb.width() + 28), qMax(38.0, vb.height() + 16)).toRect());
  ed->selectAll();
  const int undoIndex = doc->undoStack()->index();
  ed->onCommit = [this, id, isNew, undoIndex](const QString &text) {
    const tpl::Element *cur = doc->find(id);
    if (!cur) return;
    if (isNew && text.trimmed().isEmpty() && doc->undoStack()->index() == undoIndex)
      {
        // a text item that was just created and left empty: take the creation back
        doc->undoStack()->undo();
        return;
      }
    if (text == cur->text) return;
    tpl::Element n = *cur;
    n.text = text;
    doc->modifyItems(QVector<tpl::Element>() << n, tr("Edit text"));
  };
  textEdit = ed;
  ed->show();
  ed->setFocus();
}

// --- clipboard ---------------------------------------------------------------------------------------------------
QString EditorCanvas::mimeType() const
{
  return QStringLiteral("application/x-qsstv-template-items");
}

void EditorCanvas::copySelection()
{
  const QVector<tpl::Element> sel = selectedElements();
  if (sel.isEmpty()) return;
  tpl::TemplateDocument tmp;
  tmp.canvas = doc->doc().canvas;
  tmp.items = sel;
  QMimeData *md = new QMimeData;
  md->setData(mimeType(), tpl::toJson(tmp));
  QStringList texts;
  for (const tpl::Element &e : sel)
    if (e.type == tpl::ItemType::Text) texts << e.text;
  if (!texts.isEmpty()) md->setText(texts.join(QLatin1Char('\n')));
  QApplication::clipboard()->setMimeData(md);
  pasteCount = 0;
}

void EditorCanvas::cutSelection()
{
  copySelection();
  const QStringList ids = selection();
  if (!ids.isEmpty()) doc->removeItems(ids, tr("Cut"));
}

bool EditorCanvas::canPaste() const
{
  const QMimeData *md = QApplication::clipboard()->mimeData();
  return md && (md->hasFormat(mimeType()) || md->hasImage() || md->hasText());
}

void EditorCanvas::insertElements(QVector<tpl::Element> els, const QPointF &offset, const QString &text)
{
  for (tpl::Element &e : els) e.pos += offset;
  doc->addItems(els, text);
}

void EditorCanvas::paste()
{
  const QMimeData *md = QApplication::clipboard()->mimeData();
  if (!md) return;
  if (md->hasFormat(mimeType()))
    {
      const tpl::LoadResult r = tpl::loadJson(md->data(mimeType()));
      if (r.ok && !r.doc.items.isEmpty())
        {
          pasteCount++;
          insertElements(r.doc.items, QPointF(12, 12) * pasteCount, tr("Paste"));
          return;
        }
    }
  if (md->hasImage())
    {
      const QImage img = qvariant_cast<QImage>(md->imageData());
      if (insertImage(img)) return;
    }
  if (md->hasText())
    {
      const QString t = md->text().left(tpl::limits::maxTextLength);
      if (!t.trimmed().isEmpty())
        {
          const QRectF c = doc->doc().canvas;
          tpl::Element e = defaultElement(tpl::ItemType::Text);
          e.text = t;
          tpl::RenderOptions literal;
          literal.expandMacros = false;
          const QRectF tb = tpl::textBounds(e, literal);
          e.pos = c.center() - tb.center();
          doc->addItems(QVector<tpl::Element>() << e, tr("Paste text"));
        }
    }
}

void EditorCanvas::duplicateSelection()
{
  const QVector<tpl::Element> sel = selectedElements();
  if (sel.isEmpty()) return;
  insertElements(sel, QPointF(10, 10), tr("Duplicate"));
}

void EditorCanvas::deleteSelection()
{
  const QStringList ids = selection();
  if (!ids.isEmpty()) doc->removeItems(ids, tr("Delete"));
}

// --- arranging ---------------------------------------------------------------------------------------------------
void EditorCanvas::nudge(double dx, double dy)
{
  QVector<tpl::Element> after;
  for (tpl::Element e : selectedElements())
    {
      if (e.locked) continue;
      e.pos += QPointF(dx, dy);
      after.append(e);
    }
  if (!after.isEmpty()) doc->modifyItems(after, tr("Move"), QStringLiteral("nudge"));
}

void EditorCanvas::align(AlignMode mode)
{
  QVector<tpl::Element> sel = selectedElements();
  if (sel.isEmpty()) return;
  QRectF ref = doc->doc().canvas;
  if (sel.size() > 1) ref = selectionBounds();
  QVector<tpl::Element> after;
  for (tpl::Element e : sel)
    {
      if (e.locked) continue;
      const QRectF b = sceneBounds(e);
      QPointF d;
      switch (mode)
        {
        case AlignLeft: d.setX(ref.left() - b.left()); break;
        case AlignHCenter: d.setX(ref.center().x() - b.center().x()); break;
        case AlignRight: d.setX(ref.right() - b.right()); break;
        case AlignTop: d.setY(ref.top() - b.top()); break;
        case AlignVCenter: d.setY(ref.center().y() - b.center().y()); break;
        case AlignBottom: d.setY(ref.bottom() - b.bottom()); break;
        }
      e.pos += d;
      after.append(e);
    }
  if (!after.isEmpty()) doc->modifyItems(after, tr("Align"));
}

void EditorCanvas::distribute(Qt::Orientation o)
{
  QVector<tpl::Element> sel;
  for (const tpl::Element &e : selectedElements())
    if (!e.locked) sel.append(e);
  if (sel.size() < 3) return;
  const bool horiz = o == Qt::Horizontal;
  std::sort(sel.begin(), sel.end(), [&](const tpl::Element &a, const tpl::Element &b) {
    const QRectF ra = sceneBounds(a), rb = sceneBounds(b);
    return horiz ? ra.left() < rb.left() : ra.top() < rb.top();
  });
  double first = horiz ? sceneBounds(sel.first()).left() : sceneBounds(sel.first()).top();
  double last = horiz ? sceneBounds(sel.last()).right() : sceneBounds(sel.last()).bottom();
  double sizes = 0;
  for (const tpl::Element &e : sel) sizes += horiz ? sceneBounds(e).width() : sceneBounds(e).height();
  const double gap = (last - first - sizes) / (sel.size() - 1);
  double at = first;
  QVector<tpl::Element> after;
  for (tpl::Element e : sel)
    {
      const QRectF b = sceneBounds(e);
      const double target = at;
      if (horiz) e.pos.rx() += target - b.left();
      else e.pos.ry() += target - b.top();
      at += (horiz ? b.width() : b.height()) + gap;
      after.append(e);
    }
  doc->modifyItems(after, tr("Distribute"));
}

namespace
{
QStringList reordered(const QStringList &order, const QSet<QString> &sel, int mode)
{
  QStringList r = order;
  switch (mode)
    {
    case 0:   // to front
      {
        QStringList others, mine;
        for (const QString &id : order) (sel.contains(id) ? mine : others) << id;
        return others + mine;
      }
    case 1:   // to back
      {
        QStringList others, mine;
        for (const QString &id : order) (sel.contains(id) ? mine : others) << id;
        return mine + others;
      }
    case 2:   // one step forward
      for (int i = r.size() - 2; i >= 0; i--)
        if (sel.contains(r[i]) && !sel.contains(r[i + 1])) r.swapItemsAt(i, i + 1);
      return r;
    default:  // one step backward
      for (int i = 1; i < r.size(); i++)
        if (sel.contains(r[i]) && !sel.contains(r[i - 1])) r.swapItemsAt(i, i - 1);
      return r;
    }
}
}  // namespace

void EditorCanvas::bringToFront()
{
  if (hasSelection()) doc->reorder(reordered(doc->ids(), selected, 0), tr("Bring to front"));
}

void EditorCanvas::sendToBack()
{
  if (hasSelection()) doc->reorder(reordered(doc->ids(), selected, 1), tr("Send to back"));
}

void EditorCanvas::bringForward()
{
  if (hasSelection()) doc->reorder(reordered(doc->ids(), selected, 2), tr("Bring forward"));
}

void EditorCanvas::sendBackward()
{
  if (hasSelection()) doc->reorder(reordered(doc->ids(), selected, 3), tr("Send backward"));
}

void EditorCanvas::toggleLock()
{
  QVector<tpl::Element> sel = selectedElements();
  if (sel.isEmpty()) return;
  bool anyUnlocked = false;
  for (const tpl::Element &e : sel) anyUnlocked |= !e.locked;
  for (tpl::Element &e : sel) e.locked = anyUnlocked;
  doc->modifyItems(sel, anyUnlocked ? tr("Lock") : tr("Unlock"));
}

// --- drag and drop of picture files -----------------------------------------------------------------------------
void EditorCanvas::dragEnterEvent(QDragEnterEvent *e)
{
  if (e->mimeData()->hasUrls() || e->mimeData()->hasImage()) e->acceptProposedAction();
}

void EditorCanvas::dragMoveEvent(QDragMoveEvent *e)
{
  e->acceptProposedAction();
}

void EditorCanvas::dropEvent(QDropEvent *e)
{
  const QPointF at = mapToScene(e->position().toPoint());
  bool any = false;
  if (e->mimeData()->hasUrls())
    {
      for (const QUrl &u : e->mimeData()->urls())
        if (u.isLocalFile()) any |= insertImageFile(u.toLocalFile(), at);
    }
  else if (e->mimeData()->hasImage())
    {
      any = insertImage(qvariant_cast<QImage>(e->mimeData()->imageData()), at);
    }
  if (any) e->acceptProposedAction();
}
