/***************************************************************************
 *   mmsstv-linux-port: JB60 TX chroma pre-emphasis                        *
 *   See chromapreemphasis.h for what this is and why.                     *
 ***************************************************************************/
#include "chromapreemphasis.h"

#include <QSettings>

namespace
{
  int preEmphasisOverride=-1;
}

void setChromaPreEmphasisOverride(int override)
{
  preEmphasisOverride=override;
}

bool chromaPreEmphasisEnabled()
{
  if(preEmphasisOverride>=0) return preEmphasisOverride==1;
  QSettings qSettings;
  qSettings.beginGroup("TX");
  bool enabled=qSettings.value("jb60ChromaPreEmphasis",false).toBool();
  qSettings.endGroup();
  return enabled;
}

void setChromaPreEmphasisEnabled(bool enabled)
{
  QSettings qSettings;
  qSettings.beginGroup("TX");
  qSettings.setValue("jb60ChromaPreEmphasis",enabled);
  qSettings.endGroup();
}
