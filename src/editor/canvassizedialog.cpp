#include "canvassizedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
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
  QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->addLayout(form);
  lay->addWidget(scaleCheck);
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

bool CanvasSizeDialog::scaleItems() const
{
  return scaleCheck->isVisible() && scaleCheck->isChecked();
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
