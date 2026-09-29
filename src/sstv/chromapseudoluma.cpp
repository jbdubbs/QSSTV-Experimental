/***************************************************************************
 *   mmsstv-linux-port: JB60 pseudo-luma edge cue (experiment only)        *
 *   See chromapseudoluma.h for what this is and why.                     *
 ***************************************************************************/
#include "chromapseudoluma.h"

namespace
{
  float gChromaPseudoLumaAmplitude=0.0f;
}

float chromaPseudoLumaAmplitude()
{
  return gChromaPseudoLumaAmplitude;
}

void setChromaPseudoLumaAmplitude(float amplitude)
{
  gChromaPseudoLumaAmplitude=amplitude;
}
