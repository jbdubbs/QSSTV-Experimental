#include "macropalette.h"

#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "templatevars.h"

MacroPalette::MacroPalette(QWidget *parent) : QWidget(parent)
{
  QLabel *help = new QLabel(tr("Macros are replaced by their value when the picture is transmitted. Double-click one to put it into the selected text, or to add a new text with it."));
  help->setWordWrap(true);
  list = new QListWidget;
  list->setAlternatingRowColors(true);
  list->setToolTip(tr("Double-click to insert"));
  previewCheck = new QCheckBox(tr("Show sample values on the canvas"));
  previewCheck->setToolTip(tr("Show what the text will look like once the macros are replaced (the template itself keeps the macros)"));
  QPushButton *insert = new QPushButton(tr("Insert"));

  QVBoxLayout *lay = new QVBoxLayout(this);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->addWidget(help);
  lay->addWidget(list, 1);
  lay->addWidget(previewCheck);
  lay->addWidget(insert);

  setValues(tpl::sampleVars());
  auto activate = [this]() {
    QListWidgetItem *it = list->currentItem();
    if (it) emit macroActivated(it->data(Qt::UserRole).toString());
  };
  // double-click only: on desktops where a single click activates items, selecting a macro must not insert it
  connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) { emit macroActivated(it->data(Qt::UserRole).toString()); });
  connect(insert, &QPushButton::clicked, this, activate);
}

void MacroPalette::setValues(const QHash<QChar, QString> &values)
{
  const QString keep = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toString() : QString();
  list->clear();
  for (const tpl::MacroInfo &m : tpl::macroTable())
    {
      const QString macro = QStringLiteral("%") + m.tag;
      QListWidgetItem *it = new QListWidgetItem(QStringLiteral("%1   %2   →   %3").arg(macro, m.name, values.value(m.tag, m.sample)));
      it->setData(Qt::UserRole, macro);
      it->setToolTip(m.description);
      list->addItem(it);
      if (macro == keep) list->setCurrentItem(it);
    }
  if (!list->currentItem()) list->setCurrentRow(0);
}

void MacroPalette::setPreviewAction(QAction *a)
{
  previewCheck->setChecked(a->isChecked());
  connect(previewCheck, &QCheckBox::toggled, a, &QAction::setChecked);
  connect(a, &QAction::toggled, previewCheck, &QCheckBox::setChecked);
}
