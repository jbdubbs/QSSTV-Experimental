/***************************************************************************
 *   mmsstv-linux-port: TX edge-adaptive chroma pre-emphasis boost for JB60 *
 *   See chromaedgeboost.h for what this is and why.                       *
 ***************************************************************************/
#include "chromaedgeboost.h"

#include <QSettings>

namespace
{
  int chromaEdgeBoostOverride=-1;
}

void setChromaEdgeBoostOverride(int override)
{
  chromaEdgeBoostOverride=override;
}

bool chromaEdgeBoostEnabled()
{
  if(chromaEdgeBoostOverride>=0) return chromaEdgeBoostOverride==1;
  QSettings qSettings;
  qSettings.beginGroup("TX");
  bool enabled=qSettings.value("chromaEdgeBoost",false).toBool();
  qSettings.endGroup();
  return enabled;
}

void setChromaEdgeBoostEnabled(bool enabled)
{
  QSettings qSettings;
  qSettings.beginGroup("TX");
  qSettings.setValue("chromaEdgeBoost",enabled);
  qSettings.endGroup();
}
