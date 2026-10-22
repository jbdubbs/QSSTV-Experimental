// psnr <a.png> <b.png>: prints the PSNR (dB, 999 when identical) of two images, -1 when the sizes differ.
#include <QtGui>
#include <cmath>
int main(int argc, char **argv)
{
  QGuiApplication app(argc, argv);
  if (argc < 3) return 2;
  QImage x = QImage(argv[1]).convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QImage y = QImage(argv[2]).convertToFormat(QImage::Format_ARGB32_Premultiplied);
  if (x.isNull() || y.isNull() || x.size() != y.size()) { printf("-1\n"); return 1; }
  double mse = 0;
  for (int j = 0; j < x.height(); j++)
    {
      const QRgb *p = reinterpret_cast<const QRgb *>(x.constScanLine(j));
      const QRgb *q = reinterpret_cast<const QRgb *>(y.constScanLine(j));
      for (int i = 0; i < x.width(); i++)
        {
          const int d[4] = {qRed(p[i]) - qRed(q[i]), qGreen(p[i]) - qGreen(q[i]), qBlue(p[i]) - qBlue(q[i]), qAlpha(p[i]) - qAlpha(q[i])};
          for (int c = 0; c < 4; c++) mse += double(d[c]) * d[c];
        }
    }
  mse /= double(x.width()) * x.height() * 4;
  printf("%.1f\n", mse == 0 ? 999.0 : 10 * std::log10(255.0 * 255.0 / mse));
  return 0;
}
