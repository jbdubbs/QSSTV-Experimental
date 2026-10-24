#include "editorsetup.h"

#include <QMap>
#include <algorithm>
#include "appglobal.h"
#include "configparams.h"
#include "imageviewer.h"
#include "sstvparam.h"
#include "templateeditor.h"
#include "templatevars.h"
#include "txwidget.h"

QVector<SizePreset> sstvSizePresets()
{
  QMap<QPair<int, int>, QStringList> bySize;   // (width, height) -> mode names
  for (int m = 0; m < NUMSSTVMODES; m++)
    {
      const sSSTVParam &p = SSTVTable[m];
      if (p.numberOfPixels == 0 || p.numberOfDisplayLines == 0) continue;
      const QString name = p.shortName.isEmpty() ? p.name : p.shortName;
      if (!name.isEmpty()) bySize[qMakePair(int(p.numberOfPixels), int(p.numberOfDisplayLines))].append(name);
    }
  QVector<SizePreset> presets;
  for (auto it = bySize.constBegin(); it != bySize.constEnd(); ++it)
    {
      QStringList names = it.value();
      names.removeDuplicates();
      const int shown = qMin(4, int(names.size()));
      QString label = names.mid(0, shown).join(QStringLiteral(", "));
      if (names.size() > shown) label += QStringLiteral(", …");
      presets.append({QStringLiteral("%1 (%2 × %3)").arg(label).arg(it.key().first).arg(it.key().second), QSize(it.key().first, it.key().second)});
    }
  std::sort(presets.begin(), presets.end(), [](const SizePreset &a, const SizePreset &b) {
    return a.size.width() != b.size.width() ? a.size.width() < b.size.width() : a.size.height() < b.size.height();
  });
  return presets;
}

QHash<QChar, QString> editorMacroValues()
{
  QHash<QChar, QString> v = tpl::sampleVars();
  auto set = [&v](QChar tag, const QString &value) {
    if (!value.trimmed().isEmpty()) v.insert(tag, value);
  };
  set('m', myCallsign);
  set('q', myQth);
  set('l', myLocator);
  set('n', myLastname);
  set('f', myFirstname);
  set('v', qsstvVersion);
  if (txWidgetPtr)
    {
      if (imageViewer *iv = txWidgetPtr->getImageViewerPtr())
        {
          set('c', iv->toCall);
          set('r', iv->rsv);
          set('o', iv->toOperator);
          set('x', iv->comment1);
          set('y', iv->comment2);
          set('z', iv->comment3);
          set('a', iv->latitude);
          set('b', iv->longitude);
          set('g', iv->place);
        }
    }
  return v;
}

void configureTemplateEditor(TemplateEditor *ed)
{
  ed->setTemplatesDir(templatesPath);
  ed->setSizePresets(sstvSizePresets());
  QSize def(640, 496);
  if (transmissionModeIndex == TRXSSTV && sstvModeIndexTx >= 0 && sstvModeIndexTx < NUMSSTVMODES)
    {
      const sSSTVParam &p = SSTVTable[sstvModeIndexTx];
      if (p.numberOfPixels > 0 && p.numberOfDisplayLines > 0) def = QSize(int(p.numberOfPixels), int(p.numberOfDisplayLines));
    }
  ed->setDefaultCanvasSize(def);
  ed->setSampleValues(editorMacroValues());
  if (txWidgetPtr)
    {
      if (imageViewer *iv = txWidgetPtr->getImageViewerPtr())
        if (iv->hasValidImage()) ed->setPreviewPicture(*iv->getImagePtr());
    }
}
