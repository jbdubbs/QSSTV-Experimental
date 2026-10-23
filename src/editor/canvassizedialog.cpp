#include "canvassizedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

CanvasSizeDialog::CanvasSizeDialog(const QVector<SizePreset> &presetList, const QSize &current, bool scaleOption, const QString &title, QWidget *parent)
  : QDialog(parent), presets(presetList)
{
  setWindowTitle(title);
  combo = new QComboBox;
  combo->addItem(tr("Custom size"));
  for (const SizePreset &p : presets) combo->addItem(p.label);
  combo->setToolTip(tr("The picture size of an SSTV mode. A template is stretched to the transmitted picture, so use the size of the mode you send most."));
  widthSpin = new QSpinBox;
  heightSpin = new QSpinBox;
  // a template read from a file may be outside the usual range: OK without changes must not resize it
  widthSpin->setRange(qMin(minSide, current.width()), qMax(maxSide, current.width()));
  heightSpin->setRange(qMin(minSide, current.height()), qMax(maxSide, current.height()));
  widthSpin->setSuffix(tr(" px"));
  heightSpin->setSuffix(tr(" px"));
  scaleCheck = new QCheckBox(tr("Scale the existing items to the new size"));
  scaleCheck->setVisible(scaleOption);
  scaleCheck->setToolTip(tr("Otherwise the items keep their size and position and only the canvas changes"));

  QFormLayout *form = new QFormLayout;
  form->addRow(tr("Size of"), combo);
  form->addRow(tr("Width"), widthSpin);
  form->addRow(tr("Height"), heightSpin);

  // the layout choice, hidden until setLayoutOption()
  QGroupBox *lb = new QGroupBox(tr("When the picture has another shape"));
  keepRadio = new QRadioButton(tr("Keep the proportions (recommended)"));
  stretchRadio = new QRadioButton(tr("Stretch the template to the picture"));
  keepRadio->setToolTip(tr("Text and shapes keep their shape on every mode. Each item sticks to the edge or corner you choose (Anchor in the Properties panel); a bar can span the full width."));
  stretchRadio->setToolTip(tr("The whole template is stretched over the picture, as in older templates: text and circles look squashed when the picture's shape differs."));
  keepRadio->setChecked(true);
  QVBoxLayout *ll = new QVBoxLayout(lb);
  ll->addWidget(keepRadio);
  ll->addWidget(stretchRadio);
  lb->setVisible(false);
  layoutBox = lb;

  // the "Start from" list, hidden until setStarters()
  starterCombo = new QComboBox;
  starterCombo->setToolTip(tr("A ready-made arrangement to start from; you can change everything afterwards"));
  QWidget *sr = new QWidget;
  QFormLayout *sf = new QFormLayout(sr);
  sf->setContentsMargins(0, 0, 0, 0);
  sf->addRow(tr("Start from"), starterCombo);
  sr->setVisible(false);
  starterRow = sr;

  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->addLayout(form);
  lay->addWidget(scaleCheck);
  lay->addWidget(layoutBox);
  lay->addWidget(starterRow);
  lay->addWidget(box);

  updating = true;
  widthSpin->setValue(current.width());
  heightSpin->setValue(current.height());
  updating = false;
  sizeEdited();

  connect(combo, QOverload<int>::of(&QComboBox::activated), this, &CanvasSizeDialog::presetChosen);
  connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CanvasSizeDialog::sizeEdited);
  connect(heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &CanvasSizeDialog::sizeEdited);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QSize CanvasSizeDialog::canvasSize() const
{
  return QSize(widthSpin->value(), heightSpin->value());
}

void CanvasSizeDialog::setLayoutOption(bool keep)
{
  (keep ? keepRadio : stretchRadio)->setChecked(true);   // (the radio buttons are exclusive: the other one is unchecked by this)
  layoutBox->setVisible(true);
}

bool CanvasSizeDialog::keepProportions() const
{
  return keepRadio->isChecked();
}

void CanvasSizeDialog::setStarters(const QStringList &names)
{
  starterCombo->clear();
  starterCombo->addItems(names);
  starterRow->setVisible(!names.isEmpty());
}

int CanvasSizeDialog::starter() const
{
  return starterCombo->count() > 0 ? qMax(0, starterCombo->currentIndex()) : 0;
}

bool CanvasSizeDialog::scaleItems() const
{
  return scaleCheck->isVisibleTo(this) && scaleCheck->isChecked();   // (isVisible() is false once the dialog has been closed)
}

void CanvasSizeDialog::presetChosen(int index)
{
  if (index <= 0 || index > presets.size()) return;
  updating = true;
  widthSpin->setValue(presets[index - 1].size.width());
  heightSpin->setValue(presets[index - 1].size.height());
  updating = false;
}

void CanvasSizeDialog::sizeEdited()
{
  if (updating) return;
  // show which preset the numbers match, if any
  int match = 0;
  for (int i = 0; i < presets.size(); i++)
    if (presets[i].size == canvasSize())
      {
        match = i + 1;
        break;
      }
  combo->setCurrentIndex(match);
}
