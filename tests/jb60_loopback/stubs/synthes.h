#pragma once
#include <vector>
struct synthesizer {
  std::vector<float> out;
  void sendSample(double f){out.push_back((float)f);}
  void sendSamples(unsigned int n,double f){for(unsigned i=0;i<n;i++) out.push_back((float)f);}
};
extern synthesizer *synthesPtr;
