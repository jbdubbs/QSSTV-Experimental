/***************************************************************************
 *   mmsstv-linux-port: RX chroma deconvolution for JB60                   *
 *   See chromadeconvolution.h for what this is and why.                   *
 ***************************************************************************/
#include "chromadeconvolution.h"

#include <QSettings>

namespace
{
  int chromaDeconvolutionOverride=-1;
}

void setChromaDeconvolutionOverride(int override)
{
  chromaDeconvolutionOverride=override;
}

bool chromaDeconvolutionEnabled()
{
  if(chromaDeconvolutionOverride>=0) return chromaDeconvolutionOverride==1;
  QSettings qSettings;
  qSettings.beginGroup("RX");
  bool enabled=qSettings.value("chromaDeconvolution",false).toBool();
  qSettings.endGroup();
  return enabled;
}

void setChromaDeconvolutionEnabled(bool enabled)
{
  QSettings qSettings;
  qSettings.beginGroup("RX");
  qSettings.setValue("chromaDeconvolution",enabled);
  qSettings.endGroup();
}
