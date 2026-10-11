/***************************************************************************
 *   QSSTV-Experimental: JB60 chroma magnitude companding (experiment only) *
 *   See chromacompanding.h for what this is and why.                     *
 ***************************************************************************/
#include "chromacompanding.h"

namespace
{
  float gChromaCompandGamma=1.0f;
}

float chromaCompandGamma()
{
  return gChromaCompandGamma;
}

void setChromaCompandGamma(float gamma)
{
  gChromaCompandGamma=gamma;
}
