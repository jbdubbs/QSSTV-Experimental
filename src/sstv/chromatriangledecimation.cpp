/***************************************************************************
 *   QSSTV-Experimental: JB60 triangle-windowed chroma decimation           *
 *   See chromatriangledecimation.h for what this is and why.             *
 ***************************************************************************/
#include "chromatriangledecimation.h"

#include <QSettings>

namespace
{
  int chromaTriangleOverride=-1;
}

void setChromaTriangleDecimationOverride(int override)
{
  chromaTriangleOverride=override;
}

bool chromaTriangleDecimationEnabled()
{
  if(chromaTriangleOverride>=0) return chromaTriangleOverride==1;
  QSettings qSettings;
  qSettings.beginGroup("TX");
  bool enabled=qSettings.value("chromaTriangleDecimation",false).toBool();
  qSettings.endGroup();
  return enabled;
}

void setChromaTriangleDecimationEnabled(bool enabled)
{
  QSettings qSettings;
  qSettings.beginGroup("TX");
  qSettings.setValue("chromaTriangleDecimation",enabled);
  qSettings.endGroup();
}
