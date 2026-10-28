// Bounded parser, Qt6 native conversion, persistence and actual editor workflow.
#include "mmsstvimport.h"
#include "mmsstvtemplate.h"
#include "templateeditor.h"
#include "templatevars.h"
#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QDialog>
#include <QAction>
#include <QAbstractButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include <QtEndian>
#include <stdexcept>
#include <cstdio>
#define CHECK(c) do { if(!(c)) throw std::runtime_error(#c); } while(false)
static void integer(QByteArray &b,qint32 value) {
  char bytes[4];qToLittleEndian<qint32>(value,reinterpret_cast<uchar *>(bytes));b.append(bytes,4);
}
static void text(QByteArray &b,const QByteArray &value){integer(b,value.size());b+=value;}
static QByteArray base(int command,int version) {
  QByteArray b;
  for(int value:{command,version,10,20,100,50,0x000000ff})integer(b,value);
  b.append(char(0));integer(b,1);return b;
}
static QByteArray group(const QList<QByteArray> &records) {
  QByteArray b=base(1,2);
  for(int value:{0,0,0x00f8f8f8,320,256,int(records.size())})integer(b,value);
  for(const QByteArray &r:records)b+=r;
  return b;
}
static QByteArray shapeRecord(int command,int shape,int pen) {
  QByteArray b=base(command,0);b[28]=char(pen);b.chop(4);
  integer(b,0x55aa0000);integer(b,shape);integer(b,1);return b;
}
static QByteArray textRecord(int grade=0,int shadow=0) {
  QByteArray b=base(4,7);
  for(int v:{grade,shadow,0,0,10,20,0,0,0,0,0,-6})integer(b,v);
  text(b,"%m %n %q");for(int value:{0x000000ff,0x0000ff00,0x00ff0000,0x00ffffff,0x00223344,0})integer(b,value);
  text(b,"Sans");for(int v:{0,-20,1,0})integer(b,v);
  if(grade==3) {
    integer(b,4);integer(b,4);
    QImage tile(4,4,QImage::Format_RGB32);tile.fill(Qt::red);
    for(int y=0;y<4;++y)for(int x=0;x<4;++x)if((x+y)%2)tile.setPixelColor(x,y,Qt::blue);
    QBuffer buffer(&b);buffer.open(QIODevice::Append);CHECK(tile.save(&buffer,"BMP"));
  }
  return b;
}
static QByteArray picture(int shape=0,int type=1,bool legacy=false) {
  QByteArray b=base(5,5);
  for(int v:{type,shape,0,0})integer(b,v);
  if(type) {
    int size=shape?32:2;
    integer(b,size);integer(b,size);
    QImage image(size,size,QImage::Format_RGB32);image.fill(Qt::green);
    QBuffer buffer(&b);buffer.open(QIODevice::Append);CHECK(image.save(&buffer,"BMP"));
  }
  if(shape==5) {
    if(!legacy)for(int v:{0x55aa2233,3,320,256})integer(b,v);
    else integer(b,3);
    for(int v:{0,0,legacy?256:320,0,0,legacy?200:256})integer(b,v);
  }
  return b;
}

static QString store(const QString &root,const QByteArray &bytes) {
  QString name=root+"/input.mtm";QFile f(name);CHECK(f.open(QIODevice::WriteOnly));CHECK(f.write(bytes)==bytes.size());return name;
}
static void roundTrip(const tpl::TemplateDocument &doc, const QString &directory)
{
  QString error;
  const QString file = directory + "/native.qtempl";
  CHECK(tpl::saveFile(file, doc, &error));
  const auto loaded = tpl::loadFile(file);
  CHECK(loaded.ok);
  CHECK(loaded.doc.items.size() == doc.items.size());
  CHECK(loaded.doc.sampledTransparency == doc.sampledTransparency);
  CHECK(loaded.doc.transparencyPoint == doc.transparencyPoint);
  CHECK(tpl::renderThumbnail(loaded.doc) == tpl::renderThumbnail(doc));
  QImage base(640, 480, QImage::Format_RGB32); base.fill(Qt::cyan);
  tpl::RenderOptions opt; opt.vars = tpl::sampleVars();
  CHECK(tpl::overlay(loaded.doc, base, opt) == tpl::overlay(doc, base, opt));
}
static void previewResponse(bool accept)
{
  QTimer::singleShot(0, [accept]() {
    auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    CHECK(dialog && dialog->objectName() == "mmsstvImportPreview");
    if (accept) dialog->accept(); else dialog->reject();
  });
}
static void batchWorkflow(TemplateEditor &editor, const QStringList &paths, const QString &destination, int cancelStage)
{
  const auto before = tpl::toJson(editor.document()->doc());
  const bool dirty = editor.document()->isModified();
  int stage = 0; bool reportSeen = false; QString report;
  QTimer poll;
  QObject::connect(&poll, &QTimer::timeout, [&]() {
    QWidget *modal = QApplication::activeModalWidget();
    if (!modal) return;
    if (auto file = qobject_cast<QFileDialog *>(modal)) {
      if (file->fileMode() == QFileDialog::ExistingFiles) {
        stage = 1;
        if (cancelStage == 1) { file->reject(); return; }
        file->setDirectory(QFileInfo(paths.first()).absolutePath());
        QStringList quoted; for (const auto &path : paths) quoted << "\"" + path + "\"";
        auto name = file->findChild<QLineEdit *>("fileNameEdit"); CHECK(name);
        name->setText(quoted.join(' '));
      } else {
        stage = 2;
        if (cancelStage == 2) { file->reject(); return; }
        file->setDirectory(destination);
      }
      QMetaObject::invokeMethod(file, "accept");
    } else if (auto question = qobject_cast<QMessageBox *>(modal)) {
      stage = 3; question->button(cancelStage == 3 ? QMessageBox::No : QMessageBox::Yes)->click();
    } else if (auto summary = qobject_cast<QDialog *>(modal)) {
      CHECK(summary->windowTitle().startsWith("MMSSTV import:"));
      reportSeen = true; report = summary->findChild<QPlainTextEdit *>()->toPlainText();
      summary->reject();
    } else { CHECK(false); }
  });
  poll.start(20); editor.importMmsstvBatch(); poll.stop();
  CHECK(stage == (cancelStage ? cancelStage : 3));
  CHECK(reportSeen == !bool(cancelStage));
  CHECK(tpl::toJson(editor.document()->doc()) == before);
  CHECK(editor.document()->isModified() == dirty);
  if (!cancelStage) { CHECK(report.contains("FAILED")); CHECK(report.contains(".qtempl")); CHECK(report.contains(paths.first())); }
}
int main(int argc, char **argv)
{
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  QTemporaryDir isolated; CHECK(isolated.isValid());
  qputenv("HOME", isolated.path().toUtf8()); qputenv("XDG_CONFIG_HOME", isolated.path().toUtf8());
  app.setOrganizationName("QSSTV-test"); app.setApplicationName("mmsstv-import");
  try {
    const QString root = isolated.path();
    const QByteArray fixture = group({base(2,0), base(3,0), shapeRecord(6,4,255),
      textRecord(), textRecord(1,1), textRecord(2), textRecord(3), picture(), picture(4), picture(5), picture(5,1,true)});
    const QString source = store(root, fixture);
    const auto r = Mmsstv::loadTemplate(source);
    CHECK(r.ok); CHECK(r.doc.items.size() == 11); CHECK(r.doc.sampledTransparency);
    CHECK(r.doc.items[2].type == tpl::ItemType::Ellipse);
    CHECK(r.doc.items[3].text == "%m %o %%q");
    CHECK(r.doc.items[4].gradient.isActive()); CHECK(r.doc.items[4].outline.width == 2);
    CHECK(!r.doc.items[6].image.isNull()); CHECK(!r.doc.items[6].text.isEmpty());
    CHECK(r.doc.items[8].image.pixelColor(0,0).alpha() == 0);
    CHECK(r.doc.items[8].image.pixelColor(16,16).alpha() == 255);
    roundTrip(r.doc, root);
    CHECK(QJsonDocument::fromJson(tpl::toJson(r.doc)).object()["version"].toInt() == 2);
    CHECK(QJsonDocument::fromJson(tpl::toJson(tpl::TemplateDocument())).object()["version"].toInt() == 1);
    Mmsstv::Document unchanged; unchanged.size = QSize(7,9); QString error;
    for (int n = 0; n < fixture.size(); ++n) {
      const QString truncated = store(root, fixture.left(n));
      CHECK(!Mmsstv::readTemplate(truncated, &unchanged, &error)); CHECK(unchanged.size == QSize(7,9));
    }
    for (const auto &bad : {group({base(99,0)}), fixture + 'x', group({shapeRecord(3,9,0)})}) {
      CHECK(!Mmsstv::loadTemplate(store(root, bad)).ok);
    }
    store(root, fixture);
    QDir(root).mkdir("batch"); const QString destination = root + "/batch";
    QFile protectedFile(destination + "/input.qtempl"); CHECK(protectedFile.open(QIODevice::WriteOnly));
    protectedFile.write("existing"); protectedFile.close();
    QFile::link(destination + "/does-not-exist", destination + "/input_2.qtempl");
    const auto batch = Mmsstv::importTemplates({source, source, root + "/missing.mtm"}, destination);
    CHECK(batch.size() == 3); CHECK(batch[0].error.isEmpty()); CHECK(batch[1].error.isEmpty());
    CHECK(batch[0].target.endsWith("input_3.qtempl")); CHECK(batch[1].target.endsWith("input_4.qtempl"));
    CHECK(!batch[2].error.isEmpty()); CHECK(protectedFile.open(QIODevice::ReadOnly)); CHECK(protectedFile.readAll() == "existing");
    CHECK(QDir(destination).entryList({".qsstv-import-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    QFile original(source); CHECK(original.open(QIODevice::ReadOnly)); CHECK(original.readAll() == fixture); original.close();
    CHECK(!Mmsstv::importTemplates({source}, root + "/absent")[0].error.isEmpty());
    // A key sampled inside the top red object must cut through the lower green object to the TX base.
    Mmsstv::Document keyed; keyed.colorKey = true; keyed.transparencyPoint = QPoint(20,20);
    Mmsstv::Item lower; lower.rect = QRectF(0,0,100,100); lower.fill = Qt::green; lower.penStyle = 5;
    Mmsstv::Item upper = lower; upper.rect = QRectF(10,10,40,40); upper.fill = Qt::red;
    keyed.items = {lower, upper}; const auto key = Mmsstv::convertTemplate(keyed); CHECK(key.ok);
    QImage base(320,256,QImage::Format_RGB32); base.fill(Qt::blue);
    const QImage over = tpl::overlay(key.doc, base, {});
    CHECK(over.pixelColor(20,20) == QColor(Qt::blue)); CHECK(over.pixelColor(60,60) == QColor(Qt::green));
    CHECK(tpl::renderThumbnail(key.doc).pixelColor(20,20).alpha() == 0);
    roundTrip(key.doc, root);
    // Shared field expansion: one instant, local vs UTC, literal unknown macros and report separators.
    qputenv("TZ", "Europe/Vienna"); tzset();
    QHash<QChar,QString> vars;
    tpl::addMmsstvVars(vars,"OE6PLD","595#007",QDateTime(QDate(2026,7,1),QTime(23,15),Qt::UTC));
    CHECK(vars['t'] == "23:15" && vars['T'] == "2315"); CHECK(vars['D'] == "2026/07/02");
    CHECK(vars['U'] == "01:15" && vars['C'] == "oe6pld"); CHECK(vars['R'] == "595" && vars['N'] == "#007");
    QStringList warnings;
    CHECK(Mmsstv::convertMacros("%q %s %M %n %T %t %D %L", &warnings) == "%%q %%s %%M %o %t %T %d %D");
    CHECK(tpl::expandMacros("%%q %R%N",vars) == "%q 595#007");
    // Actual editor: preview cancellation and refusal to discard preserve the old document.
    TemplateEditor editor; editor.setTemplatesDir(destination);
    editor.setMessageHandler([](const QString &, const QString &) {});
    CHECK(editor.findChild<QAction *>("importMmsstvAct")); CHECK(editor.findChild<QAction *>("importMmsstvBatchAct"));
    tpl::Element old; old.text = "old"; editor.document()->addItems({old}, "old");
    const auto before = tpl::toJson(editor.document()->doc());
    previewResponse(false); CHECK(!editor.importMmsstvFile(source)); CHECK(tpl::toJson(editor.document()->doc()) == before);
    editor.setUnsavedHandler([]() { return TemplateEditor::Cancel; });
    previewResponse(true); CHECK(!editor.importMmsstvFile(source)); CHECK(tpl::toJson(editor.document()->doc()) == before);
    editor.setUnsavedHandler([]() { return TemplateEditor::Discard; });
    previewResponse(true); CHECK(editor.importMmsstvFile(source)); CHECK(editor.document()->fileName().isEmpty());
    CHECK(editor.document()->isModified()); CHECK(editor.document()->doc().items.size() == 11);
    const auto imported = tpl::renderThumbnail(editor.document()->doc());
    CHECK(imported == tpl::renderThumbnail(r.doc));
    CHECK(!editor.saveTo(root + "/absent/fail.qtempl")); CHECK(editor.document()->isModified());
    CHECK(editor.saveTo(root + "/editor.qtempl")); CHECK(!editor.document()->isModified());
    CHECK(editor.openFile(root + "/editor.qtempl")); CHECK(tpl::renderThumbnail(editor.document()->doc()) == imported);
    CHECK(QDir(root).mkdir("UI batch")); const QString uiDir = root + "/UI batch";
    const QString broken = root + "/broken.mtm";
    QFile bad(broken); CHECK(bad.open(QIODevice::WriteOnly)); bad.write("broken"); bad.close();
    editor.document()->addItems({old}, "dirty before batch");
    for (int cancel = 1; cancel <= 3; ++cancel) {
      batchWorkflow(editor, {source, broken}, uiDir, cancel);
      CHECK(QDir(uiDir).entryList(QDir::Files).isEmpty());
    }
    batchWorkflow(editor, {source, broken}, uiDir, 0);
    CHECK(QDir(uiDir).entryList({"*.qtempl"}, QDir::Files).size() == 1);
    // Resizing an imported canvas keeps its sample point valid, and undo restores it.
    auto resizeDoc = editor.document()->doc(); resizeDoc.transparencyPoint = QPointF(319,255);
    editor.document()->load(resizeDoc, QString());
    const QPointF sample = editor.document()->doc().transparencyPoint;
    editor.applyCanvasSize(QSize(160,128), true);
    CHECK(editor.document()->doc().canvas.contains(editor.document()->doc().transparencyPoint));
    roundTrip(editor.document()->doc(), root);
    editor.document()->undoStack()->undo(); CHECK(editor.document()->doc().transparencyPoint == sample);
    if (argc > 1) {
      int count = 0;
      for (const auto &file : QDir(QString::fromLocal8Bit(argv[1])).entryList({"def*.mtm"}, QDir::Files)) {
        auto reference = Mmsstv::loadTemplate(QDir(QString::fromLocal8Bit(argv[1])).filePath(file));
        CHECK(reference.ok); roundTrip(reference.doc, root); ++count;
      }
      CHECK(count == 5); printf("Five original MMSSTV reference templates passed.\n");
    }
    puts("MMSSTV parser, conversion, batch, transparency, macros and editor tests passed.");
  } catch (const std::exception &e) { fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
  return 0;
}
