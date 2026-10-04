#ifndef SLANTFIT_H
#define SLANTFIT_H

#include <vector>

/*!
  Measures the slant of a single bright vertical line in a received picture (see calibrationSstv).

  Every row is searched for the line (sub-pixel centroid of the brightest part above the half level between the
  row's own dark and bright level), then a straight line x = intercept + slope * row is fitted through the
  centres, rejecting rows that do not belong to it (noise, rows that were not received, a line that has slanted
  out of the picture). Plain C++, no Qt, so it can be tested stand-alone.
*/

struct slantFitResult
{
  bool valid;
  double slope;        //!< pixels per image row; positive when the line moves to the right going down
  double intercept;    //!< pixel position of the line at row 0
  double rms;          //!< rms deviation of the used rows from the fitted line, in pixels
  int used;            //!< rows that took part in the fit
  int found;           //!< rows in which a line was found
  int total;           //!< rows in the picture
  slantFitResult() : valid(false),slope(0),intercept(0),rms(0),used(0),found(0),total(0) {}
};

/**
  \param gray   8 bit luminance, row after row
  \param width,height,stride  picture size and the distance in bytes between two rows
  \param minContrast  a row needs at least this difference between its brightest and darkest pixel to count
*/
slantFitResult fitSlant(const unsigned char *gray,int width,int height,int stride,int minContrast=60);

/** the same for a list of row centres, one per row, NaN-free: rows without a line are left out by passing them in rows[] */
slantFitResult fitSlantPoints(const std::vector<double> &rows,const std::vector<double> &x,int totalRows);

#endif // SLANTFIT_H
