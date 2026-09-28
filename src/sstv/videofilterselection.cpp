/***************************************************************************
 *   mmsstv-linux-port: wide video filter for the fast modes               *
 *   See videofilterselection.h for what this is and why.                  *
 ***************************************************************************/
#include "videofilterselection.h"

#include <QSettings>

bool wideVideoFilterEnabled()
{
  QSettings qSettings;
  qSettings.beginGroup("RX");
  bool enabled=qSettings.value("wideVideoFilter",false).toBool();
  qSettings.endGroup();
  return enabled;
}

void setWideVideoFilterEnabled(bool enabled)
{
  QSettings qSettings;
  qSettings.beginGroup("RX");
  qSettings.setValue("wideVideoFilter",enabled);
  qSettings.endGroup();
}

bool videoFilterWideSuits(esstvMode mode)
{
  switch(mode)
    {
    case PD120:
    case PD120S:
    case PD120W:
    case JB60:
      return true;
    default:
      return false;
    }
}

bool modeUsesWideVideoFilter(esstvMode mode)
{
  return videoFilterWideSuits(mode) && wideVideoFilterEnabled();
}
