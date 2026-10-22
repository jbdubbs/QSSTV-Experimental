#include "gradientedit.h"

#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

/*! The bar showing the gradient as a left-to-right ramp with a marker per stop. */
class GradientStopBar : public QWidget
{
  Q_OBJECT
public:
  explicit GradientStopBar(QWidget *parent = nullptr) : QWidget(parent)
  {
    setMinimumHeight(40);
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Click the bar to add a colour stop, drag a marker to move it, double-click it to change its colour"));
  }
  QVector<tpl::GradientStop> stops;
  int selected = 0;
  bool active = false;
  QSize sizeHint() const override { return QSize(180, 44); }

signals:
  void edited();
  void colorRequested();

protected:
  QRectF barRect() const { return QRectF(8, 4, width() - 16, height() - 18); }
  QPolygonF marker(double x) const
  {
    const QRectF b = barRect();
    return QPolygonF() << QPointF(x, b.bottom() + 1) << QPointF(x - 5, b.bottom() + 10) << QPointF(x + 5, b.bottom() + 10);
  }
  double xOf(double pos) const { return barRect().left() + pos * barRect().width(); }
  double posOf(double x) const { return qBound(0.0, (x - barRect().left()) / barRect().width(), 1.0); }
  int stopAt(const QPointF &p) const
  {
    for (int i = stops.size() - 1; i >= 0; i--)
      if (qAbs(p.x() - xOf(stops[i].pos)) <= 6 && p.y() >= barRect().bottom() - 2) return i;
    return -1;
  }
  QColor colorAt(double pos) const
  {
    if (stops.isEmpty()) return Qt::black;
    if (pos <= stops.first().pos) return stops.first().color;
    for (int i = 1; i < stops.size(); i++)
      if (pos <= stops[i].pos)
        {
          const double span = stops[i].pos - stops[i - 1].pos;
          const double t = span > 0 ? (pos - stops[i - 1].pos) / span : 0;
          const QColor a = stops[i - 1].color, b = stops[i].color;
          return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                                  a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
        }
    return stops.last().color;
  }

  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF b = barRect();
    // checkerboard under the ramp so transparency is visible
    p.save();
    p.setClipRect(b);
    for (int y = 0; y * 6 < b.height(); y++)
      for (int x = 0; x * 6 < b.width(); x++)
        p.fillRect(QRectF(b.left() + x * 6, b.top() + y * 6, 6, 6), ((x + y) & 1) ? QColor(0xcc, 0xcc, 0xcc) : QColor(0xf4, 0xf4, 0xf4));
    p.restore();
    if (active && !stops.isEmpty())
      {
        QLinearGradient g(b.left(), 0, b.right(), 0);
        for (const tpl::GradientStop &s : stops) g.setColorAt(s.pos, s.color);
        p.fillRect(b, g);
      }
    p.setPen(palette().color(QPalette::Mid));
    p.drawRect(b);
    if (!active) return;
    for (int i = 0; i < stops.size(); i++)
      {
        const bool sel = i == selected;
        p.setPen(QPen(palette().color(sel ? QPalette::Highlight : QPalette::WindowText), sel ? 2 : 1));
        p.setBrush(stops[i].color);
        p.drawPolygon(marker(xOf(stops[i].pos)));
      }
  }

  void mousePressEvent(QMouseEvent *e) override
  {
    if (!active || e->button() != Qt::LeftButton) return;
    setFocus();
    const int hit = stopAt(e->position());
    if (hit >= 0)
      {
        selected = hit;
        dragging = true;
      }
    else if (barRect().adjusted(-4, -4, 4, 12).contains(e->position()))
      {
        tpl::GradientStop s;
        s.pos = posOf(e->position().x());
        s.color = colorAt(s.pos);
        stops.append(s);
        std::stable_sort(stops.begin(), stops.end(), [](const tpl::GradientStop &a, const tpl::GradientStop &b) { return a.pos < b.pos; });
        for (int i = 0; i < stops.size(); i++)
          if (stops[i].pos == s.pos) selected = i;
        dragging = true;
        emit edited();
      }
    update();
  }
  void mouseMoveEvent(QMouseEvent *e) override
  {
    if (!dragging || selected < 0 || selected >= stops.size()) return;
    stops[selected].pos = posOf(e->position().x());
    // keep the order while dragging past a neighbour
    const tpl::GradientStop moved = stops[selected];
    std::stable_sort(stops.begin(), stops.end(), [](const tpl::GradientStop &a, const tpl::GradientStop &b) { return a.pos < b.pos; });
    for (int i = 0; i < stops.size(); i++)
      if (stops[i].pos == moved.pos && stops[i].color == moved.color) selected = i;
    emit edited();
    update();
  }
  void mouseReleaseEvent(QMouseEvent *) override { dragging = false; }
  void mouseDoubleClickEvent(QMouseEvent *e) override
  {
    const int hit = stopAt(e->position());
    if (hit >= 0)
      {
        selected = hit;
        emit colorRequested();
      }
  }
  void keyPressEvent(QKeyEvent *e) override
  {
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && stops.size() > 2 && selected >= 0 && selected < stops.size())
      {
        stops.remove(selected);
        selected = qMin(selected, int(stops.size()) - 1);
        emit edited();
        update();
        return;
      }
    QWidget::keyPressEvent(e);
  }

private:
  bool dragging = false;
};

GradientEdit::GradientEdit(QWidget *parent) : QWidget(parent)
{
  kind = new QComboBox(this);
  kind->addItem(tr("Solid colour"), int(tpl::Gradient::None));
  kind->addItem(tr("Linear"), int(tpl::Gradient::Linear));
  kind->addItem(tr("Radial"), int(tpl::Gradient::Radial));
  kind->addItem(tr("Conical"), int(tpl::Gradient::Conical));
  kind->setToolTip(tr("Fill with a single colour or a gradient"));
  angle = new QSpinBox(this);
  angle->setRange(0, 359);
  angle->setWrapping(true);
  angle->setSuffix(QStringLiteral("°"));
  angle->setToolTip(tr("Direction of the gradient (0° = left to right, 90° = top to bottom)"));
  bar = new GradientStopBar(this);
  colorButton = new QPushButton(tr("Stop colour…"), this);
  removeButton = new QPushButton(tr("Remove stop"), this);

  QHBoxLayout *top = new QHBoxLayout;
  top->addWidget(kind, 1);
  top->addWidget(angle);
  QHBoxLayout *buttons = new QHBoxLayout;
  buttons->addWidget(colorButton);
  buttons->addWidget(removeButton);
  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addLayout(top);
  lay->addWidget(bar);
  lay->addLayout(buttons);

  connect(kind, QOverload<int>::of(&QComboBox::activated), this, &GradientEdit::kindChanged);
  connect(angle, QOverload<int>::of(&QSpinBox::valueChanged), this, &GradientEdit::angleChanged);
  connect(bar, &GradientStopBar::edited, this, &GradientEdit::barChanged);
  connect(bar, &GradientStopBar::colorRequested, this, &GradientEdit::stopColor);
  connect(colorButton, &QPushButton::clicked, this, &GradientEdit::stopColor);
  connect(removeButton, &QPushButton::clicked, this, &GradientEdit::removeStop);
  updateControls();
}

void GradientEdit::setGradient(const tpl::Gradient &g, const QColor &base)
{
  grad = g;
  baseColor = base;
  updating = true;
  kind->setCurrentIndex(qMax(0, kind->findData(int(g.kind))));
  angle->setValue(((g.angle % 360) + 360) % 360);
  bar->stops = g.stops;
  bar->selected = qBound(0, bar->selected, qMax(0, int(g.stops.size()) - 1));
  updating = false;
  updateControls();
}

void GradientEdit::updateControls()
{
  const bool on = grad.kind != tpl::Gradient::None;
  bar->active = on;
  bar->setEnabled(on);
  angle->setEnabled(grad.kind == tpl::Gradient::Linear || grad.kind == tpl::Gradient::Conical);
  colorButton->setEnabled(on);
  removeButton->setEnabled(on && bar->stops.size() > 2);
  bar->update();
}

void GradientEdit::kindChanged(int index)
{
  if (updating) return;
  grad.kind = tpl::Gradient::Kind(kind->itemData(index).toInt());
  if (grad.kind != tpl::Gradient::None && grad.stops.size() < 2)
    {
      grad.stops.clear();
      tpl::GradientStop a, b;
      a.pos = 0;
      a.color = baseColor.isValid() ? baseColor : QColor(Qt::white);
      b.pos = 1;
      b.color = a.color.lightness() > 128 ? a.color.darker(250) : a.color.lighter(250);
      grad.stops << a << b;
    }
  bar->stops = grad.stops;
  bar->selected = 0;
  updateControls();
  emit changed(grad);
}

void GradientEdit::angleChanged(int value)
{
  if (updating) return;
  grad.angle = value;
  emit changed(grad);
}

void GradientEdit::barChanged()
{
  grad.stops = bar->stops;
  updateControls();
  emit changed(grad);
}

void GradientEdit::stopColor()
{
  if (bar->selected < 0 || bar->selected >= bar->stops.size()) return;
  const QColor c = QColorDialog::getColor(bar->stops[bar->selected].color, this, tr("Stop colour"), QColorDialog::ShowAlphaChannel);
  if (!c.isValid()) return;
  bar->stops[bar->selected].color = c;
  barChanged();
}

void GradientEdit::removeStop()
{
  if (bar->stops.size() <= 2 || bar->selected < 0 || bar->selected >= bar->stops.size()) return;
  bar->stops.remove(bar->selected);
  bar->selected = qMin(bar->selected, int(bar->stops.size()) - 1);
  barChanged();
}

#include "gradientedit.moc"
