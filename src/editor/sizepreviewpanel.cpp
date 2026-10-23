#include "sizepreviewpanel.h"

#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>
#include "templaterender.h"

/*! the rendered picture, scaled to the room it has, aspect kept */
class PreviewImage : public QWidget
{
public:
  explicit PreviewImage(QWidget *parent = nullptr) : QWidget(parent)
  {
    setMinimumSize(160, 130);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }
  void setImage(const QImage &img)
  {
    image = img;
    update();
  }
  const QImage &current() const { return image; }
  QSize sizeHint() const override { return QSize(300, 230); }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));
    if (image.isNull()) return;
    QSize s = image.size();
    s.scale(size() - QSize(8, 8), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - s.width()) / 2, (height() - s.height()) / 2), s);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(target, image);
    p.setPen(palette().color(QPalette::Mid));
    p.setBrush(Qt::NoBrush);
    p.drawRect(target.adjusted(-1, -1, 0, 0));
  }

private:
  QImage image;
};

SizePreviewPanel::SizePreviewPanel(EditorDocument *document, QWidget *parent) : QWidget(parent), doc(document)
{
  setObjectName(QStringLiteral("sizePreviewPanel"));
  sizeCombo = new QComboBox;
  sizeCombo->setToolTip(tr("The picture size of an SSTV mode: see how the template lands on it"));
  view = new PreviewImage;
  hint = new QLabel;
  hint->setWordWrap(true);
  hint->setVisible(false);
  keepButton = new QPushButton(tr("Keep proportions"));
  keepButton->setToolTip(tr("Switch this template to the layout that keeps proportions (every item is anchored to the nearest edge)"));
  keepButton->setVisible(false);
  // in a scroll area: a short dock scrolls instead of letting the wrapped hint overlap the picture
  QWidget *content = new QWidget;
  QVBoxLayout *lay = new QVBoxLayout(content);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->addWidget(sizeCombo);
  lay->addWidget(view, 1);
  lay->addWidget(hint);
  lay->addWidget(keepButton);
  QScrollArea *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(content);
  QVBoxLayout *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(scroll);

  timer.setSingleShot(true);
  timer.setInterval(60);
  connect(&timer, &QTimer::timeout, this, &SizePreviewPanel::rebuild);
  connect(doc, &EditorDocument::itemInserted, this, &SizePreviewPanel::schedule);
  connect(doc, &EditorDocument::itemRemoved, this, &SizePreviewPanel::schedule);
  connect(doc, &EditorDocument::itemChanged, this, &SizePreviewPanel::schedule);
  connect(doc, &EditorDocument::orderChanged, this, &SizePreviewPanel::schedule);
  connect(doc, &EditorDocument::canvasChanged, this, &SizePreviewPanel::schedule);
  connect(doc, &EditorDocument::reset, this, &SizePreviewPanel::schedule);
  connect(sizeCombo, &QComboBox::activated, this, &SizePreviewPanel::sizeChosen);
  connect(keepButton, &QPushButton::clicked, this, &SizePreviewPanel::keepProportionsRequested);
  setSizes(QVector<SizePreset>(), QSize());
}

void SizePreviewPanel::setSizes(const QVector<SizePreset> &list, const QSize &preferredSize)
{
  presets = list;
  preferred = preferredSize;
  QSize want = preferredSize;
  bool canvasSize = false;
  const QString saved = QSettings().value(QStringLiteral("TemplateEditor/previewSize")).toString();
  const QStringList parts = saved.split(QLatin1Char('x'));
  if (saved == QLatin1String("canvas")) canvasSize = true;
  else if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0) want = QSize(parts[0].toInt(), parts[1].toInt());
  sizeCombo->clear();
  sizeCombo->addItem(tr("Canvas size"), QSize());
  for (const SizePreset &p : presets) sizeCombo->addItem(p.label, p.size);
  int index = 0;
  if (!canvasSize)
    for (int i = 1; i < sizeCombo->count(); i++)
      if (sizeCombo->itemData(i).toSize() == want) index = i;
  sizeCombo->setCurrentIndex(index);
  schedule();
}

void SizePreviewPanel::setPicture(const QImage &img)
{
  picture = img;
  schedule();
}

void SizePreviewPanel::setSampleValues(const QHash<QChar, QString> &values)
{
  vars = values;
  schedule();
}

QSize SizePreviewPanel::currentSize() const
{
  const QSize chosen = sizeCombo->currentData().toSize();
  if (chosen.isValid()) return chosen;
  return doc->doc().canvas.size().toSize().expandedTo(QSize(1, 1));
}

void SizePreviewPanel::sizeChosen()
{
  const QSize s = sizeCombo->currentData().toSize();
  QSettings().setValue(QStringLiteral("TemplateEditor/previewSize"), s.isValid() ? QStringLiteral("%1x%2").arg(s.width()).arg(s.height()) : QStringLiteral("canvas"));
  schedule();
}

void SizePreviewPanel::showEvent(QShowEvent *e)
{
  QWidget::showEvent(e);
  schedule();
}

void SizePreviewPanel::schedule()
{
  if (isVisible()) timer.start();
}

void SizePreviewPanel::rebuild()
{
  if (isVisible()) renderNow();
}

QImage SizePreviewPanel::background(const QSize &size) const
{
  QImage bg(size, QImage::Format_ARGB32);
  if (picture.isNull())
    {
      QPainter p(&bg);
      QLinearGradient g(0, 0, 0, size.height());
      g.setColorAt(0, QColor(70, 120, 180));
      g.setColorAt(0.65, QColor(150, 190, 220));
      g.setColorAt(0.65, QColor(80, 125, 70));
      g.setColorAt(1, QColor(50, 90, 45));
      p.fillRect(bg.rect(), g);
      return bg;
    }
  // the way a picture is cropped to the mode's shape when it is sent
  QImage scaled = picture.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  return scaled.copy((scaled.width() - size.width()) / 2, (scaled.height() - size.height()) / 2, size.width(), size.height()).convertToFormat(QImage::Format_ARGB32);
}

QImage SizePreviewPanel::renderNow()
{
  QSize size = currentSize();
  if (qint64(size.width()) * size.height() > 8LL * 1024 * 1024) size = QSize(640, 496);
  tpl::RenderOptions opt;
  opt.vars = vars;
  const QImage out = tpl::overlay(doc->doc(), background(size), opt);
  view->setImage(out);
  // what this size does to the template
  const QSizeF c = doc->doc().canvas.size();
  const bool differs = c.height() > 0 && size.height() > 0 && qAbs((c.width() / c.height()) / (double(size.width()) / size.height()) - 1) > 0.01;
  const bool stretched = doc->doc().layout == tpl::TemplateDocument::Stretch;
  if (differs && stretched)
    hint->setText(tr("This template is stretched over the picture, so text and circles look squashed at this size."));
  else if (differs)
    hint->setText(tr("This size has another shape than the canvas: items follow the edges they are anchored to and nothing is stretched, except items set to span."));
  else
    hint->clear();
  hint->setVisible(!hint->text().isEmpty());
  keepButton->setVisible(differs && stretched);
  return out;
}
