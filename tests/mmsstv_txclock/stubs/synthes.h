#pragma once
#include <QtGlobal>
// Counts the audio frames the transmit bridge hands to the real synthesizer::writeBuffer().
struct synthesizer {
  long long frames=0;
  void writeBuffer(quint32 *,int len) {frames+=len;}
};
extern synthesizer *synthesPtr;
