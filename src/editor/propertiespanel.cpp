#include "propertiespanel.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include "gradientedit.h"
#include "templatevars.h"

namespace
{

/*! the item's unrotated frame in canvas coordinates (what the X/Y/W/H fields show) */
QRectF frameOf(const tpl::Element &e)
{
  const QRectF local = e.type == tpl::ItemType::Text ? e.rect : tpl::localBounds(e);
  return local.translated(e.pos);
}

double roundTo(double v, int decimals)
{
  const double k = std::pow(10.0, decimals);
  return std::round(v * k) / k;
}

void setValueIfChanged(QDoubleSpinBox *w, double v)
{
  if (qAbs(w->value() - roundTo(v, w->decimals())) > 1e-9) w->setValue(v);
}

}  // namespace

PropertiesPanel::PropertiesPanel(EditorDocument *document, EditorCanvas *editorCanvas, QWidget *parent)
  : QScrollArea(parent), doc(document), canvas(editorCanvas)
{
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  build();
  connect(canvas, &EditorCanvas::selectionChanged, this, &PropertiesPanel::refresh);
  // an item changes on every mouse move of a drag: refresh once per event loop turn
  refreshTimer.setSingleShot(true);
  connect(&refreshTimer, &QTimer::timeout, this, &PropertiesPanel::refresh);
  connect(doc, &EditorDocument::itemChanged, this, [this]() { refreshTimer.start(0); });
  connect(doc, &EditorDocument::reset, this, [this]() { refreshTimer.start(0); });
  connect(qApp, &QApplication::focusChanged, this, [this]() { doc->breakMerge(); });   // typing in another field starts a new undo step
  refresh();
}

QDoubleSpinBox *PropertiesPanel::spin(double min, double max, double step, int decimals, const QString &suffix)
{
  QDoubleSpinBox *s = new QDoubleSpinBox;
  s->setRange(min, max);
  s->setSingleStep(step);
  s->setDecimals(decimals);
  s->setSuffix(suffix);
  s->setKeyboardTracking(true);
  s->setAccelerated(true);
  return s;
}

QPushButton *PropertiesPanel::swatch(const QString &tip)
{
  QPushButton *b = new QPushButton;
  b->setToolTip(tip);
  b->setMinimumWidth(60);
  return b;
}

void PropertiesPanel::setSwatch(QPushButton *b, const QColor &c)
{
  QPixmap pm(40, 16);
  pm.fill(Qt::white);
  QPainter p(&pm);
  p.fillRect(0, 0, 20, 8, QColor(0xcc, 0xcc, 0xcc));
  p.fillRect(20, 8, 20, 8, QColor(0xcc, 0xcc, 0xcc));
  p.fillRect(pm.rect(), c);
  p.setPen(palette().color(QPalette::Mid));
  p.setBrush(Qt::NoBrush);
  p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
  b->setIcon(QIcon(pm));
  b->setIconSize(pm.size());
  b->setText(QString());
}

QColor PropertiesPanel::pickColor(const QColor &initial, const QString &title)
{
  return QColorDialog::getColor(initial, this, title, QColorDialog::ShowAlphaChannel);
}

void PropertiesPanel::apply(const Edit &fn, const QString &text, const QString &mergeKey)
{
  if (updating) return;
  QVector<tpl::Element> els = canvas->selectedElements();
  if (els.isEmpty()) return;
  for (tpl::Element &e : els) fn(e);
  doc->modifyItems(els, text, mergeKey);
}

void PropertiesPanel::build()
{
  QWidget *content = new QWidget;
  QVBoxLayout *top = new QVBoxLayout(content);
  top->setContentsMargins(6, 6, 6, 6);

  emptyLabel = new QLabel(tr("Select an item on the canvas or in the Layers list to edit it.\n\nUse the tools on the left to add text, shapes, a picture or the replay window."));
  emptyLabel->setWordWrap(true);
  emptyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  top->addWidget(emptyLabel);

  // --- position and size -----------------------------------------------------------------------------------
  geometryBox = new QGroupBox(tr("Position and size"));
  QGridLayout *g = new QGridLayout(geometryBox);
  xSpin = spin(-20000, 20000, 1, 1);
  ySpin = spin(-20000, 20000, 1, 1);
  wSpin = spin(1, 20000, 1, 1);
  hSpin = spin(1, 20000, 1, 1);
  rotSpin = spin(-180, 180, 1, 1, QStringLiteral("°"));
  rotSpin->setWrapping(true);
  shearHSpin = spin(-5, 5, 0.05, 2);
  shearVSpin = spin(-5, 5, 0.05, 2);
  opacitySpin = spin(0, 100, 5, 0, QStringLiteral(" %"));
  lockedCheck = new QCheckBox(tr("Locked"));
  xSpin->setToolTip(tr("Distance of the left edge from the left side of the picture, in picture pixels"));
  ySpin->setToolTip(tr("Distance of the top edge from the top of the picture, in picture pixels"));
  wSpin->setToolTip(tr("Width (text items are sized by their font size)"));
  hSpin->setToolTip(tr("Height (text items are sized by their font size; a line's thickness is its outline width)"));
  rotSpin->setToolTip(tr("Rotation about the centre of the item. Shift while dragging the rotate handle snaps to 15°"));
  shearHSpin->setToolTip(tr("Horizontal skew"));
  shearVSpin->setToolTip(tr("Vertical skew"));
  opacitySpin->setToolTip(tr("0 % is invisible, 100 % is solid"));
  lockedCheck->setToolTip(tr("A locked item cannot be moved or clicked on the canvas (select it in the Layers list to unlock it)"));
  g->addWidget(new QLabel(tr("X")), 0, 0);
  g->addWidget(xSpin, 0, 1);
  g->addWidget(new QLabel(tr("Y")), 0, 2);
  g->addWidget(ySpin, 0, 3);
  g->addWidget(new QLabel(tr("W")), 1, 0);
  g->addWidget(wSpin, 1, 1);
  g->addWidget(new QLabel(tr("H")), 1, 2);
  g->addWidget(hSpin, 1, 3);
  g->addWidget(new QLabel(tr("Rotate")), 2, 0);
  g->addWidget(rotSpin, 2, 1);
  g->addWidget(new QLabel(tr("Opacity")), 2, 2);
  g->addWidget(opacitySpin, 2, 3);
  g->addWidget(new QLabel(tr("Skew H")), 3, 0);
  g->addWidget(shearHSpin, 3, 1);
  g->addWidget(new QLabel(tr("Skew V")), 3, 2);
  g->addWidget(shearVSpin, 3, 3);
  g->addWidget(lockedCheck, 4, 0, 1, 4);
  top->addWidget(geometryBox);

  // --- fill ------------------------------------------------------------------------------------------------
  fillBox = new QGroupBox(tr("Fill"));
  QVBoxLayout *fl = new QVBoxLayout(fillBox);
  QHBoxLayout *fr = new QHBoxLayout;
  fr->addWidget(new QLabel(tr("Colour")));
  fillButton = swatch(tr("Fill colour (click to change)"));
  fr->addWidget(fillButton);
  fr->addStretch();
  fl->addLayout(fr);
  gradientEdit = new GradientEdit;
  fl->addWidget(gradientEdit);
  top->addWidget(fillBox);

  // --- outline ---------------------------------------------------------------------------------------------
  outlineBox = new QGroupBox(tr("Outline"));
  QFormLayout *ol = new QFormLayout(outlineBox);
  outlineColorButton = swatch(tr("Outline colour (click to change)"));
  outlineWidthSpin = spin(0, 200, 0.5, 1, QStringLiteral(" px"));
  outlineStyleCombo = new QComboBox;
  outlineStyleCombo->addItem(tr("None"), int(Qt::NoPen));
  outlineStyleCombo->addItem(tr("Solid"), int(Qt::SolidLine));
  outlineStyleCombo->addItem(tr("Dashed"), int(Qt::DashLine));
  outlineStyleCombo->addItem(tr("Dotted"), int(Qt::DotLine));
  outlineStyleCombo->addItem(tr("Dash-dot"), int(Qt::DashDotLine));
  outlineJoinCombo = new QComboBox;
  outlineJoinCombo->addItem(tr("Round"), int(Qt::RoundJoin));
  outlineJoinCombo->addItem(tr("Mitre"), int(Qt::MiterJoin));
  outlineJoinCombo->addItem(tr("Bevel"), int(Qt::BevelJoin));
  outlineJoinLabel = new QLabel(tr("Corners"));
  ol->addRow(tr("Colour"), outlineColorButton);
  ol->addRow(tr("Width"), outlineWidthSpin);
  ol->addRow(tr("Style"), outlineStyleCombo);
  ol->addRow(outlineJoinLabel, outlineJoinCombo);
  top->addWidget(outlineBox);

  // --- text ------------------------------------------------------------------------------------------------
  textBox = new QGroupBox(tr("Text"));
  QVBoxLayout *tl = new QVBoxLayout(textBox);
  textEdit = new QPlainTextEdit;
  textEdit->setMinimumHeight(60);
  textEdit->setMaximumHeight(110);
  textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
  textEdit->setToolTip(tr("The text. Use %c, %m and the other macros (Macros tab) for values that change with every transmission"));
  macroWarning = new QLabel;
  macroWarning->setWordWrap(true);
  macroWarning->setStyleSheet(QStringLiteral("color: #c07000;"));
  tl->addWidget(textEdit);
  tl->addWidget(macroWarning);
  fontCombo = new QFontComboBox;
  fontCombo->setEditable(true);
  fontCombo->setToolTip(tr("Font family"));
  fontWarning = new QLabel;
  fontWarning->setWordWrap(true);
  fontWarning->setStyleSheet(QStringLiteral("color: #c07000;"));
  sizeSpin = spin(1, 1000, 1, 1, QStringLiteral(" pt"));
  sizeSpin->setToolTip(tr("Font size. Dragging a corner handle of a text item scales it too"));
  weightCombo = new QComboBox;
  const struct { const char *name; int weight; } weights[] = {{QT_TR_NOOP("Thin"), 100}, {QT_TR_NOOP("Extra light"), 200}, {QT_TR_NOOP("Light"), 300},
                                                                {QT_TR_NOOP("Regular"), 400}, {QT_TR_NOOP("Medium"), 500}, {QT_TR_NOOP("Semi-bold"), 600},
                                                                {QT_TR_NOOP("Bold"), 700}, {QT_TR_NOOP("Extra bold"), 800}, {QT_TR_NOOP("Black"), 900}};
  for (const auto &w : weights) weightCombo->addItem(tr(w.name), w.weight);
  weightCombo->setToolTip(tr("Font weight"));
  italicCheck = new QCheckBox(tr("Italic"));
  underlineCheck = new QCheckBox(tr("Underline"));
  strikeCheck = new QCheckBox(tr("Strike out"));
  spacingSpin = spin(-100, 1000, 0.5, 1, QStringLiteral(" px"));
  spacingSpin->setToolTip(tr("Extra space between letters"));
  tl->addWidget(fontCombo);
  tl->addWidget(fontWarning);
  QFormLayout *fontForm = new QFormLayout;
  fontForm->addRow(tr("Size"), sizeSpin);
  fontForm->addRow(tr("Weight"), weightCombo);
  fontForm->addRow(tr("Letter spacing"), spacingSpin);
  tl->addLayout(fontForm);
  QHBoxLayout *styleRow = new QHBoxLayout;
  styleRow->addWidget(italicCheck);
  styleRow->addWidget(underlineCheck);
  styleRow->addWidget(strikeCheck);
  tl->addLayout(styleRow);
  top->addWidget(textBox);

  // --- picture -----------------------------------------------------------------------------------------------
  pictureBox = new QGroupBox(tr("Picture"));
  QVBoxLayout *pl = new QVBoxLayout(pictureBox);
  pictureInfo = new QLabel;
  pictureInfo->setWordWrap(true);
  replaceImageButton = new QPushButton(tr("Replace picture…"));
  pl->addWidget(pictureInfo);
  pl->addWidget(replaceImageButton);
  top->addWidget(pictureBox);
  top->addStretch();
  setWidget(content);

  // --- connections -------------------------------------------------------------------------------------------
  auto frameNow = [this]() {
    const QVector<tpl::Element> sel = canvas->selectedElements();
    if (sel.size() == 1) return frameOf(sel.first());
    QRectF r;
    for (const tpl::Element &e : sel) r = r.isNull() ? canvas->sceneBounds(e) : r.united(canvas->sceneBounds(e));
    return r;
  };
  connect(xSpin, &QDoubleSpinBox::valueChanged, this, [this, frameNow](double v) {
    if (updating) return;
    const double d = v - frameNow().x();
    apply([d](tpl::Element &e) { if (!e.locked) e.pos.rx() += d; }, tr("Move"), QStringLiteral("x"));
  });
  connect(ySpin, &QDoubleSpinBox::valueChanged, this, [this, frameNow](double v) {
    if (updating) return;
    const double d = v - frameNow().y();
    apply([d](tpl::Element &e) { if (!e.locked) e.pos.ry() += d; }, tr("Move"), QStringLiteral("y"));
  });
  connect(wSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) {
      if (e.locked || e.type == tpl::ItemType::Text) return;
      QRectF r = e.rect;
      r.setWidth(v);
      EditorDocument::setRectKeeping(e, r, r.topLeft());
    }, tr("Resize"), QStringLiteral("w"));
  });
  connect(hSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) {
      if (e.locked || e.type == tpl::ItemType::Text || e.type == tpl::ItemType::Line) return;
      QRectF r = e.rect;
      r.setHeight(v);
      EditorDocument::setRectKeeping(e, r, r.topLeft());
    }, tr("Resize"), QStringLiteral("h"));
  });
  connect(rotSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { if (!e.locked) e.rotation = v; }, tr("Rotate"), QStringLiteral("rot"));
  });
  connect(shearHSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { if (!e.locked) e.skewH = v; }, tr("Skew"), QStringLiteral("shearh"));
  });
  connect(shearVSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { if (!e.locked) e.skewV = v; }, tr("Skew"), QStringLiteral("shearv"));
  });
  connect(opacitySpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { e.opacity = v / 100.0; }, tr("Change opacity"), QStringLiteral("opacity"));
  });
  connect(lockedCheck, &QCheckBox::toggled, this, [this](bool on) {
    apply([on](tpl::Element &e) { e.locked = on; }, on ? tr("Lock") : tr("Unlock"), QString());
  });

  connect(fillButton, &QPushButton::clicked, this, [this]() {
    const QVector<tpl::Element> sel = canvas->selectedElements();
    if (sel.isEmpty()) return;
    const QColor c = pickColor(sel.first().fill, tr("Fill colour"));
    if (!c.isValid()) return;
    apply([c](tpl::Element &e) { e.fill = c; }, tr("Change fill"), QString());
  });
  connect(gradientEdit, &GradientEdit::changed, this, [this](const tpl::Gradient &gr) {
    apply([gr](tpl::Element &e) { e.gradient = gr; }, tr("Change gradient"), QStringLiteral("gradient"));
  });

  connect(outlineColorButton, &QPushButton::clicked, this, [this]() {
    const QVector<tpl::Element> sel = canvas->selectedElements();
    if (sel.isEmpty()) return;
    const QColor c = pickColor(sel.first().outline.color, tr("Outline colour"));
    if (!c.isValid()) return;
    apply([c](tpl::Element &e) { e.outline.color = c; }, tr("Change outline"), QString());
  });
  connect(outlineWidthSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { e.outline.width = v; }, tr("Change outline"), QStringLiteral("owidth"));
  });
  connect(outlineStyleCombo, &QComboBox::activated, this, [this](int i) {
    const Qt::PenStyle st = Qt::PenStyle(outlineStyleCombo->itemData(i).toInt());
    apply([st](tpl::Element &e) { e.outline.style = st; }, tr("Change outline"), QString());
  });
  connect(outlineJoinCombo, &QComboBox::activated, this, [this](int i) {
    const Qt::PenJoinStyle js = Qt::PenJoinStyle(outlineJoinCombo->itemData(i).toInt());
    apply([js](tpl::Element &e) { e.outline.join = js; }, tr("Change outline"), QString());
  });

  connect(textEdit, &QPlainTextEdit::textChanged, this, [this]() {
    if (updating) return;
    const QString t = textEdit->toPlainText();
    apply([t](tpl::Element &e) { e.text = t; }, tr("Edit text"), QStringLiteral("text"));
  });
  connect(fontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
    const QString fam = f.family();
    apply([fam](tpl::Element &e) { e.font.family = fam; }, tr("Change font"), QString());
  });
  connect(sizeSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { e.font.pointSize = v; }, tr("Change font size"), QStringLiteral("fsize"));
  });
  connect(weightCombo, &QComboBox::activated, this, [this](int i) {
    const int w = weightCombo->itemData(i).toInt();
    apply([w](tpl::Element &e) { e.font.weight = w; }, tr("Change font"), QString());
  });
  connect(italicCheck, &QCheckBox::toggled, this, [this](bool on) { apply([on](tpl::Element &e) { e.font.italic = on; }, tr("Change font"), QString()); });
  connect(underlineCheck, &QCheckBox::toggled, this, [this](bool on) { apply([on](tpl::Element &e) { e.font.underline = on; }, tr("Change font"), QString()); });
  connect(strikeCheck, &QCheckBox::toggled, this, [this](bool on) { apply([on](tpl::Element &e) { e.font.strikeOut = on; }, tr("Change font"), QString()); });
  connect(spacingSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) {
    apply([v](tpl::Element &e) { e.font.letterSpacing = v; }, tr("Change letter spacing"), QStringLiteral("spacing"));
  });
  connect(replaceImageButton, &QPushButton::clicked, this, &PropertiesPanel::replaceImage);

  // object names: used by the tests and by anything that automates the editor
  const struct { QObject *obj; const char *name; } names[] = {
    {xSpin, "xSpin"}, {ySpin, "ySpin"}, {wSpin, "wSpin"}, {hSpin, "hSpin"}, {rotSpin, "rotSpin"}, {shearHSpin, "shearHSpin"},
    {shearVSpin, "shearVSpin"}, {opacitySpin, "opacitySpin"}, {lockedCheck, "lockedCheck"}, {fillButton, "fillButton"},
    {gradientEdit, "gradientEdit"}, {outlineColorButton, "outlineColorButton"}, {outlineWidthSpin, "outlineWidthSpin"},
    {outlineStyleCombo, "outlineStyleCombo"}, {outlineJoinCombo, "outlineJoinCombo"}, {textEdit, "textEdit"},
    {macroWarning, "macroWarning"}, {fontWarning, "fontWarning"}, {fontCombo, "fontCombo"}, {sizeSpin, "sizeSpin"},
    {weightCombo, "weightCombo"}, {italicCheck, "italicCheck"}, {spacingSpin, "spacingSpin"}, {pictureInfo, "pictureInfo"},
    {geometryBox, "geometryBox"}, {fillBox, "fillBox"}, {outlineBox, "outlineBox"}, {textBox, "textBox"}, {pictureBox, "pictureBox"}};
  for (const auto &n : names) n.obj->setObjectName(QLatin1String(n.name));
}

bool PropertiesPanel::insertMacro(const QString &macro)
{
  const QVector<tpl::Element> sel = canvas->selectedElements();
  if (sel.size() != 1 || sel.first().type != tpl::ItemType::Text || sel.first().locked) return false;
  textEdit->setFocus();
  textEdit->insertPlainText(macro);   // textChanged() applies it
  return true;
}

void PropertiesPanel::replaceImage()
{
  const QVector<tpl::Element> sel = canvas->selectedElements();
  if (sel.size() != 1 || sel.first().type != tpl::ItemType::Image) return;
  const QString fn = QFileDialog::getOpenFileName(this, tr("Replace picture"), QString(),
                                                  tr("Pictures (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.avif *.heic *.svg);;All files (*)"));
  if (fn.isEmpty()) return;
  QString error;
  const QImage img = EditorCanvas::loadPicture(fn, &error);
  if (img.isNull())
    {
      QMessageBox::warning(this, tr("Replace picture"), error);
      return;
    }
  apply([img](tpl::Element &e) {
    e.image = img;
    // keep the left edge and the width, follow the new picture's proportions
    QRectF r = e.rect;
    r.setHeight(qMax(1.0, r.width() * img.height() / img.width()));
    EditorDocument::setRectKeeping(e, r, r.topLeft());
  }, tr("Replace picture"), QString());
}

void PropertiesPanel::refresh()
{
  const QVector<tpl::Element> sel = canvas->selectedElements();
  const bool any = !sel.isEmpty();
  emptyLabel->setVisible(!any);
  for (QGroupBox *b : {geometryBox, fillBox, outlineBox, textBox, pictureBox}) b->setVisible(any);
  if (!any) return;

  updating = true;
  const bool single = sel.size() == 1;
  const tpl::Element &e = sel.first();
  bool allText = true, allFill = true, allLocked = true, sameType = true;
  for (const tpl::Element &s : sel)
    {
      allText &= s.type == tpl::ItemType::Text;
      allFill &= s.type == tpl::ItemType::Text || s.type == tpl::ItemType::Rect || s.type == tpl::ItemType::Ellipse;
      allLocked &= s.locked;
      sameType &= s.type == e.type;
    }

  QRectF box;
  if (single) box = frameOf(e);
  else
    for (const tpl::Element &s : sel) box = box.isNull() ? canvas->sceneBounds(s) : box.united(canvas->sceneBounds(s));
  setValueIfChanged(xSpin, box.x());
  setValueIfChanged(ySpin, box.y());
  setValueIfChanged(wSpin, box.width());
  setValueIfChanged(hSpin, box.height());
  setValueIfChanged(rotSpin, e.rotation);
  setValueIfChanged(shearHSpin, e.skewH);
  setValueIfChanged(shearVSpin, e.skewV);
  setValueIfChanged(opacitySpin, e.opacity * 100.0);
  lockedCheck->setChecked(allLocked);
  const bool geo = !allLocked;
  xSpin->setEnabled(geo);
  ySpin->setEnabled(geo);
  wSpin->setEnabled(geo && single && e.type != tpl::ItemType::Text);
  hSpin->setEnabled(geo && single && e.type != tpl::ItemType::Text && e.type != tpl::ItemType::Line);
  rotSpin->setEnabled(geo && single);
  shearHSpin->setEnabled(geo && single);
  shearVSpin->setEnabled(geo && single);

  fillBox->setVisible(allFill);
  setSwatch(fillButton, e.gradient.isActive() && !e.gradient.stops.isEmpty() ? e.gradient.stops.first().color : e.fill);
  fillButton->setEnabled(!e.gradient.isActive());
  gradientEdit->setGradient(e.gradient, e.fill);

  QString outlineTitle = tr("Outline");
  if (sameType && e.type == tpl::ItemType::Line) outlineTitle = tr("Line");
  else if (sameType && (e.type == tpl::ItemType::Image || e.type == tpl::ItemType::Replay)) outlineTitle = tr("Border");
  outlineBox->setTitle(outlineTitle);
  setSwatch(outlineColorButton, e.outline.color);
  setValueIfChanged(outlineWidthSpin, e.outline.width);
  outlineStyleCombo->setCurrentIndex(qMax(0, outlineStyleCombo->findData(int(e.outline.style))));
  outlineJoinCombo->setCurrentIndex(qMax(0, outlineJoinCombo->findData(int(e.outline.join))));
  const bool hasJoin = !(sameType && e.type == tpl::ItemType::Line);
  outlineJoinCombo->setVisible(hasJoin);
  outlineJoinLabel->setVisible(hasJoin);

  textBox->setVisible(allText);
  if (allText)
    {
      textEdit->setEnabled(single && !e.locked);
      if (textEdit->toPlainText() != e.text) textEdit->setPlainText(e.text);
      const QString unknown = tpl::unknownMacros(e.text);
      macroWarning->setVisible(!unknown.isEmpty());
      if (!unknown.isEmpty())
        {
          QStringList tags;
          for (const QChar &c : unknown) tags << QStringLiteral("%") + c;
          macroWarning->setText(tr("Unknown macro %1 will print nothing.").arg(tags.join(QStringLiteral(", "))));
        }
      const bool installed = e.font.family.isEmpty() || QFontDatabase::hasFamily(e.font.family);
      fontCombo->setEditText(e.font.family);
      fontWarning->setVisible(!installed);
      if (!installed) fontWarning->setText(tr("The font \"%1\" is not installed on this computer; another font is used to draw it. The template keeps the name.").arg(e.font.family));
      setValueIfChanged(sizeSpin, e.font.pointSize);
      int best = 0, bestDiff = 100000;
      for (int i = 0; i < weightCombo->count(); i++)
        {
          const int diff = qAbs(weightCombo->itemData(i).toInt() - e.font.weight);
          if (diff < bestDiff)
            {
              bestDiff = diff;
              best = i;
            }
        }
      weightCombo->setCurrentIndex(best);
      italicCheck->setChecked(e.font.italic);
      underlineCheck->setChecked(e.font.underline);
      strikeCheck->setChecked(e.font.strikeOut);
      setValueIfChanged(spacingSpin, e.font.letterSpacing);
    }

  const bool isPicture = single && (e.type == tpl::ItemType::Image || e.type == tpl::ItemType::Replay);
  pictureBox->setVisible(isPicture);
  if (isPicture)
    {
      if (e.type == tpl::ItemType::Image)
        pictureInfo->setText(tr("Picture, %1 × %2 pixels.").arg(e.image.width()).arg(e.image.height()));
      else
        pictureInfo->setText(tr("The replay window: the picture shown in the Transmit tab's preview is placed here when the template is applied."));
      replaceImageButton->setVisible(e.type == tpl::ItemType::Image);
    }
  updating = false;
}
