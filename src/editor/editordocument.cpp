#include "editordocument.h"

#include <cmath>

#include <QSet>
#include "editorcommands.h"

namespace
{

const char *typePrefix(tpl::ItemType t)
{
  switch (t)
    {
    case tpl::ItemType::Text: return "text";
    case tpl::ItemType::Rect: return "rect";
    case tpl::ItemType::Ellipse: return "ellipse";
    case tpl::ItemType::Line: return "line";
    case tpl::ItemType::Image: return "image";
    case tpl::ItemType::Replay: return "replay";
    }
  return "item";
}

bool sameGradient(const tpl::Gradient &a, const tpl::Gradient &b)
{
  if (a.kind != b.kind || a.angle != b.angle || a.stops.size() != b.stops.size()) return false;
  for (int i = 0; i < a.stops.size(); i++)
    if (a.stops[i].pos != b.stops[i].pos || a.stops[i].color != b.stops[i].color) return false;
  return true;
}

bool sameElement(const tpl::Element &a, const tpl::Element &b)
{
  return a.type == b.type && a.id == b.id && a.pos == b.pos && a.rect == b.rect && a.rotation == b.rotation
         && a.skewH == b.skewH && a.skewV == b.skewV && a.opacity == b.opacity && a.locked == b.locked && a.fill == b.fill
         && sameGradient(a.gradient, b.gradient) && a.outline.color == b.outline.color && a.outline.width == b.outline.width
         && a.outline.join == b.outline.join && a.outline.cap == b.outline.cap && a.outline.style == b.outline.style
         && a.outline.miterLimit == b.outline.miterLimit && a.text == b.text && a.font.family == b.font.family
         && a.font.pointSize == b.font.pointSize && a.font.weight == b.font.weight && a.font.italic == b.font.italic
         && a.font.underline == b.font.underline && a.font.strikeOut == b.font.strikeOut && a.font.stretch == b.font.stretch
         && a.font.letterSpacing == b.font.letterSpacing && (a.image.cacheKey() == b.image.cacheKey() || a.image == b.image)
         && a.anchorH == b.anchorH && a.anchorV == b.anchorV && a.radius == b.radius && a.shadow.on == b.shadow.on
         && a.shadow.color == b.shadow.color && a.shadow.dx == b.shadow.dx && a.shadow.dy == b.shadow.dy && a.shadow.blur == b.shadow.blur
         && a.textBox == b.textBox && a.align == b.align && a.valign == b.valign && a.wrap == b.wrap && a.shrinkToFit == b.shrinkToFit;
}

bool sameElements(const QVector<tpl::Element> &a, const QVector<tpl::Element> &b)
{
  if (a.size() != b.size()) return false;
  for (int i = 0; i < a.size(); i++)
    if (!sameElement(a[i], b[i])) return false;
  return true;
}

}  // namespace

EditorDocument::EditorDocument(QObject *parent) : QObject(parent)
{
  stack.setUndoLimit(200);   // commands hold their pictures: do not keep an unbounded history
}

int EditorDocument::indexOf(const QString &id) const
{
  for (int i = 0; i < d.items.size(); i++)
    if (d.items[i].id == id) return i;
  return -1;
}

const tpl::Element *EditorDocument::find(const QString &id) const
{
  const int i = indexOf(id);
  return i < 0 ? nullptr : &d.items[i];
}

QStringList EditorDocument::ids() const
{
  QStringList l;
  for (const tpl::Element &e : d.items) l.append(e.id);
  return l;
}

QVector<tpl::Element> EditorDocument::elements(const QStringList &want) const
{
  QVector<tpl::Element> out;
  for (const tpl::Element &e : d.items)
    if (want.contains(e.id)) out.append(e);
  return out;
}

QString EditorDocument::newId(tpl::ItemType type) const
{
  const QString prefix = QString::fromLatin1(typePrefix(type));
  QSet<QString> used;
  for (const tpl::Element &e : d.items) used.insert(e.id);
  for (int n = 1;; n++)
    {
      const QString id = QStringLiteral("%1-%2").arg(prefix).arg(n);
      if (!used.contains(id)) return id;
    }
}

void EditorDocument::setRectKeeping(tpl::Element &e, const QRectF &newRect, const QPointF &keep)
{
  const QPointF before = tpl::sceneTransform(e).map(keep);
  e.rect = newRect;
  const QPointF after = tpl::sceneTransform(e).map(keep);
  e.pos += before - after;
}

void EditorDocument::scaleElement(tpl::Element &e, const QPointF &origin, double sx, double sy)
{
  const double s = std::sqrt(sx * sy);
  // free text is re-fitted to its letters afterwards; every other rectangle (a text box too) would be sheared if it were stretched along its own axes
  const bool rigidRect = e.type != tpl::ItemType::Text || e.textBox;
  if (rigidRect && (e.rotation != 0 || e.skewH != 0 || e.skewV != 0))
    {
      // stretching the rectangle along its own axes would shear a turned item: move its center with everything else and
      // give it the mean scale instead
      const QPointF center = e.pos + e.rect.center();
      const QPointF moved(origin.x() + (center.x() - origin.x()) * sx, origin.y() + (center.y() - origin.y()) * sy);
      const QPointF c = e.rect.center();
      e.rect = QRectF(c.x() - e.rect.width() * s / 2, c.y() - e.rect.height() * s / 2, e.rect.width() * s, e.rect.height() * s);
      e.pos = moved - c;
    }
  else
    {
      e.pos = QPointF(origin.x() + (e.pos.x() - origin.x()) * sx, origin.y() + (e.pos.y() - origin.y()) * sy);
      e.rect = QRectF(e.rect.x() * sx, e.rect.y() * sy, e.rect.width() * sx, e.rect.height() * sy);
    }
  e.font.pointSize *= s;
  e.font.letterSpacing *= s;
  e.outline.width *= s;
  e.radius *= s;
  e.shadow.dx *= s;
  e.shadow.dy *= s;
  e.shadow.blur *= s;
}

void EditorDocument::fitTextRect(tpl::Element &e)
{
  if (e.type != tpl::ItemType::Text || e.textBox) return;
  tpl::RenderOptions literal;
  literal.expandMacros = false;
  const QRectF bounds = tpl::textBounds(e, literal);
  if (qAbs(bounds.x() - e.rect.x()) < 1e-6 && qAbs(bounds.y() - e.rect.y()) < 1e-6 && qAbs(bounds.width() - e.rect.width()) < 1e-6
      && qAbs(bounds.height() - e.rect.height()) < 1e-6)
    return;
  setRectKeeping(e, bounds, QPointF(0, 0));
}

void EditorDocument::load(const tpl::TemplateDocument &doc, const QString &fileName)
{
  d = doc;
  QSet<QString> used;
  for (tpl::Element &e : d.items)
    {
      if (e.id.isEmpty() || used.contains(e.id))
        {
          // newId() looks at d.items, which already holds the id being replaced: free it first
          e.id.clear();
          e.id = newId(e.type);
        }
      used.insert(e.id);
      fitTextRect(e);
    }
  stack.clear();
  file = fileName;
  legacy = doc.fromLegacy;
  epoch++;
  emit reset();
  emit fileChanged();
}

void EditorDocument::setFile(const QString &fileName, bool legacyImport)
{
  file = fileName;
  legacy = legacyImport;
  emit fileChanged();
}

QStringList EditorDocument::addItems(const QVector<tpl::Element> &items, const QString &text)
{
  if (items.isEmpty()) return QStringList();
  QVector<tpl::Element> list;
  QSet<QString> used;
  for (const tpl::Element &e : d.items) used.insert(e.id);
  for (tpl::Element e : items)
    {
      tpl::sanitize(e);
      if (e.id.isEmpty() || used.contains(e.id))
        {
          e.id.clear();
          // newId() only sees the document: count what this call has handed out already
          const QString prefix = QString::fromLatin1(typePrefix(e.type));
          for (int n = 1;; n++)
            {
              const QString id = QStringLiteral("%1-%2").arg(prefix).arg(n);
              if (!used.contains(id))
                {
                  e.id = id;
                  break;
                }
            }
        }
      used.insert(e.id);
      fitTextRect(e);
      list.append(e);
    }
  stack.push(new AddItemsCommand(this, list, text));
  QStringList out;
  for (const tpl::Element &e : list) out.append(e.id);
  return out;
}

void EditorDocument::removeItems(const QStringList &want, const QString &text)
{
  QStringList present;
  for (const QString &id : want)
    if (indexOf(id) >= 0) present.append(id);
  if (present.isEmpty()) return;
  stack.push(new RemoveItemsCommand(this, present, text));
}

void EditorDocument::clearItems(const QString &text)
{
  removeItems(ids(), text);
}

void EditorDocument::modifyItems(const QVector<tpl::Element> &afterIn, const QString &text, const QString &mergeKey, bool followAnchors)
{
  QVector<tpl::Element> before, after;
  for (tpl::Element e : afterIn)
    {
      const tpl::Element *cur = find(e.id);
      if (!cur) continue;
      tpl::sanitize(e);
      fitTextRect(e);
      if (followAnchors && d.layout == tpl::TemplateDocument::Anchored) tpl::reanchor(*cur, e, d.canvas);
      before.append(*cur);
      after.append(e);
    }
  if (before.isEmpty() || sameElements(before, after)) return;
  stack.push(new ModifyItemsCommand(this, before, after, text, mergeKey));
}

void EditorDocument::commitModify(const QVector<tpl::Element> &beforeIn, const QVector<tpl::Element> &afterIn, const QString &text)
{
  QVector<tpl::Element> before, after;
  for (int i = 0; i < beforeIn.size() && i < afterIn.size(); i++)
    {
      if (!find(afterIn[i].id)) continue;
      tpl::Element e = afterIn[i];
      tpl::sanitize(e);
      fitTextRect(e);
      if (d.layout == tpl::TemplateDocument::Anchored) tpl::reanchor(beforeIn[i], e, d.canvas);
      before.append(beforeIn[i]);
      after.append(e);
    }
  if (before.isEmpty() || sameElements(before, after))
    {
      // the live preview may have left the document in a state that differs from `before`: put it back
      for (const tpl::Element &e : before) replaceRaw(e);
      return;
    }
  stack.push(new ModifyItemsCommand(this, before, after, text, QString()));
}

void EditorDocument::reorder(const QStringList &newOrder, const QString &text)
{
  const QStringList cur = ids();
  if (newOrder == cur || newOrder.size() != cur.size()) return;
  for (const QString &id : cur)
    if (!newOrder.contains(id)) return;
  stack.push(new ReorderCommand(this, newOrder, text));
}

void EditorDocument::setCanvas(const QRectF &canvas, tpl::TemplateDocument::Layout layout, const QString &text)
{
  if (!(canvas.width() >= 1 && canvas.height() >= 1 && canvas.width() <= tpl::limits::maxCoord && canvas.height() <= tpl::limits::maxCoord))
    return;
  if (canvas == d.canvas && layout == d.layout) return;
  stack.push(new CanvasCommand(this, canvas, layout, text));
}

void EditorDocument::insertRaw(int index, const tpl::Element &e)
{
  index = qBound(0, index, int(d.items.size()));
  d.items.insert(index, e);
  emit itemInserted(e.id);
}

void EditorDocument::removeRaw(const QString &id)
{
  const int i = indexOf(id);
  if (i < 0) return;
  d.items.remove(i);
  emit itemRemoved(id);
}

void EditorDocument::replaceRaw(const tpl::Element &e)
{
  const int i = indexOf(e.id);
  if (i < 0) return;
  d.items[i] = e;
  emit itemChanged(e.id);
}

void EditorDocument::setOrderRaw(const QStringList &order)
{
  QVector<tpl::Element> sorted;
  for (const QString &id : order)
    {
      const int i = indexOf(id);
      if (i >= 0) sorted.append(d.items[i]);
    }
  if (sorted.size() != d.items.size()) return;
  d.items = sorted;
  emit orderChanged();
}

void EditorDocument::setCanvasRaw(const QRectF &canvas, tpl::TemplateDocument::Layout layout)
{
  if (d.sampledTransparency && d.canvas.width() > 0 && d.canvas.height() > 0)
    d.transparencyPoint = canvas.topLeft() + QPointF(
      (d.transparencyPoint.x() - d.canvas.x()) * canvas.width() / d.canvas.width(),
      (d.transparencyPoint.y() - d.canvas.y()) * canvas.height() / d.canvas.height());
  d.canvas = canvas;
  d.layout = layout;
  emit canvasChanged();
}
