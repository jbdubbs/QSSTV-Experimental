#ifndef LINEMATCH_H
#define LINEMATCH_H

#include <cstdint>

/**
 * @brief lineMatch: is the distance between two sync pulses a whole number of lines?
 * @param delta distance between the ends of the two pulses (samples)
 * @param samPerLine samples per line
 * @param tolerance largest accepted fractional part
 * @param lineNumber number of lines (the distance rounded to the nearest line)
 * @param fraction fractional part
 * @return true if a match for that line (i.e fractional part is smaller than tolerance)
 *
 * Two pulses less than half a line apart round to 0 lines with a tiny fraction. That is one pulse seen twice
 * (a dip in it, or a noise burst next to it), never two lines, so it is not a match. Chain entries with a line
 * count of 0 made syncProcessor::calcSyncQuality divide by zero.
 */

inline bool lineMatch(double delta,double samPerLine,double tolerance,uint16_t &lineNumber,double &fraction)
{
  lineNumber=(delta+samPerLine/2.) /samPerLine;
  fraction=(double)lineNumber-delta/samPerLine;
  if(fraction<0) fraction=-fraction;
  return (lineNumber>0) && (fraction<tolerance);
}

#endif // LINEMATCH_H
