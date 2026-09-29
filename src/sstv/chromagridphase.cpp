/***************************************************************************
 *   mmsstv-linux-port: JB60 chroma sampling-grid phase (experiment only)  *
 *   See chromagridphase.h for what this is and why.                      *
 ***************************************************************************/
#include "chromagridphase.h"

namespace
{
  float gChromaGridPhase=0.0f;
}

float chromaGridPhase()
{
  return gChromaGridPhase;
}

void setChromaGridPhase(float phase)
{
  gChromaGridPhase=phase;
}
