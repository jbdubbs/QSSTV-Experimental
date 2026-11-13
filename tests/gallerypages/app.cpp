// The gallery page label (issue #209): "Page N of M" must keep its size and stay clear of the buttons when the page number goes from
// one digit to two (and the page count from 9 to 10), in the REAL imageMatrix, offscreen.
//   app <scratch-dir>
// Built and run by tests/gallerypages/app.sh against an existing build-cmake tree (it links the application's object files).
#include <QtWidgets>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "widgets/imagematrix.h"

static int passed = 0, failed = 0;
static void check(bool ok, const QString &what)
{
  printf("  %s %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
  ok ? passed++ : failed++;
}

[[noreturn]] static void finish()
{
  printf("\n%d passed, %d failed\n", passed, failed);
  fflush(stdout);
  _exit(failed ? 1 : 0);
}

static void settle() { for (int i = 0; i < 5; i++) { QApplication::processEvents(); QThread::msleep(5); } }

// the label, in window coordinates
static QRect rectOf(QWidget *w, QWidget *top) { return QRect(w->mapTo(top, QPoint(0, 0)), w->size()); }

static void runCase(const QString &dir, int files, int cols, int rows)
{
  QDir().mkpath(dir);
  QImage img(16, 16, QImage::Format_RGB32);
  for (int i = 0; i < files; i++) { img.fill(QColor(i * 9 % 256, 80, 160)); img.save(QString("%1/pic%2.png").arg(dir).arg(i, 3, 10, QChar('0'))); }
  QWidget top;
  imageMatrix *m = new imageMatrix(&top);   // lays its grid and buttons out on the parent widget itself
  m->init(rows, cols, dir, imageViewer::RXIMG);
  top.resize(511, 300);
  top.show();
  settle();
  const int perPage = rows * cols;
  const int pages = (files + perPage - 1) / perPage;
  printf("== %d files, %d x %d per page = %d pages\n", files, rows, cols, pages);

  QLabel *label = nullptr;
  foreach (QLabel *l, top.findChildren<QLabel *>()) if (l->text().contains("Page")) label = l;
  QToolButton *prev = top.findChild<QToolButton *>("prevPushButton");
  QToolButton *next = top.findChild<QToolButton *>("nextPushButton");
  printf("    label=%p prev=%p next=%p, labels=%d\n", (void*)label, (void*)prev, (void*)next, (int)top.findChildren<QLabel *>().size());
  check(label && prev && next, "found the page label and the prev/next buttons");
  if (!label || !prev || !next) return;

  int w0 = -1, bad = 0, wChanged = 0, overlap = 0, clipped = 0, prevMoved = 0, nextMoved = 0, p0 = -1, n0 = -1;
  for (int p = 1; p <= pages; p++)
    {
      settle();
      if (w0 < 0) { w0 = label->width(); p0 = rectOf(prev, &top).x(); n0 = rectOf(next, &top).x(); }
      if (label->width() != w0) wChanged++;
      QRect lr = rectOf(label, &top);
      if (lr.intersects(rectOf(prev, &top)) || lr.intersects(rectOf(next, &top))) overlap++;
      QRect cr = label->contentsRect();
      if (label->fontMetrics().horizontalAdvance(label->text()) > cr.width()) clipped++;
      if (rectOf(prev, &top).x() != p0) prevMoved++;
      if (rectOf(next, &top).x() != n0) nextMoved++;
      if (label->text().trimmed() != QString("Page %1 of %2").arg(p).arg(pages)) { bad++; printf("    text is '%s'\n", qPrintable(label->text())); }
      if (p < pages) next->click();
    }
  check(bad == 0, "the text reads 'Page N of M' on every page");
  check(wChanged == 0, "the label keeps one width on every page");
  check(overlap == 0, "the label never touches the prev/next buttons");
  check(clipped == 0, "the text always fits in the label");
  check(prevMoved == 0 && nextMoved == 0, "the buttons do not move from page to page");
  // and back down again across 10 -> 9
  for (int p = pages; p > 1; p--) prev->click();
  settle();
  check(label->text().trimmed() == QString("Page 1 of %1").arg(pages) && label->width() == w0, "back on page 1 with the same width");
}

int main(int argc, char **argv)
{
  setvbuf(stdout, nullptr, _IOLBF, 0);
  if (argc < 2) return 2;
  const QString scratch = argv[1];
  qputenv("HOME", scratch.toLocal8Bit());
  qputenv("XDG_CONFIG_HOME", (scratch + "/config").toLocal8Bit());
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QCoreApplication::setOrganizationName("qsstv-app-test");
  QCoreApplication::setApplicationName("qsstv-app-test");
  QApplication app(argc, argv);
  runCase(scratch + "/gal12", 12, 1, 1);    // 12 pages: 9 -> 10, "of 12"
  runCase(scratch + "/gal120", 120, 1, 1);  // 120 pages: 9 -> 10 and 99 -> 100
  finish();
}
