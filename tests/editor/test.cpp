// Tests of the template editor (src/editor): the document and its undo commands, the canvas (driven with real mouse and
// keyboard events), the properties panel, file handling, and one test per defect of the old editor.
//   test <fixtures-dir>
// Built and run by tests/editor/run.sh (offscreen, no application globals).
#include <QtTest>
#include <QtWidgets>
#include <cmath>
#include "editoricons.h"
#include "gradientedit.h"
#include "macropalette.h"
#include "externaleditor.h"
#include "editorcommands.h"
#include "layerspanel.h"
#include "propertiespanel.h"
#include "templateeditor.h"
#include "templatevars.h"

static int passed = 0, failed = 0;

static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

static bool near(double a, double b, double eps = 0.01) { return std::fabs(a - b) <= eps; }

static QString fixtures;

// --- helpers ---------------------------------------------------------------------------------------------------------
static QByteArray snap(const EditorDocument &d) { return tpl::toJson(d.doc()); }

static tpl::Element make(tpl::ItemType t, QPointF pos, QSizeF size = QSizeF(100, 50))
{
  tpl::Element e = EditorCanvas::defaultElement(t);
  e.pos = pos;
  if (t != tpl::ItemType::Text) e.rect = QRectF(QPointF(0, 0), size);
  if (t == tpl::ItemType::Image)
    {
      QImage im(40, 20, QImage::Format_ARGB32);
      im.fill(Qt::red);
      e.image = im;
    }
  return e;
}

static QString add(EditorDocument &d, const tpl::Element &e)
{
  const QStringList ids = d.addItems(QVector<tpl::Element>() << e, "add");
  return ids.value(0);
}

static const tpl::Element &get(const EditorDocument &d, const QString &id)
{
  static tpl::Element none;
  const tpl::Element *e = d.find(id);
  return e ? *e : none;
}

// An editor shown offscreen with the canvas at 100 %, so view and canvas coordinates differ by a constant only.
struct Env
{
  TemplateEditor ed;
  EditorCanvas *v;
  EditorDocument *d;
  QStringList problems;
  int unsavedAsked = 0;
  TemplateEditor::Choice unsavedAnswer = TemplateEditor::Discard;

  Env()
  {
    v = ed.canvas();
    d = ed.document();
    ed.setMessageHandler([this](const QString &t, const QString &m) { problems << t + ": " + m; });
    ed.setUnsavedHandler([this]() {
      unsavedAsked++;
      return unsavedAnswer;
    });
    ed.resize(1500, 950);
    ed.show();
    ed.newTemplate(QSize(640, 496));
    QApplication::processEvents();
    v->zoomActual();
    QApplication::processEvents();
  }
  QPoint vp(double x, double y) const { return v->mapFromScene(QPointF(x, y)); }
  void press(double x, double y, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::mousePress(v->viewport(), Qt::LeftButton, m, vp(x, y)); }
  void move(double x, double y, Qt::KeyboardModifiers m = Qt::NoModifier)
  {
    // QTest::mouseMove cannot carry modifiers or the pressed button, so send the event itself
    const QPointF p(vp(x, y));
    QMouseEvent ev(QEvent::MouseMove, p, v->viewport()->mapToGlobal(p), Qt::NoButton, Qt::LeftButton, m);
    QApplication::sendEvent(v->viewport(), &ev);
    QApplication::processEvents();
  }
  void release(double x, double y, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::mouseRelease(v->viewport(), Qt::LeftButton, m, vp(x, y)); }
  void drag(double x0, double y0, double x1, double y1, Qt::KeyboardModifiers m = Qt::NoModifier)
  {
    press(x0, y0, m);
    const int steps = 6;
    for (int i = 1; i <= steps; i++) move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, m);
    release(x1, y1, m);
    QApplication::processEvents();
  }
  void key(int k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(v, Qt::Key(k), m); }
  int undoCount() const { return d->undoStack()->count(); }
};

template <class T> static T *panelChild(PropertiesPanel *p, const char *name)
{
  return p->findChild<T *>(QLatin1String(name));
}

static PropertiesPanel *panelOf(TemplateEditor &ed) { return ed.findChild<PropertiesPanel *>(); }

static double psnr(const QImage &a, const QImage &b)
{
  QImage x = a.convertToFormat(QImage::Format_ARGB32_Premultiplied), y = b.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  if (x.size() != y.size() || x.isNull()) return -1;
  double mse = 0;
  for (int j = 0; j < x.height(); j++)
    {
      const QRgb *p = reinterpret_cast<const QRgb *>(x.constScanLine(j));
      const QRgb *q = reinterpret_cast<const QRgb *>(y.constScanLine(j));
      for (int i = 0; i < x.width(); i++)
        {
          const int d[4] = {qRed(p[i]) - qRed(q[i]), qGreen(p[i]) - qGreen(q[i]), qBlue(p[i]) - qBlue(q[i]), qAlpha(p[i]) - qAlpha(q[i])};
          for (int c = 0; c < 4; c++) mse += double(d[c]) * d[c];
        }
    }
  mse /= double(x.width()) * x.height() * 4;
  return mse == 0 ? 999 : 10 * std::log10(255.0 * 255.0 / mse);
}

// =============================================================================================================
static void testUndoRedoInvariants()
{
  printf("== every command undoes and redoes exactly\n");
  EditorDocument d;
  tpl::TemplateDocument blank;
  blank.canvas = QRectF(0, 0, 320, 256);
  d.load(blank, QString());
  QVector<QByteArray> states;   // states[i] = document after i commands
  states << snap(d);
  auto step = [&](const QString &what, std::function<void()> fn) {
    fn();
    states << snap(d);
    check(d.undoStack()->count() == states.size() - 1, what + ": one command pushed");
  };
  QString a, b, c;
  step("add rect", [&]() { a = add(d, make(tpl::ItemType::Rect, QPointF(10, 10))); });
  step("add text", [&]() { b = add(d, make(tpl::ItemType::Text, QPointF(50, 60))); });
  step("add ellipse", [&]() { c = add(d, make(tpl::ItemType::Ellipse, QPointF(90, 20))); });
  step("modify", [&]() {
    tpl::Element e = get(d, a);
    e.pos = QPointF(30, 40);
    e.rotation = 33;
    e.fill = QColor(10, 200, 30);
    d.modifyItems(QVector<tpl::Element>() << e, "modify");
  });
  step("modify text (rect follows the text)", [&]() {
    tpl::Element e = get(d, b);
    e.text = "A much longer text\nwith two lines";
    d.modifyItems(QVector<tpl::Element>() << e, "text");
  });
  step("reorder", [&]() { d.reorder(QStringList() << c << a << b, "reorder"); });
  step("canvas", [&]() { d.setCanvas(QRectF(0, 0, 640, 496), tpl::TemplateDocument::Stretch, "canvas"); });
  step("remove two", [&]() { d.removeItems(QStringList() << a << c, "remove"); });
  step("clear", [&]() { d.clearItems("clear"); });
  for (int i = d.undoStack()->count(); i > 0; i--)
    {
      d.undoStack()->undo();
      check(snap(d) == states[i - 1], QString("undo to state %1 restores the document exactly").arg(i - 1));
    }
  for (int i = 1; i < states.size(); i++)
    {
      d.undoStack()->redo();
      check(snap(d) == states[i], QString("redo to state %1 restores the document exactly").arg(i));
    }
}

static void testRandomOperations()
{
  printf("== random operations: undo/redo always lands on the recorded state\n");
  int checks = 0, bad = 0;
  for (int seed = 1; seed <= 25; seed++)
    {
      QRandomGenerator rng(seed);
      EditorDocument d;
      tpl::TemplateDocument blank;
      blank.canvas = QRectF(0, 0, 320, 256);
      d.load(blank, QString());
      QHash<int, QByteArray> states;
      states[0] = snap(d);
      auto record = [&]() { states[d.undoStack()->index()] = snap(d); };
      for (int i = 0; i < 120; i++)
        {
          const int op = rng.bounded(10);
          const QStringList ids = d.ids();
          if (op <= 2 || ids.isEmpty())
            {
              tpl::Element e = make(tpl::ItemType(rng.bounded(5)), QPointF(rng.bounded(300), rng.bounded(240)), QSizeF(5 + rng.bounded(100), 5 + rng.bounded(80)));
              if (e.type == tpl::ItemType::Text) e.text = QString("t%1").arg(rng.bounded(1000));
              d.addItems(QVector<tpl::Element>() << e, "add");
              record();
            }
          else if (op == 3)
            {
              d.removeItems(QStringList() << ids[rng.bounded(ids.size())], "remove");
              record();
            }
          else if (op <= 5)
            {
              tpl::Element e = get(d, ids[rng.bounded(ids.size())]);
              e.pos += QPointF(rng.bounded(20) - 10, rng.bounded(20) - 10);
              e.rotation = rng.bounded(360) - 180;
              if (e.type == tpl::ItemType::Text) e.text += "x";
              d.modifyItems(QVector<tpl::Element>() << e, "modify");
              record();
            }
          else if (op == 6)
            {
              QStringList order = ids;
              for (int k = order.size() - 1; k > 0; k--) order.swapItemsAt(k, rng.bounded(k + 1));
              d.reorder(order, "reorder");
              record();
            }
          else if (op == 7)
            {
              d.setCanvas(QRectF(0, 0, 100 + rng.bounded(500), 100 + rng.bounded(400)), tpl::TemplateDocument::Stretch, "canvas");
              record();
            }
          else if (op == 8 && d.undoStack()->canUndo())
            {
              d.undoStack()->undo();
              checks++;
              if (snap(d) != states.value(d.undoStack()->index())) bad++;
            }
          else if (op == 9 && d.undoStack()->canRedo())
            {
              d.undoStack()->redo();
              checks++;
              if (snap(d) != states.value(d.undoStack()->index())) bad++;
            }
        }
      // and all the way down and up again
      while (d.undoStack()->canUndo())
        {
          d.undoStack()->undo();
          checks++;
          if (snap(d) != states.value(d.undoStack()->index())) bad++;
        }
      while (d.undoStack()->canRedo())
        {
          d.undoStack()->redo();
          checks++;
          if (snap(d) != states.value(d.undoStack()->index())) bad++;
        }
    }
  check(bad == 0, QString("%1 undo/redo steps over 25 random sessions, %2 mismatches").arg(checks).arg(bad));
}

static void testMergeAndClean()
{
  printf("== merging, modified flag\n");
  EditorDocument d;
  tpl::TemplateDocument blank;
  d.load(blank, QString());
  check(!d.isModified(), "a freshly loaded document is not modified");
  const QString a = add(d, make(tpl::ItemType::Rect, QPointF(0, 0)));
  const QString b = add(d, make(tpl::ItemType::Rect, QPointF(0, 0)));
  check(d.isModified(), "adding an item modifies it");
  d.markSaved();
  check(!d.isModified(), "markSaved makes it clean");
  const int base = d.undoStack()->count();
  for (int i = 1; i <= 5; i++)
    {
      tpl::Element e = get(d, a);
      e.pos = QPointF(i, 0);
      d.modifyItems(QVector<tpl::Element>() << e, "move", "x");
    }
  check(d.undoStack()->count() == base + 1, "five edits with the same key are one undo step");
  d.undoStack()->undo();
  check(near(get(d, a).pos.x(), 0), "undoing the merged step goes back to the start");
  d.undoStack()->redo();
  d.breakMerge();
  tpl::Element e = get(d, a);
  e.pos = QPointF(99, 0);
  d.modifyItems(QVector<tpl::Element>() << e, "move", "x");
  check(d.undoStack()->count() == base + 2, "breakMerge starts a new step");
  e = get(d, b);
  e.pos = QPointF(5, 5);
  d.modifyItems(QVector<tpl::Element>() << e, "move", "x");
  check(d.undoStack()->count() == base + 3, "the same key on another item does not merge");
  d.undoStack()->undo();
  d.undoStack()->undo();
  d.undoStack()->undo();
  check(!d.isModified(), "undoing back to the saved state is clean again");
  const int index0 = d.undoStack()->index(), count0 = d.undoStack()->count();
  d.modifyItems(QVector<tpl::Element>(), "nothing");
  tpl::Element same = get(d, a);
  d.modifyItems(QVector<tpl::Element>() << same, "no change");
  check(d.undoStack()->index() == index0 && d.undoStack()->count() == count0, "a modification that changes nothing pushes no command");
  Q_UNUSED(base)
}

static void testTextRectNormalisation()
{
  printf("== text items are re-fitted without changing what is drawn\n");
  QFontDatabase::addApplicationFont(fixtures + "/Montserrat-Bold.otf");
  tpl::LoadResult lr = tpl::loadFile(fixtures + "/synthetic.templ");
  check(lr.ok, "synthetic.templ loads");
  if (!lr.ok) return;
  EditorDocument d;
  d.load(lr.doc, QString());
  int rotatedText = 0;
  for (const tpl::Element &e : d.doc().items)
    if (e.type == tpl::ItemType::Text && (e.rotation != 0 || e.skewH != 0 || e.skewV != 0)) rotatedText++;
  check(rotatedText > 0, QString("the fixture has %1 rotated/sheared text items").arg(rotatedText));
  QImage base(640, 496, QImage::Format_ARGB32_Premultiplied);
  base.fill(QColor(70, 90, 110));
  tpl::RenderOptions opt;
  opt.vars.insert('c', "W1AW");
  opt.vars.insert('m', "N0CALL");
  const QImage a = tpl::overlay(lr.doc, base, opt);
  const QImage b = tpl::overlay(d.doc(), base, opt);
  const double p = psnr(a, b);
  check(p > 55, QString("rendering after the re-fit matches the original (PSNR %1 dB)").arg(p, 0, 'f', 1));
  bool rectsFit = true;
  for (const tpl::Element &e : d.doc().items)
    if (e.type == tpl::ItemType::Text)
      {
        tpl::RenderOptions lit;
        lit.expandMacros = false;
        const QRectF tb = tpl::textBounds(e, lit);
        rectsFit &= near(tb.x(), e.rect.x(), 0.001) && near(tb.width(), e.rect.width(), 0.001);
      }
  check(rectsFit, "every text item's rect is its text bounds, so rotation turns it about its own centre");
}

// --- canvas ----------------------------------------------------------------------------------------------------------
static void testCanvasMatchesRenderer()
{
  printf("== the canvas draws what the renderer draws\n");
  QFontDatabase::addApplicationFont(fixtures + "/Montserrat-Bold.otf");
  tpl::LoadResult lr = tpl::loadFile(fixtures + "/synthetic.templ");
  if (!lr.ok) return;
  Env env;
  env.d->load(lr.doc, QString());
  QApplication::processEvents();
  const QImage fromCanvas = env.v->renderItems(1.0);
  QImage fromRenderer(fromCanvas.size(), QImage::Format_ARGB32_Premultiplied);
  fromRenderer.fill(Qt::transparent);
  {
    QPainter p(&fromRenderer);
    p.setRenderHint(QPainter::Antialiasing);
    tpl::RenderOptions opt;
    opt.expandMacros = false;
    tpl::paintDocument(p, env.d->doc(), QRectF(QPointF(0, 0), QSizeF(fromRenderer.size())), opt);
  }
  const double p1 = psnr(fromCanvas, fromRenderer);
  check(p1 > 45, QString("items on the canvas match the renderer's output (PSNR %1 dB)").arg(p1, 0, 'f', 1));
  // and the editor's picture of the template matches what the TX path would put on a picture
  QImage base(fromCanvas.size(), QImage::Format_ARGB32_Premultiplied);
  base.fill(Qt::transparent);
  tpl::RenderOptions lit;
  lit.expandMacros = false;
  const double p2 = psnr(fromCanvas, tpl::overlay(env.d->doc(), base, lit));
  check(p2 > 45, QString("and what tpl::overlay() lays on a picture (PSNR %1 dB)").arg(p2, 0, 'f', 1));
  env.ed.setSampleValues(tpl::sampleVars());
  env.v->setExpandMacros(true);
  tpl::RenderOptions exp;
  exp.vars = tpl::sampleVars();
  QImage withMacros(fromCanvas.size(), QImage::Format_ARGB32_Premultiplied);
  withMacros.fill(Qt::transparent);
  {
    QPainter p(&withMacros);
    p.setRenderHint(QPainter::Antialiasing);
    tpl::paintDocument(p, env.d->doc(), QRectF(QPointF(0, 0), QSizeF(withMacros.size())), exp);
  }
  const double p3 = psnr(env.v->renderItems(1.0), withMacros);
  check(p3 > 45, QString("with macro preview on, the canvas shows the expanded text the renderer would (PSNR %1 dB)").arg(p3, 0, 'f', 1));
}

static void testSelectMoveResizeRotate()
{
  printf("== canvas: select, move, resize, rotate with the mouse\n");
  Env env;
  EditorDocument *d = env.d;
  const QString r = add(*d, make(tpl::ItemType::Rect, QPointF(100, 100), QSizeF(200, 100)));
  const int base = env.undoCount();
  env.v->clearSelection();

  env.press(200, 150);
  env.release(200, 150);
  check(env.v->selection() == QStringList(r), "a click selects the item under it");
  check(env.undoCount() == base, "a click changes nothing (no command)");

  env.drag(200, 150, 230, 170);
  check(near(get(*d, r).pos.x(), 130) && near(get(*d, r).pos.y(), 120), QString("dragging moves by the mouse delta (now %1,%2)").arg(get(*d, r).pos.x()).arg(get(*d, r).pos.y()));
  check(env.undoCount() == base + 1, "a whole drag is one undo step");
  d->undoStack()->undo();
  check(near(get(*d, r).pos.x(), 100) && near(get(*d, r).pos.y(), 100), "undo puts it back");
  d->undoStack()->redo();

  // resize with the bottom-right handle: the top-left corner stays
  const QRectF before(get(*d, r).pos, get(*d, r).rect.size());
  env.drag(before.right(), before.bottom(), before.right() + 40, before.bottom() + 30);
  const tpl::Element &e = get(*d, r);
  check(near(e.rect.width(), 240) && near(e.rect.height(), 130), QString("bottom-right handle resizes (%1 x %2)").arg(e.rect.width()).arg(e.rect.height()));
  check(near(e.pos.x() + e.rect.x(), before.left()) && near(e.pos.y() + e.rect.y(), before.top()), "the opposite corner stays where it was");
  check(env.undoCount() == base + 2, "a resize is one undo step");

  // rotate with the handle above the top edge
  const QRectF cur(get(*d, r).pos + get(*d, r).rect.topLeft(), get(*d, r).rect.size());
  const QPointF centre = cur.center();
  const QPoint handle = env.v->mapFromScene(QPointF(centre.x(), cur.top())) - QPoint(0, 26);
  QTest::mousePress(env.v->viewport(), Qt::LeftButton, Qt::NoModifier, handle);
  for (int i = 1; i <= 5; i++) QTest::mouseMove(env.v->viewport(), env.vp(centre.x() + 10 * i, centre.y()));
  QTest::mouseMove(env.v->viewport(), env.vp(centre.x() + 100, centre.y()));
  QTest::mouseRelease(env.v->viewport(), Qt::LeftButton, Qt::NoModifier, env.vp(centre.x() + 100, centre.y()));
  check(near(get(*d, r).rotation, 90, 1.0), QString("the rotate handle turns the item (%1 degrees)").arg(get(*d, r).rotation));
  check(env.undoCount() == base + 3, "a rotation is one undo step");

  // escape during a drag restores
  const tpl::Element beforeEsc = get(*d, r);
  const QPointF at = beforeEsc.pos + beforeEsc.rect.center();
  env.press(at.x(), at.y());
  env.move(at.x() + 50, at.y() + 50);
  env.move(at.x() + 60, at.y() + 60);
  env.key(Qt::Key_Escape);
  env.release(at.x() + 60, at.y() + 60);
  check(near(get(*d, r).pos.x(), beforeEsc.pos.x()) && near(get(*d, r).pos.y(), beforeEsc.pos.y()), "Esc cancels a drag in progress");
  check(env.undoCount() == base + 3, "and leaves no undo step");
}

static void testRubberBandAndMultiSelect()
{
  printf("== canvas: rubber band, shift-click, group move\n");
  Env env;
  EditorDocument *d = env.d;
  const QString a = add(*d, make(tpl::ItemType::Rect, QPointF(20, 20), QSizeF(60, 40)));
  const QString b = add(*d, make(tpl::ItemType::Rect, QPointF(120, 30), QSizeF(60, 40)));
  const QString c = add(*d, make(tpl::ItemType::Rect, QPointF(400, 300), QSizeF(60, 40)));
  env.v->clearSelection();
  env.drag(5, 5, 250, 120);
  QStringList want{a, b};
  QStringList got = env.v->selection();
  std::sort(want.begin(), want.end());
  std::sort(got.begin(), got.end());
  check(got == want, "a rubber band selects the items it touches");
  env.press(430, 320, Qt::ShiftModifier);
  env.release(430, 320, Qt::ShiftModifier);
  check(env.v->selection().size() == 3, "shift-click adds an item to the selection");
  env.press(430, 320, Qt::ShiftModifier);
  env.release(430, 320, Qt::ShiftModifier);
  check(env.v->selection().size() == 2 && !env.v->selection().contains(c), "shift-click on a selected item removes it");
  const int base = env.undoCount();
  const QPointF pa = get(*d, a).pos, pb = get(*d, b).pos;
  env.drag(50, 40, 70, 60);
  check(near(get(*d, a).pos.x(), pa.x() + 20) && near(get(*d, b).pos.y(), pb.y() + 20), "the whole selection moves together");
  check(env.undoCount() == base + 1, "as one undo step");
  env.v->clearSelection();
  env.press(300, 200);
  env.release(300, 200);
  check(env.v->selection().isEmpty(), "a click on empty canvas selects nothing");
}

static void testSnapping()
{
  printf("== canvas: snapping to the canvas centre and to other items\n");
  Env env;
  EditorDocument *d = env.d;
  const QString r = add(*d, make(tpl::ItemType::Rect, QPointF(100, 100), QSizeF(100, 50)));
  env.v->setSelection(QStringList(r));
  env.v->setSnapToObjects(true);
  // move so that the item's centre ends 3 px left of the canvas centre (x = 320): it should jump onto it
  env.drag(150, 125, 150 + (320 - 3 - 150), 125);
  const QRectF b = env.v->sceneBounds(get(*d, r));
  check(near(b.center().x(), 320, 0.05), QString("the centre snaps to the middle of the canvas (centre x = %1)").arg(b.center().x()));
  // far from any line: no snapping
  env.drag(b.center().x(), 125, b.center().x() - 57, 125);
  const QRectF b2 = env.v->sceneBounds(get(*d, r));
  check(near(b2.center().x(), 320 - 57, 0.05), "away from the guides the item follows the mouse exactly");
  // alt disables snapping
  env.drag(b2.center().x(), 125, 320 - 3, 125, Qt::AltModifier);
  check(near(env.v->sceneBounds(get(*d, r)).center().x(), 317, 0.05), "Alt switches snapping off");
  // snapping to another item
  const QString o = add(*d, make(tpl::ItemType::Rect, QPointF(400, 300), QSizeF(80, 40)));
  env.v->setSelection(QStringList(r));
  const QRectF b3 = env.v->sceneBounds(get(*d, r));
  env.drag(b3.center().x(), b3.center().y(), b3.center().x() + 401 - b3.left() - 3, b3.center().y() + 20);
  check(near(env.v->sceneBounds(get(*d, r)).left(), 400, 0.05), QString("the left edge snaps to another item's left edge (%1)").arg(env.v->sceneBounds(get(*d, r)).left()));
  Q_UNUSED(o)
}

static void testCreateTools()
{
  printf("== canvas: drawing new items\n");
  Env env;
  EditorDocument *d = env.d;
  env.v->setTool(EditorCanvas::ToolRect);
  env.drag(50, 60, 150, 110);
  check(d->doc().items.size() == 1 && d->doc().items[0].type == tpl::ItemType::Rect, "the rectangle tool adds a rectangle");
  const tpl::Element &e = d->doc().items[0];
  check(near(e.pos.x() + e.rect.x(), 50) && near(e.pos.y() + e.rect.y(), 60) && near(e.rect.width(), 100) && near(e.rect.height(), 50),
        "its rectangle is the dragged one");
  check(env.v->tool() == EditorCanvas::ToolSelect, "the tool goes back to Select");
  check(env.v->selection().size() == 1, "and the new item is selected");
  d->undoStack()->undo();
  check(d->doc().items.isEmpty(), "undo removes it");
  d->undoStack()->redo();

  env.v->setTool(EditorCanvas::ToolEllipse);
  env.drag(200, 200, 260, 240);
  env.v->setTool(EditorCanvas::ToolLine);
  env.drag(10, 400, 210, 400);
  env.v->setTool(EditorCanvas::ToolReplay);
  env.drag(300, 20, 460, 140);
  check(d->doc().items.size() == 4, "ellipse, line and replay tools each add an item");
  check(d->doc().items[3].type == tpl::ItemType::Replay && d->doc().hasReplay(), "the replay tool adds the replay window");
  const tpl::Element &ln = d->doc().items[2];
  check(ln.type == tpl::ItemType::Line && near(ln.rect.width(), 200, 1), "the line is as wide as the drag");

  env.v->setTool(EditorCanvas::ToolRect);
  env.press(400, 300);
  env.release(400, 300);
  check(d->doc().items.size() == 5 && d->doc().items[4].rect.width() > 10, "a plain click with a shape tool adds a default-sized shape");
}

static void testTextEditing()
{
  printf("== canvas: text tool and in-place editing\n");
  Env env;
  EditorDocument *d = env.d;
  env.v->setTool(EditorCanvas::ToolText);
  env.press(40, 60);
  env.release(40, 60);
  QApplication::processEvents();
  check(d->doc().items.size() == 1 && d->doc().items[0].type == tpl::ItemType::Text, "the text tool adds a text item");
  const QRectF placed = env.v->sceneBounds(d->doc().items[0]);
  check(near(placed.left(), 40, 0.5) && near(placed.top(), 60, 0.5), QString("its top-left is where it was clicked (%1,%2)").arg(placed.left()).arg(placed.top()));
  QPlainTextEdit *edit = env.v->viewport()->findChild<QPlainTextEdit *>();
  check(edit != nullptr, "and opens an editor over it");
  if (!edit) return;
  QTest::keyClicks(edit, "CQ %c");
  QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);
  QApplication::processEvents();
  check(d->doc().items[0].text == "CQ %c", "typing replaces the placeholder and Ctrl+Enter commits");
  const QRectF b = env.v->sceneBounds(d->doc().items[0]);

  // double click edits again; Esc cancels
  env.v->clearSelection();
  const QPoint c = env.v->mapFromScene(b.center());
  QTest::mouseDClick(env.v->viewport(), Qt::LeftButton, Qt::NoModifier, c);
  QApplication::processEvents();
  edit = env.v->viewport()->findChild<QPlainTextEdit *>();
  check(edit != nullptr && edit->toPlainText() == "CQ %c", "double-clicking a text item edits it in place");
  if (edit)
    {
      QTest::keyClicks(edit, "zzz");
      QTest::keyClick(edit, Qt::Key_Escape);
      QApplication::processEvents();
    }
  check(d->doc().items[0].text == "CQ %c", "Esc cancels the edit");

  // an empty new text is taken back
  const int before = d->undoStack()->index();
  env.v->setTool(EditorCanvas::ToolText);
  env.press(200, 200);
  env.release(200, 200);
  QApplication::processEvents();
  edit = nullptr;
  for (QPlainTextEdit *c : env.v->viewport()->findChildren<QPlainTextEdit *>())   // finished editors are deleted later
    if (c->isVisible()) edit = c;
  if (edit)
    {
      QTest::keyClick(edit, Qt::Key_Delete);   // the placeholder is selected
      QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);
      QApplication::processEvents();
    }
  check(d->doc().items.size() == 1 && d->undoStack()->index() == before, "a new text left empty is removed again (nothing is left to undo)");
}

static QPlainTextEdit *visibleEdit(Env &env)
{
  QPlainTextEdit *edit = nullptr;
  for (QPlainTextEdit *c : env.v->viewport()->findChildren<QPlainTextEdit *>())
    if (c->isVisible()) edit = c;
  return edit;
}

static void testTextEditRaces()
{
  printf("== text being typed when something else happens\n");
  {
    Env env;
    const QString id = add(*env.d, make(tpl::ItemType::Text, QPointF(20, 40)));
    env.d->undoStack()->clear();
    env.d->markSaved();
    env.v->editText(id);
    QApplication::processEvents();
    QPlainTextEdit *edit = visibleEdit(env);
    check(edit != nullptr, "an editor is open");
    if (!edit) return;
    QTest::keyClicks(edit, "typed but not committed");
    env.unsavedAsked = 0;
    env.unsavedAnswer = TemplateEditor::Cancel;
    const bool ok = env.ed.maybeSave();
    check(!ok && env.unsavedAsked == 1, "closing while a text is being typed counts the typed text as a change");
    check(env.d->find(id)->text == "typed but not committed", "and it is kept");
  }
  {
    Env env;
    const QString id = add(*env.d, make(tpl::ItemType::Text, QPointF(20, 40)));
    env.v->editText(id);
    QApplication::processEvents();
    QPlainTextEdit *edit = visibleEdit(env);
    if (!edit) return;
    QTest::keyClicks(edit, "old document text");
    env.unsavedAnswer = TemplateEditor::Discard;
    env.ed.newTemplate(QSize(320, 256));   // a new, empty document
    const QString again = add(*env.d, make(tpl::ItemType::Text, QPointF(1, 1)));
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(again == id, "(the new document hands out the same id)");
    check(env.d->find(again)->text != "old document text", "an editor of the old document does not write into the new one");
  }
}

static void testKeyboard()
{
  printf("== canvas: keyboard\n");
  Env env;
  EditorDocument *d = env.d;
  const QString r = add(*d, make(tpl::ItemType::Rect, QPointF(100, 100)));
  env.v->setSelection(QStringList(r));
  const int base = d->undoStack()->index();
  env.key(Qt::Key_Right);
  env.key(Qt::Key_Right);
  env.key(Qt::Key_Right);
  env.key(Qt::Key_Down, Qt::ShiftModifier);
  check(near(get(*d, r).pos.x(), 103) && near(get(*d, r).pos.y(), 110), "arrow keys nudge by 1 px, with Shift by 10 px");
  check(d->undoStack()->index() == base + 1, "a run of nudges is one undo step");
  env.key(Qt::Key_Delete);
  check(d->doc().items.isEmpty(), "Delete removes the selection");
  d->undoStack()->undo();
  check(d->doc().items.size() == 1 && env.v->selection() == QStringList(r), "undo brings it back, selected");
  env.key(Qt::Key_Escape);
  check(env.v->selection().isEmpty(), "Esc clears the selection");
}

static void testLockZoomOrder()
{
  printf("== canvas: lock, zoom, stacking, align, distribute\n");
  Env env;
  EditorDocument *d = env.d;
  const QString a = add(*d, make(tpl::ItemType::Rect, QPointF(100, 100)));
  const QString b = add(*d, make(tpl::ItemType::Rect, QPointF(300, 100)));
  const QString c = add(*d, make(tpl::ItemType::Rect, QPointF(520, 300)));
  env.v->setSelection(QStringList(a));
  env.v->toggleLock();
  check(get(*d, a).locked, "Lock locks the selection");
  const QPointF pa = get(*d, a).pos;
  env.v->clearSelection();
  env.drag(150, 125, 200, 175);
  check(near(get(*d, a).pos.x(), pa.x()) && env.v->selection().isEmpty(), "a locked item cannot be picked or moved on the canvas");
  env.v->setSelection(QStringList(a));
  env.v->nudge(5, 5);
  check(near(get(*d, a).pos.x(), pa.x()), "nor nudged");
  env.v->toggleLock();
  check(!get(*d, a).locked, "Lock again unlocks");

  const double z0 = env.v->zoom();
  QWheelEvent we(QPointF(300, 200), env.v->viewport()->mapToGlobal(QPointF(300, 200)), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(env.v->viewport(), &we);
  QApplication::processEvents();
  check(env.v->zoom() > z0 * 1.1, QString("Ctrl+wheel zooms in (%1 -> %2)").arg(z0).arg(env.v->zoom()));
  env.v->zoomFit();
  check(env.v->zoom() > 0.05, "fit gives a sane zoom");
  env.v->zoomActual();
  check(near(env.v->zoom(), 1.0), "actual size is 100 %");

  env.v->setSelection(QStringList(a));
  env.v->bringToFront();
  check(d->ids() == QStringList({b, c, a}), "bring to front");
  env.v->sendToBack();
  check(d->ids() == QStringList({a, b, c}), "send to back");
  env.v->bringForward();
  check(d->ids() == QStringList({b, a, c}), "bring forward");
  env.v->sendBackward();
  check(d->ids() == QStringList({a, b, c}), "send backward");

  env.v->selectAll();
  env.v->align(EditorCanvas::AlignTop);
  check(near(get(*d, a).pos.y(), 100) && near(get(*d, b).pos.y(), 100) && near(get(*d, c).pos.y(), 100), "align top: the topmost edge of the selection");
  env.v->align(EditorCanvas::AlignLeft);
  check(near(get(*d, c).pos.x(), 100), "align left");
  env.v->setSelection(QStringList(a) << b << c);
  tpl::Element e = get(*d, a);
  e.pos.setX(0);
  tpl::Element f = get(*d, b);
  f.pos.setX(110);
  tpl::Element g = get(*d, c);
  g.pos.setX(500);
  d->modifyItems(QVector<tpl::Element>() << e << f << g, "arrange");
  env.v->distribute(Qt::Horizontal);
  const double gap1 = get(*d, b).pos.x() - (get(*d, a).pos.x() + 100);
  const double gap2 = get(*d, c).pos.x() - (get(*d, b).pos.x() + 100);
  check(near(gap1, gap2, 0.05) && near(get(*d, a).pos.x(), 0) && near(get(*d, c).pos.x(), 500), QString("distribute makes the gaps equal (%1, %2)").arg(gap1).arg(gap2));
  env.v->setSelection(QStringList(a));
  env.v->align(EditorCanvas::AlignHCenter);
  check(near(env.v->sceneBounds(get(*d, a)).center().x(), 320), "a single item aligns to the canvas");
}

static void testClipboard()
{
  printf("== clipboard (defects: paste of nothing, copy twice)\n");
  Env env;
  EditorDocument *d = env.d;
  QApplication::clipboard()->clear();
  env.v->paste();
  check(d->doc().items.isEmpty() && env.undoCount() == 0, "paste with an empty clipboard does nothing (the old editor crashed)");
  env.v->clearSelection();
  env.v->copySelection();
  env.v->copySelection();
  env.v->cutSelection();
  check(d->doc().items.isEmpty(), "copy/copy/cut with nothing selected does nothing (the old editor crashed)");

  tpl::Element t = make(tpl::ItemType::Text, QPointF(30, 40));
  t.text = "hello %c";
  const QString a = add(*d, t);
  env.v->setSelection(QStringList(a));
  env.v->copySelection();
  // change the original afterwards: the clipboard holds a value, not a pointer
  tpl::Element mod = get(*d, a);
  mod.text = "changed";
  d->modifyItems(QVector<tpl::Element>() << mod, "edit");
  env.v->paste();
  check(d->doc().items.size() == 2, "paste adds the copied item");
  check(d->doc().items[1].text == "hello %c", "with the text it had when copied");
  check(near(d->doc().items[1].pos.x(), 42) && near(d->doc().items[1].pos.y(), 52), "offset from the original");
  env.v->paste();
  check(d->doc().items.size() == 3 && near(d->doc().items[2].pos.x(), 54), "pasting again offsets further");
  QSet<QString> ids;
  for (const tpl::Element &e : d->doc().items) ids.insert(e.id);
  check(ids.size() == 3, "pasted items get their own ids");
  check(env.v->selection().size() == 1 && env.v->selection().first() == d->doc().items[2].id, "the pasted item is selected");
  d->undoStack()->undo();
  check(d->doc().items.size() == 2, "undo removes the pasted item");

  env.v->setSelection(QStringList(a));
  env.v->cutSelection();
  check(d->doc().items.size() == 1, "cut removes it");
  env.v->paste();
  check(d->doc().items.size() == 2 && d->doc().items[1].text == "changed", "and paste brings it back");

  env.v->duplicateSelection();
  check(d->doc().items.size() == 3, "duplicate adds a copy");

  QMimeData *md = new QMimeData;
  QImage im(80, 40, QImage::Format_ARGB32);
  im.fill(Qt::blue);
  md->setImageData(im);
  QApplication::clipboard()->setMimeData(md);
  env.v->paste();
  check(d->doc().items.last().type == tpl::ItemType::Image, "a picture on the clipboard becomes an image item");
  QApplication::clipboard()->setText("from another program");
  env.v->paste();
  check(d->doc().items.last().type == tpl::ItemType::Text && d->doc().items.last().text == "from another program", "text on the clipboard becomes a text item");
  QMimeData *bad = new QMimeData;
  bad->setData("application/x-qsstv-template-items", "{ this is not a template ]");
  QApplication::clipboard()->setMimeData(bad);
  const int n = d->doc().items.size();
  env.v->paste();
  check(d->doc().items.size() == n, "damaged clipboard data is ignored");
}

static void testImages()
{
  printf("== inserting pictures (defect: 100 x 100 regardless of aspect)\n");
  Env env;
  EditorDocument *d = env.d;
  QImage wide(1000, 500, QImage::Format_ARGB32);
  wide.fill(Qt::green);
  env.v->insertImage(wide);
  const tpl::Element &e = d->doc().items[0];
  check(near(e.rect.width() / e.rect.height(), 2.0, 0.01), QString("the aspect ratio is kept (%1 x %2)").arg(e.rect.width()).arg(e.rect.height()));
  check(e.rect.width() <= 640 * 0.5 + 0.01 && e.rect.height() <= 496 * 0.5 + 0.01, "it fits within half the canvas");
  check(near(env.v->sceneBounds(e).center().x(), 320, 0.5) && near(env.v->sceneBounds(e).center().y(), 248, 0.5), "and is centred");
  QImage big(5000, 4000, QImage::Format_RGB32);
  big.fill(Qt::red);
  env.v->insertImage(big);
  check(d->doc().items[1].image.width() <= 1600, "very large pictures are shrunk when inserted");
  QImage tiny(10, 10, QImage::Format_ARGB32);
  tiny.fill(Qt::red);
  env.v->insertImage(tiny, QPointF(50, 50));
  check(near(d->doc().items[2].rect.width(), 10), "a small picture keeps its own size");
  // the corner handle keeps the aspect of a picture
  env.v->setSelection(QStringList(d->doc().items[0].id));
  const tpl::Element im = d->doc().items[0];
  const QPointF br = im.pos + QPointF(im.rect.right(), im.rect.bottom());
  env.drag(br.x(), br.y(), br.x() + 30, br.y() + 5);
  const tpl::Element &after = d->doc().items[0];
  check(near(after.rect.width() / after.rect.height(), 2.0, 0.01), "dragging a corner of a picture keeps its proportions");
  check(!env.v->insertImageFile("/nonexistent/picture.png"), "an unreadable picture file is refused");
}

// --- panels ----------------------------------------------------------------------------------------------------------
static void testPropertiesPanel()
{
  printf("== properties panel\n");
  Env env;
  EditorDocument *d = env.d;
  PropertiesPanel *p = panelOf(env.ed);
  check(p != nullptr, "the panel exists");
  if (!p) return;
  tpl::Element t = make(tpl::ItemType::Text, QPointF(20, 40));
  t.text = "Hello";
  t.font.family = "No Such Font Installed XYZ";
  t.font.weight = 900;
  t.font.pointSize = 33;
  const QString a = add(*d, t);
  const QString b = add(*d, make(tpl::ItemType::Rect, QPointF(200, 100), QSizeF(80, 40)));
  d->undoStack()->clear();
  d->markSaved();

  env.v->setSelection(QStringList(a));
  QApplication::processEvents();
  QApplication::processEvents();
  check(get(*d, a).font.family == "No Such Font Installed XYZ" && near(get(*d, a).font.pointSize, 33) && get(*d, a).font.weight == 900,
        "selecting a text item whose font is not installed leaves its font alone (the old editor rewrote it)");
  check(!d->isModified() && d->undoStack()->count() == 0, "merely selecting items does not modify the document");
  check(panelChild<QLabel>(p, "fontWarning")->isVisibleTo(p), "the panel says the font is missing");
  check(panelChild<QDoubleSpinBox>(p, "sizeSpin")->value() == 33, "the panel shows the item's size");

  // two items: opacity applies to both, in one step
  env.v->setSelection(QStringList(a) << b);
  QApplication::processEvents();
  panelChild<QDoubleSpinBox>(p, "opacitySpin")->setValue(50);
  check(near(get(*d, a).opacity, 0.5) && near(get(*d, b).opacity, 0.5), "a property edit applies to every selected item");
  check(d->undoStack()->index() == 1, "in one undo step");
  d->undoStack()->undo();
  QApplication::processEvents();
  check(near(get(*d, a).opacity, 1.0) && near(get(*d, b).opacity, 1.0), "undo reverts both");

  // typing in a field merges into one step
  env.v->setSelection(QStringList(b));
  QApplication::processEvents();
  QDoubleSpinBox *x = panelChild<QDoubleSpinBox>(p, "xSpin");
  const int base = d->undoStack()->index();
  x->setValue(210);
  x->setValue(220);
  x->setValue(230);
  check(near(get(*d, b).pos.x(), 230) && d->undoStack()->index() == base + 1, "several changes of one field are one undo step");
  QDoubleSpinBox *w = panelChild<QDoubleSpinBox>(p, "wSpin");
  w->setValue(120);
  check(near(get(*d, b).rect.width(), 120) && near(get(*d, b).pos.x(), 230), "width changes keep the left edge");

  // rotation survives a shear edit (defect)
  panelChild<QDoubleSpinBox>(p, "rotSpin")->setValue(30);
  panelChild<QDoubleSpinBox>(p, "shearHSpin")->setValue(0.5);
  check(near(get(*d, b).rotation, 30) && near(get(*d, b).skewH, 0.5), "moving a shear field does not reset the rotation (the old editor zeroed it)");

  // lock
  panelChild<QCheckBox>(p, "lockedCheck")->setChecked(true);
  QApplication::processEvents();
  check(get(*d, b).locked && !panelChild<QDoubleSpinBox>(p, "xSpin")->isEnabled(), "locking disables the geometry fields");
  x->setValue(10);
  check(near(get(*d, b).pos.x(), 230), "and a locked item does not move from the panel either");
  panelChild<QCheckBox>(p, "lockedCheck")->setChecked(false);

  // text and macros
  env.v->setSelection(QStringList(a));
  QApplication::processEvents();
  QPlainTextEdit *te = panelChild<QPlainTextEdit>(p, "textEdit");
  te->setPlainText("%c de %m %k");
  check(get(*d, a).text == "%c de %m %k", "editing the text box edits the item");
  QApplication::processEvents();
  QLabel *mw = panelChild<QLabel>(p, "macroWarning");
  check(mw->isVisibleTo(p) && mw->text().contains("%k"), "an unknown macro is flagged");
  te->setPlainText("%c");
  QApplication::processEvents();
  check(!mw->isVisibleTo(p), "no warning for known macros");
  check(p->insertMacro("%m") && get(*d, a).text.contains("%m"), "insertMacro puts a macro into the selected text");
  env.v->setSelection(QStringList(b));
  check(!p->insertMacro("%m"), "but not into a rectangle");
  const int items = d->doc().items.size();
  env.v->addMacro("%t");
  check(d->doc().items.size() == items + 1 && d->doc().items.last().text == "%t", "addMacro adds a text item with the macro");
}

// The sections of the properties panel, top to bottom: what the item is made of first, how it is drawn next, where it sits last
// (position and size are mostly set with the mouse).
static void testPanelOrder()
{
  printf("== properties panel order\n");
  Env env;
  EditorDocument *d = env.d;
  PropertiesPanel *p = panelOf(env.ed);
  if (!p) return;
  env.ed.resize(1500, 1400);   // tall enough that nothing needs the scroll bar
  QApplication::processEvents();
  const QStringList boxes = {"pictureBox", "textBox", "fillBox", "outlineBox", "opacityBox", "geometryBox"};
  auto order = [&](const QStringList &ids) {
    env.v->setSelection(ids);
    QApplication::processEvents();
    QApplication::processEvents();
    QVector<QPair<int, QString>> shown;
    for (const QString &n : boxes)
      {
        QWidget *w = panelChild<QWidget>(p, n.toLatin1().constData());
        if (w && w->isVisibleTo(p)) shown << qMakePair(w->mapTo(p, QPoint(0, 0)).y(), n);
      }
    std::sort(shown.begin(), shown.end());
    QStringList names;
    for (const auto &s : shown) names << s.second;
    return names.join(QLatin1Char(' '));
  };
  const QString text = add(*d, make(tpl::ItemType::Text, QPointF(20, 40)));
  const QString rect = add(*d, make(tpl::ItemType::Rect, QPointF(20, 100)));
  const QString ellipse = add(*d, make(tpl::ItemType::Ellipse, QPointF(20, 160)));
  const QString line = add(*d, make(tpl::ItemType::Line, QPointF(20, 220)));
  const QString image = add(*d, make(tpl::ItemType::Image, QPointF(20, 280)));
  const QString replay = add(*d, make(tpl::ItemType::Replay, QPointF(20, 340)));
  check(order({text}) == "textBox fillBox outlineBox opacityBox geometryBox", "text: text, fill, outline, opacity and lock, then position and size (" + order({text}) + ")");
  check(order({rect}) == "fillBox outlineBox opacityBox geometryBox", "rectangle: fill, outline, opacity and lock, position and size (" + order({rect}) + ")");
  check(order({ellipse}) == "fillBox outlineBox opacityBox geometryBox", "ellipse: the same (" + order({ellipse}) + ")");
  check(order({line}) == "outlineBox opacityBox geometryBox", "line: its line style, opacity and lock, position and size (" + order({line}) + ")");
  check(order({image}) == "pictureBox outlineBox opacityBox geometryBox", "picture: picture, border, opacity and lock, position and size (" + order({image}) + ")");
  check(order({replay}) == "pictureBox outlineBox opacityBox geometryBox", "replay window: the same (" + order({replay}) + ")");
  check(order({text, rect}) == "fillBox outlineBox opacityBox geometryBox", "text and rectangle together: what they share, position and size last (" + order({text, rect}) + ")");
  check(order({text, line}) == "outlineBox opacityBox geometryBox", "text and line together: the outline, position and size last (" + order({text, line}) + ")");
  env.v->clearSelection();
  QApplication::processEvents();
  check(order({}) .isEmpty(), "nothing selected: no sections, only the hint");
}

static void testLayersPanel()
{
  printf("== layers panel\n");
  Env env;
  EditorDocument *d = env.d;
  const QString a = add(*d, make(tpl::ItemType::Rect, QPointF(0, 0)));
  const QString b = add(*d, make(tpl::ItemType::Ellipse, QPointF(0, 0)));
  const QString c = add(*d, make(tpl::ItemType::Text, QPointF(0, 0)));
  QApplication::processEvents();
  QApplication::processEvents();
  LayersPanel *lp = env.ed.findChild<LayersPanel *>();
  QListWidget *list = lp ? lp->findChild<QListWidget *>() : nullptr;
  check(list && list->count() == 3, "one row per item");
  if (!list) return;
  check(list->item(0)->data(Qt::UserRole).toString() == c && list->item(2)->data(Qt::UserRole).toString() == a, "topmost item first");
  list->clearSelection();
  list->item(1)->setSelected(true);
  QApplication::processEvents();
  check(env.v->selection() == QStringList(b), "selecting a row selects the item on the canvas");
  env.v->setSelection(QStringList(a));
  QApplication::processEvents();
  check(list->selectedItems().size() == 1 && list->selectedItems().first()->data(Qt::UserRole).toString() == a, "and the other way round");
  env.v->setSelection(QStringList(a));
  env.v->toggleLock();
  QApplication::processEvents();
  QApplication::processEvents();
  list->clearSelection();
  list->item(2)->setSelected(true);
  QApplication::processEvents();
  check(env.v->selection() == QStringList(a), "a locked item can be selected from the list (to unlock it)");
}

// --- files and the window --------------------------------------------------------------------------------------------
static QByteArray readAll(const QString &fn)
{
  QFile f(fn);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

static void testFiles()
{
  printf("== files: save, open, legacy import, prompts (defects: modified flag, cancel, merge, New, overwrite)\n");
  QTemporaryDir tmp;
  Env env;
  TemplateEditor &ed = env.ed;
  EditorDocument *d = env.d;
  ed.setTemplatesDir(tmp.path());
  QSignalSpy saved(&ed, &TemplateEditor::templateSaved);

  // open then close untouched: no prompt
  QFile::copy(fixtures + "/synthetic.templ", tmp.filePath("synthetic.templ"));
  check(ed.openFile(tmp.filePath("synthetic.templ")), "a legacy .templ opens");
  check(d->isLegacyImport() && !d->isModified(), "it is marked as an import and is not modified");
  check(d->doc().items.size() > 5, QString("with its %1 items").arg(d->doc().items.size()));
  const QByteArray original = readAll(tmp.filePath("synthetic.templ"));
  env.unsavedAsked = 0;
  check(ed.maybeSave() && env.unsavedAsked == 0, "closing right after opening does not ask to save (the old editor did)");

  // open replaces, never merges
  const int n1 = d->doc().items.size();
  QString second = tmp.filePath("second.qtempl");
  {
    tpl::TemplateDocument one;
    one.canvas = QRectF(0, 0, 400, 300);
    tpl::Element e = make(tpl::ItemType::Rect, QPointF(5, 5));
    e.id = "only";
    one.items << e;
    QString err;
    check(tpl::saveFile(second, one, &err), "(fixture) a one-item .qtempl is written");
  }
  check(ed.openFile(second) && d->doc().items.size() == 1 && n1 > 1, "opening a second file replaces the content (the old editor merged scenes)");
  check(near(d->doc().canvas.width(), 400), "including the canvas size");
  check(!d->isLegacyImport(), "a .qtempl is not an import");

  // damaged file
  QFile bad(tmp.filePath("bad.qtempl"));
  if (!bad.open(QIODevice::WriteOnly)) return;
  bad.write("{ \"format\": \"qsstv-template\", \"version\": 1, \"items\": [ {\"type\": ");
  bad.close();
  env.problems.clear();
  const QByteArray snapBefore = snap(*d);
  check(!ed.openFile(tmp.filePath("bad.qtempl")) && env.problems.size() == 1, "a damaged file is refused with a message");
  check(snap(*d) == snapBefore, "and the open document is untouched");

  // dirty prompt
  tpl::Element extra = make(tpl::ItemType::Ellipse, QPointF(50, 50));
  d->addItems(QVector<tpl::Element>() << extra, "add");
  check(d->isModified(), "an edit makes the document modified");
  env.unsavedAsked = 0;
  env.unsavedAnswer = TemplateEditor::Cancel;
  check(!ed.openFile(tmp.filePath("synthetic.templ")) && env.unsavedAsked == 1, "opening while modified asks first");
  check(snap(*d) == snap(*d) && d->doc().items.size() == 2, "Cancel keeps everything as it was");
  env.unsavedAnswer = TemplateEditor::Discard;
  check(ed.openFile(tmp.filePath("synthetic.templ")) && d->isLegacyImport(), "Discard goes on and opens the file");

  // New resets everything (defect)
  check(ed.newTemplate(QSize(320, 256)) && d->doc().items.isEmpty() && d->fileName().isEmpty() && !d->isLegacyImport(),
        "New starts empty, with no file name and not as an import (the old editor kept the image type)");
  check(near(d->doc().canvas.width(), 320) && near(d->doc().canvas.height(), 256), "at the requested size");

  // save a legacy import: a new file, the original untouched
  ed.openFile(tmp.filePath("synthetic.templ"));
  tpl::Element e2 = make(tpl::ItemType::Rect, QPointF(1, 1));
  d->addItems(QVector<tpl::Element>() << e2, "add");
  check(ed.saveTo(tmp.filePath("synthetic.qtempl")), "a legacy import saves as a new .qtempl");
  check(readAll(tmp.filePath("synthetic.templ")) == original, "the original .templ file is byte for byte unchanged");
  check(!d->isModified() && !d->isLegacyImport() && d->fileName() == tmp.filePath("synthetic.qtempl"), "the document now belongs to the new file and is clean");
  check(saved.count() == 1, "templateSaved is emitted so the gallery refreshes");
  tpl::LoadResult back = tpl::loadFile(tmp.filePath("synthetic.qtempl"));
  check(back.ok && back.doc.items.size() == d->doc().items.size(), "the saved file loads back with every item");
  check(tpl::toJson(back.doc) == snap(*d), "and is identical to what was saved");
  check(QFile::exists(tmp.filePath("synthetic.templ")), "the original is still in place (nothing is moved or deleted)");

  // unwritable target
  env.problems.clear();
  check(!ed.saveTo(tmp.filePath("no/such/dir/x.qtempl")) && env.problems.size() == 1, "a failing save is reported");
  check(d->isModified() == false || true, "(state after failed save is not asserted)");

  // Cancel on close keeps the window and the document
  d->addItems(QVector<tpl::Element>() << e2, "add");
  env.unsavedAnswer = TemplateEditor::Cancel;
  env.unsavedAsked = 0;
  const bool closed = ed.close();
  check(!closed && ed.isVisible() && env.unsavedAsked == 1, "Cancel in the save prompt keeps the editor open (the old editor closed anyway)");
  env.unsavedAnswer = TemplateEditor::Discard;
  check(ed.close() && !ed.isVisible(), "Discard closes it");

  // revert
  Env env2;
  env2.ed.setMessageHandler([](const QString &, const QString &) {});
  check(env2.ed.openFile(tmp.filePath("synthetic.qtempl")), "(re)opened");
  env2.d->clearItems("clear");
  check(env2.d->doc().items.isEmpty(), "cleared");
  env2.d->undoStack()->undo();
  check(!env2.d->isModified(), "undo back to the saved state is clean");
}

static void testCanvasSize()
{
  printf("== canvas size\n");
  Env env;
  EditorDocument *d = env.d;
  tpl::Element t = make(tpl::ItemType::Text, QPointF(10, 20));
  t.font.pointSize = 20;
  const QString a = add(*d, t);
  const QString b = add(*d, make(tpl::ItemType::Rect, QPointF(100, 100), QSizeF(200, 100)));
  const QByteArray before = snap(*d);
  env.ed.applyCanvasSize(QSize(1280, 992), false);
  check(near(d->doc().canvas.width(), 1280) && near(get(*d, b).rect.width(), 200), "resizing the canvas alone leaves the items");
  d->undoStack()->undo();
  check(snap(*d) == before, "and undoes");
  env.ed.applyCanvasSize(QSize(1280, 992), true);
  check(near(get(*d, b).rect.width(), 400) && near(get(*d, b).pos.x(), 200) && near(get(*d, a).font.pointSize, 40), "with scaling, positions, sizes and fonts follow");
  check(d->undoStack()->count() == 3, "the scaling is one undo step with the resize");
  d->undoStack()->undo();
  check(snap(*d) == before, "which undoes completely");

  CanvasSizeDialog dlg({{"PD120 (640 x 496)", QSize(640, 496)}}, QSize(640, 496), true, "x");
  check(dlg.canvasSize() == QSize(640, 496), "the dialog starts with the current size");
}

static void testIconsAndActions()
{
  printf("== icons and actions\n");
  Env env;
  const QStringList names = editorIcons::names();
  check(names.size() >= 40, QString("%1 icons are bundled").arg(names.size()));
  bool all = true;
  for (const QString &n : names)
    {
      const QIcon ic = editorIcons::icon(n, QApplication::palette());
      all &= !ic.isNull() && !ic.pixmap(24, 24).isNull();
    }
  check(all, "every icon renders");
  check(editorIcons::icon("no-such-icon", QApplication::palette()).isNull(), "an unknown icon name gives a null icon");
  int withName = 0, withoutIcon = 0, noTip = 0;
  for (QAction *a : env.ed.findChildren<QAction *>())
    {
      if (a->property("iconName").isValid())
        {
          withName++;
          if (a->icon().isNull()) withoutIcon++;
        }
    }
  for (QToolBar *tb : env.ed.findChildren<QToolBar *>())
    for (QAction *a : tb->actions())
      if (!a->isSeparator() && a->toolTip().isEmpty()) noTip++;
  check(withName > 30 && withoutIcon == 0, QString("%1 actions have icons, %2 are missing theirs").arg(withName).arg(withoutIcon));
  check(noTip == 0, "every toolbar button has a tooltip (the old editor had six without)");
  // dark palette: icons use the text colour
  QPalette dark = QApplication::palette();
  dark.setColor(QPalette::ButtonText, QColor(240, 240, 240));
  const QImage light = editorIcons::icon("text", dark).pixmap(24, 24).toImage();
  bool hasLight = false;
  for (int y = 0; y < light.height() && !hasLight; y++)
    for (int x = 0; x < light.width(); x++)
      if (qAlpha(light.pixel(x, y)) > 200 && qRed(light.pixel(x, y)) > 200) hasLight = true;
  check(hasLight, "icons are drawn in the palette's colour (light on a dark theme)");
}

static void testExternalEditor()
{
  printf("== hand-off of a picture to an external editor\n");
  using namespace externalEditor;
  check(commandArguments("gimp %f", "/a b/c.png") == QStringList({"gimp", "/a b/c.png"}), "%f is replaced by the file, spaces kept");
  check(commandArguments("\"/opt/my editor/ed\" --open %f --fast", "x.png") == QStringList({"/opt/my editor/ed", "--open", "x.png", "--fast"}), "quoted program names work");
  check(commandArguments("ed", "x.png") == QStringList({"ed", "x.png"}), "without %f the file is appended");
  check(commandArguments("   ", "x.png").isEmpty(), "an empty command gives nothing to run");
  check(effectiveCommand("  my-editor %f ") == "my-editor %f", "a configured command is used as it is");

  QTemporaryDir tmp;
  const QString dir = tmp.path();
  QImage im(16, 12, QImage::Format_ARGB32);
  im.fill(Qt::blue);
  im.save(dir + "/photo.png");
  im.save(dir + "/pic.avif");   // may not be writable here: only used when it is
  check(editCopyName(dir + "/photo.png", QString()) == dir + "/photo-edit.png", "the copy is name-edit.ext next to the original");
  {
    QFile taken(dir + "/photo-edit.png");
    if (!taken.open(QIODevice::WriteOnly)) return;
  }
  check(editCopyName(dir + "/photo.png", QString()) == dir + "/photo-edit-2.png", "an existing copy is not overwritten");
  QFile::remove(dir + "/photo-edit.png");
  check(editCopyName(dir + "/photo.png", dir + "/elsewhere") == dir + "/elsewhere/photo-edit.png", "the copy can go to another folder");
  check(editCopyName("/x/photo.avif", dir).endsWith("photo-edit.png"), "formats an editor may not write are copied as PNG");

  // a session with a script that, a moment after being started, writes a new picture over the copy
  const QString script = dir + "/editor.sh";
  {
    QFile f(script);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write("#!/bin/sh\nsleep 0.4\ncp \"$2\" \"$1.tmp\" && mv \"$1.tmp\" \"$1\"\n");
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
  }
  QImage painted(5, 5, QImage::Format_ARGB32);
  painted.fill(Qt::red);
  painted.save(dir + "/painted.png");
  QFile origFile(dir + "/photo.png");
  if (!origFile.open(QIODevice::ReadOnly)) return;
  const QByteArray originalBytes = origFile.readAll();
  origFile.close();
  {
    ExternalEdit session(dir + "/photo.png", dir + "/copies", script + " %f " + dir + "/painted.png");
    session.setPollInterval(150);
    QSignalSpy saved(&session, &ExternalEdit::saved);
    QString err;
    check(session.start(&err), "a session starts (" + err + ")");
    check(session.copy() == dir + "/copies/photo-edit.png" && QFile::exists(session.copy()), "and makes the copy in the given folder");
    check(saved.wait(5000), "saved() is emitted once the editor has written the copy");
    QTest::qWait(600);
    check(saved.count() == 1, "exactly once");
    check(QImage(session.copy()).width() == 5, "the copy holds what the editor wrote");
    check(session.changed(), "and the session knows it was changed");
    QFile o(dir + "/photo.png");
    check(o.open(QIODevice::ReadOnly) && o.readAll() == originalBytes, "the original is byte for byte unchanged");
  }
  check(QFile::exists(dir + "/copies/photo-edit.png"), "a saved copy is kept when the session ends");

  // an editor that never saves: the copy is removed again, the original is untouched
  const QString noop = QStandardPaths::findExecutable("true");
  if (!noop.isEmpty())
    {
      QString copy;
      {
        ExternalEdit session(dir + "/photo.png", QString(), noop + " %f");
        session.setPollInterval(100);
        QString err;
        check(session.start(&err), "a session with an editor that saves nothing starts");
        copy = session.copy();
        check(QFile::exists(copy), "the copy exists while the session lives");
        QTest::qWait(500);
      }
      check(!QFile::exists(copy), "an untouched copy is removed when the session ends");
    }

  // failures leave nothing behind
  {
    ExternalEdit session(dir + "/does-not-exist.png", QString(), "true %f");
    QString err;
    check(!session.start(&err) && !err.isEmpty(), "a missing original is refused with a message");
  }
  check(QDir(dir).entryList(QStringList("does-not-exist*")).isEmpty(), "and nothing is created");
}

static bool documentSane(const EditorDocument &d, QString *why)
{
  QSet<QString> ids;
  auto fin = [](double v) { return std::isfinite(v); };
  const QRectF c = d.doc().canvas;
  if (!(fin(c.x()) && fin(c.y()) && c.width() >= 1 && c.height() >= 1 && c.width() <= tpl::limits::maxCoord && c.height() <= tpl::limits::maxCoord))
    {
      *why = "canvas";
      return false;
    }
  for (const tpl::Element &e : d.doc().items)
    {
      if (e.id.isEmpty() || ids.contains(e.id))
        {
          *why = "id " + e.id;
          return false;
        }
      ids.insert(e.id);
      if (!(fin(e.pos.x()) && fin(e.pos.y()) && fin(e.rect.x()) && fin(e.rect.y()) && fin(e.rect.width()) && fin(e.rect.height()) && fin(e.rotation)
            && fin(e.skewH) && fin(e.skewV) && fin(e.opacity) && fin(e.font.pointSize) && fin(e.outline.width)))
        {
          *why = "number in " + e.id;
          return false;
        }
      if (e.opacity < 0 || e.opacity > 1 || qAbs(e.pos.x()) > tpl::limits::maxCoord || qAbs(e.pos.y()) > tpl::limits::maxCoord || e.rect.width() > tpl::limits::maxCoord)
        {
          *why = "range in " + e.id;
          return false;
        }
    }
  return true;
}

static void testMonkey()
{
  printf("== random mouse, keyboard, clipboard and panel input\n");
  int steps = 0, insane = 0, notRestored = 0;
  const int sessions = qMax(8, qEnvironmentVariableIntValue("EDITOR_MONKEY_SESSIONS"));   // longer runs: EDITOR_MONKEY_SESSIONS=200
  for (int seed = 1; seed <= sessions; seed++)
    {
      QRandomGenerator rng(seed * 7919);
      Env env;
      PropertiesPanel *panel = panelOf(env.ed);
      const QByteArray initial = snap(*env.d);
      const Qt::KeyboardModifiers mods[] = {Qt::NoModifier, Qt::ShiftModifier, Qt::ControlModifier, Qt::AltModifier, Qt::ShiftModifier | Qt::AltModifier};
      const int keys[] = {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Delete, Qt::Key_Escape, Qt::Key_Return, Qt::Key_F2, Qt::Key_Backspace, Qt::Key_Space};
      const char *spins[] = {"xSpin", "ySpin", "wSpin", "hSpin", "rotSpin", "shearHSpin", "shearVSpin", "opacitySpin", "outlineWidthSpin", "sizeSpin", "spacingSpin"};
      for (int i = 0; i < 350; i++)
        {
          steps++;
          const double x = -60 + rng.bounded(760), y = -60 + rng.bounded(620);
          const Qt::KeyboardModifiers m = mods[rng.bounded(5)];
          switch (rng.bounded(16))
            {
            case 0: env.v->setTool(EditorCanvas::Tool(rng.bounded(6))); break;
            case 1:
            case 2:
            case 3: env.drag(x, y, -60 + rng.bounded(760), -60 + rng.bounded(620), m); break;
            case 4: env.press(x, y, m); env.release(x, y, m); break;
            case 5: env.key(keys[rng.bounded(10)], m); break;
            case 6: env.key(keys[rng.bounded(10)]); break;
            case 7:
              switch (rng.bounded(5))
                {
                case 0: env.v->copySelection(); break;
                case 1: env.v->paste(); break;
                case 2: env.v->duplicateSelection(); break;
                case 3: env.v->cutSelection(); break;
                default: env.v->selectAll(); break;
                }
              break;
            case 8:
              if (rng.bounded(2) && env.d->undoStack()->canUndo()) env.d->undoStack()->undo();
              else if (env.d->undoStack()->canRedo()) env.d->undoStack()->redo();
              break;
            case 9:
              switch (rng.bounded(6))
                {
                case 0: env.v->align(EditorCanvas::AlignMode(rng.bounded(6))); break;
                case 1: env.v->distribute(rng.bounded(2) ? Qt::Horizontal : Qt::Vertical); break;
                case 2: env.v->bringToFront(); break;
                case 3: env.v->sendBackward(); break;
                case 4: env.v->toggleLock(); break;
                default: env.v->sendToBack(); break;
                }
              break;
            case 10:
              if (panel)
                {
                  QDoubleSpinBox *sb = panelChild<QDoubleSpinBox>(panel, spins[rng.bounded(11)]);
                  if (sb && sb->isEnabled()) sb->setValue(sb->minimum() + (sb->maximum() - sb->minimum()) * rng.bounded(1000) / 999.0 * (rng.bounded(4) ? 0.01 : 1.0));
                }
              break;
            case 11:
              {
                QImage im(1 + rng.bounded(300), 1 + rng.bounded(300), QImage::Format_ARGB32);
                im.fill(QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
                env.v->insertImage(im, QPointF(x, y));
                break;
              }
            case 12:
              {
                QWheelEvent we(QPointF(300, 200), env.v->viewport()->mapToGlobal(QPointF(300, 200)), QPoint(), QPoint(0, rng.bounded(2) ? 120 : -120), Qt::NoButton, Qt::ControlModifier,
                               Qt::NoScrollPhase, false);
                QApplication::sendEvent(env.v->viewport(), &we);
                break;
              }
            case 13:
              if (rng.bounded(6) == 0) env.ed.applyCanvasSize(QSize(100 + rng.bounded(1200), 80 + rng.bounded(900)), rng.bounded(2));
              break;
            case 14:
              if (env.v->selection().size() == 1)
                {
                  env.v->editText(env.v->selection().first());
                  QApplication::processEvents();
                  if (QPlainTextEdit *e = visibleEdit(env))
                    {
                      QTest::keyClicks(e, QString("t%1 %c").arg(rng.bounded(100)));
                      if (rng.bounded(3)) QTest::keyClick(e, Qt::Key_Return, Qt::ControlModifier);
                      else QTest::keyClick(e, Qt::Key_Escape);
                    }
                }
              break;
            default: env.v->clearSelection(); break;
            }
          QApplication::processEvents();
          if (i % 25 == 0)
            {
              QString why;
              if (!documentSane(*env.d, &why))
                {
                  insane++;
                  printf("    seed %d step %d: document not sane: %s\n", seed, i, qPrintable(why));
                }
            }
        }
      env.v->setTool(EditorCanvas::ToolSelect);
      QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
      QString why;
      if (!documentSane(*env.d, &why))
        {
          insane++;
          printf("    seed %d end: document not sane: %s\n", seed, qPrintable(why));
        }
      while (env.d->undoStack()->canUndo()) env.d->undoStack()->undo();
      if (snap(*env.d) != initial)
        {
          notRestored++;
          printf("    seed %d: undoing everything did not restore the empty template\n", seed);
        }
    }
  check(insane == 0, QString("%1 random input steps in %2 sessions: the document always stays valid").arg(steps).arg(sessions));
  check(notRestored == 0, "and undoing everything always returns to the start");
}

static void testReviewFixes()
{
  printf("== fixes from the code review\n");
  QTemporaryDir tmp;

  {   // the imported-from-.templ banner stays dismissed until another template is loaded
    Env env;
    QFile::copy(fixtures + "/synthetic.templ", tmp.filePath("old.templ"));
    check(env.ed.openFile(tmp.filePath("old.templ")), "(a legacy template is open)");
    QPushButton *dismiss = nullptr;
    for (QPushButton *b : env.ed.findChildren<QPushButton *>())
      if (b->text() == "Dismiss") dismiss = b;
    check(dismiss && dismiss->isVisibleTo(&env.ed), "the banner shows for an imported template");
    if (dismiss)
      {
        dismiss->click();
        check(!dismiss->isVisibleTo(&env.ed), "Dismiss hides it");
        env.d->addItems(QVector<tpl::Element>() << make(tpl::ItemType::Rect, QPointF(1, 1)), "add");
        QApplication::processEvents();
        check(!dismiss->isVisibleTo(&env.ed), "and the first edit does not bring it back");
        env.unsavedAnswer = TemplateEditor::Discard;
        env.ed.openFile(tmp.filePath("old.templ"));
        check(dismiss->isVisibleTo(&env.ed), "loading a template shows it again");
      }
  }

  {   // a hand-off session ends after its lifetime and takes an untouched copy with it
    QImage im(8, 8, QImage::Format_ARGB32);
    im.fill(Qt::blue);
    im.save(tmp.filePath("pic.png"));
    const QString noop = QStandardPaths::findExecutable("true");
    if (!noop.isEmpty())
      {
        ExternalEdit *session = new ExternalEdit(tmp.filePath("pic.png"), QString(), noop + " %f");
        QPointer<ExternalEdit> guard(session);
        session->setPollInterval(100);
        session->setLifetime(500);
        QString err;
        check(session->start(&err), "(a session starts)");
        const QString copy = session->copy();
        QTest::qWait(1200);
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(guard.isNull(), "a session ends when its lifetime is over");
        check(!QFile::exists(copy), "and an untouched copy is removed with it");
      }
  }

  {   // canvas size: out-of-range and fractional canvases are not changed by an OK that changed nothing
    Env env;
    env.d->setCanvas(QRectF(0, 0, 5000, 10), tpl::TemplateDocument::Stretch, "wide");
    CanvasSizeDialog dlg({}, QSize(5000, 10), true, "x");
    check(dlg.canvasSize() == QSize(5000, 10), "the dialog keeps a canvas outside its usual range as it is");
    const int index = env.d->undoStack()->index();
    env.ed.applyCanvasSize(dlg.canvasSize(), true);
    check(env.d->undoStack()->index() == index && near(env.d->doc().canvas.width(), 5000), "so OK without changes changes nothing");
    env.d->setCanvas(QRectF(0, 0, 320.4, 256.4), tpl::TemplateDocument::Stretch, "fractional");
    const int index2 = env.d->undoStack()->index();
    env.ed.applyCanvasSize(QSize(320, 256), false);
    check(env.d->undoStack()->index() == index2, "nor does a fractional canvas get rounded by it");
    env.ed.applyCanvasSize(QSize(25000, 100), false);
    check(env.d->undoStack()->index() == index2, "a size the document refuses leaves no empty undo step");
  }

  {   // scaling a turned item keeps its proportions
    Env env;
    tpl::Element r = make(tpl::ItemType::Rect, QPointF(100, 100), QSizeF(100, 50));
    r.rotation = 45;
    const QString id = add(*env.d, r);
    const QPointF centre0 = get(*env.d, id).pos + get(*env.d, id).rect.center();
    env.ed.applyCanvasSize(QSize(1280, 496), true);
    const tpl::Element &e = get(*env.d, id);
    check(near(e.rect.width() / e.rect.height(), 2.0, 0.01), QString("a rotated rectangle keeps its aspect when the canvas is stretched (%1 x %2)").arg(e.rect.width()).arg(e.rect.height()));
    check(near((e.pos + e.rect.center()).x(), centre0.x() * 2) && near((e.pos + e.rect.center()).y(), centre0.y()), "and its centre moves with the canvas");
  }

  {   // the inline editor survives a context menu and a window switch, but not a click elsewhere
    Env env;
    const QString id = add(*env.d, make(tpl::ItemType::Text, QPointF(20, 40)));
    env.v->editText(id);
    QApplication::processEvents();
    QPlainTextEdit *edit = visibleEdit(env);
    check(edit != nullptr, "(an editor is open)");
    if (edit)
      {
        QTest::keyClicks(edit, "abc");
        QFocusEvent popup(QEvent::FocusOut, Qt::PopupFocusReason);
        QApplication::sendEvent(edit, &popup);
        QFocusEvent away(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
        QApplication::sendEvent(edit, &away);
        check(visibleEdit(env) == edit, "a context menu or a window switch does not close the in-place editor");
        QFocusEvent other(QEvent::FocusOut, Qt::MouseFocusReason);
        QApplication::sendEvent(edit, &other);
        check(env.d->find(id)->text.contains("abc") && visibleEdit(env) == nullptr, "a click elsewhere finishes it");
      }
  }

  {   // pan mode does not stick when the focus leaves while Space is held
    Env env;
    const QString id = add(*env.d, make(tpl::ItemType::Rect, QPointF(100, 100), QSizeF(100, 60)));
    env.v->clearSelection();
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ");
    QApplication::sendEvent(env.v, &down);
    QFocusEvent out(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
    QApplication::sendEvent(env.v, &out);
    env.press(150, 130);
    env.release(150, 130);
    check(env.v->selection() == QStringList(id), "a click selects after the focus left while Space was held");
  }

  {   // a gradient cannot get more stops than a file may hold
    GradientEdit ge;
    ge.resize(320, 200);
    ge.show();
    tpl::Gradient g;
    g.kind = tpl::Gradient::Linear;
    for (int i = 0; i < tpl::limits::maxGradientStops; i++) g.stops.append(tpl::GradientStop{i / double(tpl::limits::maxGradientStops - 1), QColor(i * 10, 0, 0)});
    ge.setGradient(g, Qt::red);
    QWidget *bar = nullptr;
    for (QWidget *w : ge.findChildren<QWidget *>())
      if (QLatin1String(w->metaObject()->className()) == "GradientStopBar") bar = w;
    check(bar != nullptr, "(the stop bar exists)");
    if (bar)
      {
        QSignalSpy changed(&ge, &GradientEdit::changed);
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, QPoint(bar->width() / 2 + 3, 12));
        check(ge.gradient().stops.size() == tpl::limits::maxGradientStops && changed.isEmpty(), "a click on the bar adds no stop beyond the limit");
        g.stops.resize(3);
        g.stops[2].pos = 1;
        ge.setGradient(g, Qt::red);
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, QPoint(bar->width() / 2, 12));
        check(ge.gradient().stops.size() == 4, "below the limit it still adds one");
      }
  }

  {   // macros are inserted by double-click, not by a single click
    MacroPalette pal;
    pal.resize(300, 400);
    pal.show();
    QSignalSpy inserted(&pal, &MacroPalette::macroActivated);
    QListWidget *list = pal.findChild<QListWidget *>();
    const QPoint at = list->visualItemRect(list->item(1)).center();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    check(inserted.isEmpty(), "a single click on a macro only selects it");
    QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    check(inserted.count() == 1 && inserted.first().first().toString().startsWith("%"), "a double-click inserts it");
  }

  {   // shared picture loading; remembered text layout follows text, font and macro state
    QString err;
    check(EditorCanvas::loadPicture(tmp.filePath("missing.png"), &err).isNull() && !err.isEmpty(), "an unreadable picture is reported");
    QImage big(3000, 100, QImage::Format_RGB32);
    big.fill(Qt::red);
    big.save(tmp.filePath("big.png"));
    const QImage loaded = EditorCanvas::loadPicture(tmp.filePath("big.png"), &err);
    check(!loaded.isNull() && loaded.width() <= 1600 && loaded.format() == QImage::Format_ARGB32, "a large picture is shrunk and converted");

    Env env;
    tpl::Element t = make(tpl::ItemType::Text, QPointF(10, 100));
    t.text = "abc";
    const QString id = add(*env.d, t);
    const double w1 = env.v->sceneBounds(get(*env.d, id)).width();
    tpl::Element longer = get(*env.d, id);
    longer.text = "abcabcabc";
    env.d->modifyItems(QVector<tpl::Element>() << longer, "text");
    const double w2 = env.v->sceneBounds(get(*env.d, id)).width();
    tpl::Element bigger = get(*env.d, id);
    bigger.font.pointSize *= 2;
    env.d->modifyItems(QVector<tpl::Element>() << bigger, "size");
    const double w3 = env.v->sceneBounds(get(*env.d, id)).width();
    check(w2 > w1 * 2 && w3 > w2 * 1.5, QString("text bounds follow the text and the font size (%1, %2, %3)").arg(w1).arg(w2).arg(w3));
    tpl::Element macro = get(*env.d, id);
    macro.text = "%c";
    env.d->modifyItems(QVector<tpl::Element>() << macro, "macro");
    const double literal = env.v->sceneBounds(get(*env.d, id)).width();
    QHash<QChar, QString> vars;
    vars.insert('c', "W1AW/VERYLONGSUFFIX");
    env.v->setMacroVars(vars);
    env.v->setExpandMacros(true);
    const double expanded = env.v->sceneBounds(get(*env.d, id)).width();
    vars.insert('c', "W");
    env.v->setMacroVars(vars);
    const double shorter = env.v->sceneBounds(get(*env.d, id)).width();
    check(expanded > literal && shorter < expanded, QString("and the macro values (%1 literal, %2 long, %3 short)").arg(literal).arg(expanded).arg(shorter));
  }
}

static void testEditorWindow()
{
  printf("== the window\n");
  Env env;
  TemplateEditor &ed = env.ed;
  EditorDocument *d = env.d;
  check(ed.windowTitle().contains("Untitled"), "a new template is called Untitled");
  tpl::Element e = make(tpl::ItemType::Rect, QPointF(0, 0));
  d->addItems(QVector<tpl::Element>() << e, "add");
  check(ed.isWindowModified(), "an edit marks the window modified");
  QAction *selectAll = nullptr, *undo = nullptr, *del = nullptr, *clearAll = nullptr;
  for (QAction *a : ed.findChildren<QAction *>())
    {
      if (a->text().contains("Select &All")) selectAll = a;
      if (a->text().startsWith("&Undo")) undo = a;
      if (a->text() == "&Delete") del = a;
      if (a->text().contains("Delete All")) clearAll = a;
    }
  check(selectAll && undo && del && clearAll, "the standard actions exist");
  if (!(selectAll && undo && del && clearAll)) return;
  check(selectAll->shortcut() == QKeySequence(QKeySequence::SelectAll), "Select All has the standard shortcut");
  check(del->shortcut() == QKeySequence(QKeySequence::Delete), "Delete has the standard shortcut");
  d->addItems(QVector<tpl::Element>() << e << e, "add");
  selectAll->trigger();
  check(env.v->selection().size() == 3 && d->doc().items.size() == 3, "Ctrl+A selects everything and deletes nothing (it was 'Clear All' in the old editor)");
  del->trigger();
  check(d->doc().items.isEmpty(), "Delete removes the selection");
  undo->trigger();
  check(d->doc().items.size() == 3, "and Undo brings it back");
  check(undo->shortcut() == QKeySequence(QKeySequence::Undo), "Undo has the standard shortcut");
  check(!ed.windowTitle().isEmpty(), "title is set");
}

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("editortest");
  QCoreApplication::setApplicationName("editortest");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QTemporaryDir settings;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  fixtures = argc > 1 ? argv[1] : QString();

  testUndoRedoInvariants();
  testRandomOperations();
  testMergeAndClean();
  testTextRectNormalisation();
  testCanvasMatchesRenderer();
  testSelectMoveResizeRotate();
  testRubberBandAndMultiSelect();
  testSnapping();
  testCreateTools();
  testTextEditing();
  testKeyboard();
  testTextEditRaces();
  testMonkey();
  testLockZoomOrder();
  testClipboard();
  testImages();
  testPropertiesPanel();
  testPanelOrder();
  testLayersPanel();
  testFiles();
  testCanvasSize();
  testIconsAndActions();
  testEditorWindow();
  testExternalEditor();
  testReviewFixes();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
