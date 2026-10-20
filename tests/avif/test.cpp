// JPEG 2000 vs AVIF at equal byte budgets (issue #108): size, quality (PSNR) and encode time per pass.
// AVIF goes through the Qt kimg_avif plugin; quality 100 is lossless there, so the search stays in 1..99.
//   tests/avif/run.sh [image ...]
#include "jp2io.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImageReader>
#include <QImageWriter>
#include <cmath>
#include <cstdio>

static double psnr(const QImage &a, const QImage &b)
{
  if (a.size() != b.size()) return -1;
  QImage x = a.convertToFormat(QImage::Format_RGB32), y = b.convertToFormat(QImage::Format_RGB32);
  double se = 0;
  for (int j = 0; j < x.height(); j++)
    for (int i = 0; i < x.width(); i++) {
      QRgb p = x.pixel(i, j), q = y.pixel(i, j);
      double d[3] = {double(qRed(p) - qRed(q)), double(qGreen(p) - qGreen(q)), double(qBlue(p) - qBlue(q))};
      for (double v : d) se += v * v;
    }
  se /= 3.0 * x.width() * x.height();
  return se == 0 ? 99 : 10 * std::log10(255.0 * 255.0 / se);
}

static QByteArray avifAt(const QImage &im, int q)
{
  QByteArray ba; QBuffer buf(&ba); buf.open(QIODevice::WriteOnly);
  QImageWriter w(&buf, "avif"); w.setQuality(q);
  if (!w.write(im)) return QByteArray();
  return ba;
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if (!QImageWriter::supportedImageFormats().contains("avif")) { printf("no avif writer plugin\n"); return 1; }
  const int targets[] = {5000, 10000, 20000, 40000, 80000};
  for (int a = 1; a < argc; a++) {
    QImage src = QImage(argv[a]).convertToFormat(QImage::Format_RGB32);
    if (src.isNull()) { printf("cannot load %s\n", argv[a]); return 1; }
    if (src.width() > 640) src = src.scaledToWidth(640, Qt::SmoothTransformation);   // DRM-sized
    printf("== %s  %dx%d\n", argv[a], src.width(), src.height());
    printf("%8s | %8s %6s %7s | %8s %4s %6s %7s %6s\n", "target", "jp2 B", "PSNR", "ms", "avif B", "q", "PSNR", "ms/pass", "saving");
    for (int target : targets) {
      int raw = src.sizeInBytes() * 3 / 4;
      if (target >= raw) continue;
      QElapsedTimer t;
      jp2IO jp; QImage jdec; int jsize = 0;
      t.start();
      QByteArray jd = jp.encode(src, jdec, jsize, raw / target);
      qint64 jms = t.elapsed();
      // largest quality in 1..99 that fits the budget (linear scan from a bisection bracket)
      int lo = 1, hi = 99, best = 0, passes = 0; QByteArray bestData;
      t.start();
      while (lo <= hi) {
        int mid = (lo + hi) / 2;
        QByteArray d = avifAt(src, mid); passes++;
        if (!d.isEmpty() && d.size() <= target) { best = mid; bestData = d; lo = mid + 1; } else hi = mid - 1;
      }
      qint64 ams = t.elapsed() / (passes ? passes : 1);
      if (bestData.isEmpty()) { printf("%8d | %8d %6.2f %7lld | does not fit even at q=1\n", target, (int)jd.size(), psnr(src, jdec), (long long)jms); continue; }
      QImage adec; adec.loadFromData(bestData, "avif");
      if (const char *d = getenv("AVIF_DUMP")) {   // decoded results for visual comparison
        jdec.save(QString("%1/%2_%3_jp2.png").arg(d).arg(a).arg(target));
        adec.save(QString("%1/%2_%3_avif.png").arg(d).arg(a).arg(target));
        src.save(QString("%1/%2_src.png").arg(d).arg(a));
      }
      printf("%8d | %8d %6.2f %7lld | %8d %4d %6.2f %7lld %5.0f%%\n", target, (int)jd.size(), psnr(src, jdec), (long long)jms,
             (int)bestData.size(), best, psnr(src, adec), (long long)ams, 100.0 * (1.0 - double(bestData.size()) / jd.size()));
    }
  }
  return 0;
}
