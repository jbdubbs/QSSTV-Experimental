// Direct JPEG 2000 check for utils/jp2io.cpp: encode -> decode round trip (RGB) and truncated-stream
// partial decode. (Grayscale input is not covered: parameters.force_rgb is never set, so
// convert_gray_to_rgb() is unreachable.) Guards the OpenJPEG API usage (e.g. the bpp -> prec cleanup, #112).
#include "jp2io.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>

static int fails = 0;
static void check(bool ok, const char *what) { printf("  %s   %s\n", ok ? "ok " : "FAIL", what); if (!ok) fails++; }

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

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  const int W = 160, H = 120;
  QImage src(W, H, QImage::Format_RGB32);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      src.setPixel(x, y, qRgb(x * 255 / (W - 1), y * 255 / (H - 1), (x / 16 + y / 16) % 2 ? 200 : 40));

  printf("== RGB encode/decode\n");
  jp2IO jp;
  QImage enc; int size = 0;
  QByteArray data = jp.encode(src, enc, size, 4);
  check(size > 0 && data.size() == size, "encode produced a stream");
  check(data.startsWith(QByteArray("\x00\x00\x00\x0cjP  ", 8)), "stream has the JP2 signature box");
  check(enc.size() == src.size(), "encode's own re-decode has the source size");
  double p = psnr(src, enc);
  printf("       PSNR (encode's re-decode) %.1f dB\n", p);
  check(p > 30, "PSNR > 30 dB at ratio 4");

  QString fn = dir.filePath("t.jp2");
  QFile f(fn); f.open(QIODevice::WriteOnly); f.write(data); f.close();
  jp2IO jd;
  check(jd.check(fn), "check() recognises the file");
  QImage dec = jd.decode(fn);
  p = psnr(src, dec);
  printf("       PSNR (file decode) %.1f dB\n", p);
  check(p > 30, "decode() from file matches the source");
  check(psnr(enc, dec) > 90, "file decode identical to encode's re-decode");

  printf("== higher compression is smaller\n");
  QImage e2; int s2 = 0;
  jp2IO j2; j2.encode(src, e2, s2, 30);
  check(s2 > 0 && s2 < size, "ratio 30 stream smaller than ratio 4");
  p = psnr(src, e2);
  printf("       PSNR (ratio 30) %.1f dB\n", p);
  check(p > 20 && p < 98, "ratio 30 is lossy but still close to the source");

  printf("== truncated stream (partial DRM reception)\n");
  QString tr = dir.filePath("trunc.jp2");
  QFile t(tr); t.open(QIODevice::WriteOnly); t.write(data.left(data.size() * 3 / 4)); t.close();
  jp2IO jt;
  QImage part = jt.decodePartial(tr);
  check(!part.isNull() && part.size() == src.size(), "decodePartial returns a picture of the right size");
  jp2IO jt2;
  check(jt2.decode(tr).isNull(), "strict decode() of the truncated file fails");

  printf("\n%s (%d failed)\n", fails ? "FAILED" : "passed", fails);
  return fails ? 1 : 0;
}
