// Renders the template editor offscreen into PNG files, for looking at the layout and for the User's Guide screenshots:
//   shot <out-dir> <fixtures-dir> [dark]
// Not part of the test run (tests/editor/run.sh); build with: tests/editor/build.sh <out> tests/editor/shot.cpp
#include <QtWidgets>
#include "editoricons.h"
#include "templateeditor.h"
#include "templatevars.h"

static void darkPalette(QApplication &app)
{
  app.setStyle("Fusion");
  QPalette p;
  p.setColor(QPalette::Window, QColor(45, 45, 48));
  p.setColor(QPalette::WindowText, QColor(230, 230, 230));
  p.setColor(QPalette::Base, QColor(30, 30, 32));
  p.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
  p.setColor(QPalette::Text, QColor(230, 230, 230));
  p.setColor(QPalette::Button, QColor(55, 55, 58));
  p.setColor(QPalette::ButtonText, QColor(230, 230, 230));
  p.setColor(QPalette::Highlight, QColor(42, 130, 218));
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::ToolTipBase, QColor(60, 60, 60));
  p.setColor(QPalette::ToolTipText, Qt::white);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
  app.setPalette(p);
}

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("shot");
  QCoreApplication::setApplicationName("shot");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QTemporaryDir settings;
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  const QString out = argc > 1 ? argv[1] : ".";
  const QString fixtures = argc > 2 ? argv[2] : ".";
  if (argc > 3 && QString(argv[3]) == "dark") darkPalette(app);
  QDir().mkpath(out);
  const int fid = QFontDatabase::addApplicationFont(fixtures + "/Montserrat-Bold.otf");
  const QString family = fid >= 0 ? QFontDatabase::applicationFontFamilies(fid).value(0) : QString();

  TemplateEditor ed;
  ed.resize(1280, 800);
  QVector<SizePreset> presets;
  presets.append({"PD120, PD180 (640 × 496)", QSize(640, 496)});
  ed.setSizePresets(presets);
  ed.newTemplate(QSize(640, 496));
  EditorDocument *d = ed.document();
  EditorCanvas *c = ed.canvas();

  // a picture to show the template over
  QImage pic(640, 496, QImage::Format_ARGB32);
  {
    QPainter p(&pic);
    QLinearGradient sky(0, 0, 0, 496);
    sky.setColorAt(0, QColor(40, 90, 160));
    sky.setColorAt(0.65, QColor(170, 205, 235));
    sky.setColorAt(0.65, QColor(70, 120, 60));
    sky.setColorAt(1, QColor(40, 80, 40));
    p.fillRect(pic.rect(), sky);
    p.setBrush(QColor(240, 240, 235));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(450, 120), 70, 40);
    p.drawEllipse(QPointF(500, 135), 60, 35);
    p.setBrush(QColor(110, 90, 70));
    p.drawPolygon(QPolygonF() << QPointF(120, 320) << QPointF(220, 220) << QPointF(320, 320));
  }
  ed.setPreviewPicture(pic);
  ed.setSampleValues(tpl::sampleVars());

  auto fnt = [&](tpl::Element &e, double pt) {
    if (!family.isEmpty()) e.font.family = family;
    e.font.pointSize = pt;
    e.font.weight = 700;
  };
  tpl::Element bar = EditorCanvas::defaultElement(tpl::ItemType::Rect);
  bar.pos = QPointF(0, 422);
  bar.rect = QRectF(0, 0, 640, 74);
  bar.fill = QColor(10, 20, 40, 190);
  bar.gradient.kind = tpl::Gradient::Linear;
  bar.gradient.angle = 90;
  bar.gradient.stops << tpl::GradientStop{0, QColor(10, 20, 40, 120)} << tpl::GradientStop{1, QColor(10, 20, 40, 230)};
  bar.id = "bar";
  tpl::Element call = EditorCanvas::defaultElement(tpl::ItemType::Text);
  call.text = "%c de %m";
  call.pos = QPointF(14, 484);
  fnt(call, 26);
  call.id = "call";
  tpl::Element rst = EditorCanvas::defaultElement(tpl::ItemType::Text);
  rst.text = "RSV %r   %d %t UTC";
  rst.pos = QPointF(14, 446);
  fnt(rst, 13);
  rst.outline.width = 1;
  rst.id = "rsv";
  tpl::Element replay = EditorCanvas::defaultElement(tpl::ItemType::Replay);
  replay.pos = QPointF(436, 16);
  replay.rect = QRectF(0, 0, 188, 141);
  replay.id = "replay";
  tpl::Element qth = EditorCanvas::defaultElement(tpl::ItemType::Text);
  qth.text = "%q  %l";
  qth.pos = QPointF(380, 484);
  fnt(qth, 14);
  qth.id = "qth";
  d->addItems(QVector<tpl::Element>() << bar << call << rst << replay << qth, "build");
  d->undoStack()->clear();
  d->markSaved();
  qApp->processEvents();
  c->zoomFit();
  qApp->processEvents();
  c->setSelection(QStringList("call"));
  ed.show();
  for (int i = 0; i < 6; i++) qApp->processEvents();
  ed.grab().save(out + "/editor-selected.png");

  // preview of macros over the picture
  for (QAction *a : ed.findChildren<QAction *>())
    {
      if (a->text().contains("Preview &Macros")) a->setChecked(true);
      if (a->text().contains("Show Picture &Under")) a->setChecked(true);
    }
  c->clearSelection();
  for (int i = 0; i < 6; i++) qApp->processEvents();
  ed.grab().save(out + "/editor-preview.png");

  // several items selected, gradient item
  c->setSelection(QStringList() << "bar" << "call");
  for (int i = 0; i < 6; i++) qApp->processEvents();
  ed.grab().save(out + "/editor-multi.png");
  c->setSelection(QStringList("bar"));
  for (int i = 0; i < 6; i++) qApp->processEvents();
  ed.grab().save(out + "/editor-gradient.png");

  // the icon sheet
  QWidget sheet;
  QGridLayout *g = new QGridLayout(&sheet);
  int n = 0;
  for (const QString &name : editorIcons::names())
    {
      QLabel *l = new QLabel;
      l->setPixmap(editorIcons::icon(name, app.palette()).pixmap(32, 32));
      QLabel *t = new QLabel(name);
      QFont f = t->font();
      f.setPointSize(7);
      t->setFont(f);
      QVBoxLayout *v = new QVBoxLayout;
      v->addWidget(l, 0, Qt::AlignHCenter);
      v->addWidget(t, 0, Qt::AlignHCenter);
      QWidget *cell = new QWidget;
      cell->setLayout(v);
      g->addWidget(cell, n / 10, n % 10);
      n++;
    }
  sheet.show();
  qApp->processEvents();
  sheet.grab().save(out + "/icons.png");
  ed.setUnsavedHandler([]() { return TemplateEditor::Discard; });
  return 0;
}
