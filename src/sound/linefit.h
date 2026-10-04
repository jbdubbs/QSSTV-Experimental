#ifndef LINEFIT_H
#define LINEFIT_H

#include <vector>

struct linePoint
{
  double x;
  double y;
  int group;      //!< points of different groups get their own intercept but share the slope (0 for a plain fit)
};

struct lineFitResult
{
  bool valid=false;
  double slope=0;
  double slopeError=0;   //!< one standard error of the slope
  double rms=0;          //!< rms residual of the inliers
  int inliers=0;
  double span=0;         //!< x range of the inliers
};

/*!
  Least squares line through the points with iterative outlier clipping (points further than clipSigma times the
  robust (median based) scatter of the residuals, but at least minClip, are dropped and the fit repeated). Each group has its own intercept, a
  constant offset per NTP server for instance, and all groups share the slope. If residuals is given it
  receives y minus the fitted line for every input point (the group intercept included), outliers too.
*/
lineFitResult fitLine(const std::vector<linePoint> &pts,double clipSigma=3.0,double minClip=0.0,
                      std::vector<double> *residuals=nullptr,std::vector<bool> *inlier=nullptr);

#endif // LINEFIT_H
