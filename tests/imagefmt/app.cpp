// AVIF as a picture file format without the Qt AVIF plugin (issue #189: the Android APK has none, only libavif for DRM).
// The Qt plugins are switched off after start-up, so QImageReader/QImageWriter know no AVIF (Android's situation), and the
// helpers the Settings > Pictures list, the saved pictures and the gallery use must still offer, write and read AVIF.
// Links the object files of an existing QML-UI build with libavif (tests/imagefmt/app.sh [build-dir], default build-qml).
#include "avifenc.h"
#include "supportfunctions.h"
#include "thumbnailprovider.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>

static int failures = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  if (!ok) failures++;
}

static double psnr(const QImage &a, const QImage &b)
{
  if (a.size() != b.size()) return -1;
  const QImage x = a.convertToFormat(QImage::Format_RGB32), y = b.convertToFormat(QImage::Format_RGB32);
  double se = 0;
  for (int j = 0; j < x.height(); j++)
    for (int i = 0; i < x.width(); i++) {
      const QRgb p = x.pixel(i, j), q = y.pixel(i, j);
      const double d[3] = {double(qRed(p) - qRed(q)), double(qGreen(p) - qGreen(q)), double(qBlue(p) - qBlue(q))};
      for (double v : d) se += v * v;
    }
  se /= 3.0 * x.width() * x.height();
  return se == 0 ? 99 : 10 * std::log10(255.0 * 255.0 / se);
}

static QImage testPicture(int w, int h)
{
  QImage im(w, h, QImage::Format_RGB32);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      im.setPixel(x, y, qRgb(x * 255 / w, y * 255 / h, ((x / 16 + y / 16) & 1) ? 200 : 60));   // gradients and a checker: edges and smooth areas
  return im;
}

int main(int argc, char **argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QCoreApplication::setLibraryPaths(QStringList());   // no image plugins from here on, like the Android APK without kimg_avif
  if (!avifWriteSupported()) { printf("this build has no libavif: nothing to test\n"); return 1; }

  printf("== no Qt AVIF plugin\n");
  check(!QImageWriter::supportedImageFormats().contains("avif"), "Qt itself writes no AVIF (the precondition)");
  check(!QImageReader::supportedImageFormats().contains("avif"), "Qt itself reads no AVIF (the precondition)");

  printf("== the format lists\n");
  const QStringList writable = writableImageFormats();
  check(writable.contains("avif"), "the writable formats include avif");
  check(writable.contains("png"), "and still png");
  check(writable.count("avif") == 1, "avif is listed once");
  check(imageNameFilter().contains("*.avif"), "the open-file filter includes *.avif");
  check(pictureNameFilter().contains("*.avif"), "and so does the picture filter");

  printf("== saving and reading\n");
  QTemporaryDir dir;
  const QImage src = testPicture(320, 256);
  const QString avif = dir.filePath("picture.avif"), png = dir.filePath("picture.png");
  check(saveImageFile(src, avif, "avif"), "saveImageFile writes avif");
  check(saveImageFile(src, dir.filePath("upper.avif"), "AVIF"), "the format name is not case sensitive");
  QFile f(avif);
  if (!f.open(QIODevice::ReadOnly)) return 2;
  const QByteArray data = f.readAll();
  f.close();
  check(avifSniff(data), QString("the file is an AVIF file (%1 bytes)").arg(data.size()));
  QImage back;
  check(loadImageAutoRotate(back, avif), "loadImageAutoRotate reads it back");
  check(back.size() == src.size(), "the size is kept");
  const double p = psnr(src, back);
  check(p > 35, QString("it looks like the original (PSNR %1 dB)").arg(p, 0, 'f', 1));
  check(saveImageFile(src, png, "png") && QImage(png) == src, "png is unchanged and lossless");
  check(!saveImageFile(QImage(), dir.filePath("none.avif"), "avif"), "an empty picture is not written");
  check(!QFile::exists(dir.filePath("none.avif")), "and leaves no file");
  check(!saveImageFile(src, dir.filePath("nodir/x.avif"), "avif"), "an unwritable path fails cleanly");

  printf("== the gallery\n");
  QSize size;
  ThumbnailProvider thumbs(160), full(2048);
  const QImage t = thumbs.requestImage(ThumbnailProvider::idFor(avif), &size, QSize());
  check(!t.isNull() && qMax(t.width(), t.height()) == 160 && size == t.size(), QString("the thumbnail of the AVIF file is %1x%2").arg(t.width()).arg(t.height()));
  const QImage v = full.requestImage(ThumbnailProvider::idFor(avif), &size, QSize());
  check(v.size() == src.size(), "the viewer gets the whole picture");
  QFile bad(dir.filePath("broken.avif"));
  if (!bad.open(QIODevice::WriteOnly)) return 2;
  bad.write(data.left(20));
  bad.close();
  check(thumbs.requestImage(ThumbnailProvider::idFor(bad.fileName()), &size, QSize()).isNull(), "a damaged AVIF file gives an empty tile, no crash");

  printf("\n%s\n", failures ? "FAILED" : "all passed");
  return failures ? 1 : 0;
}
