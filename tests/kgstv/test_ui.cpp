// Real RX/TX widgets, with native peer PCM. Never initializes sound/CAT/XML-RPC.
#include "appglobal.h"
#include "kgstvcontroller.h"
#include "rxwidget.h"
#include "txwidget.h"
#include "gallerywidget.h"
#include "configparams.h"
#include "imageviewer.h"
#include "logging.h"
#include "dispatcher.h"
#include <QApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QTimer>
#include <QTemporaryDir>
#include <QHBoxLayout>
#include <QTextStream>
#include <limits>
#include <stdexcept>
#define CHECK(c) do {if(!(c))throw std::runtime_error(QString("line %1: %2").arg(__LINE__).arg(#c).toStdString());}while(false)
static void clickInvalid(QPushButton *button)
{
  bool seen=false;QString title;QTimer timer;
  QObject::connect(&timer,&QTimer::timeout,[&](){if(auto *dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())){seen=true;title=dialog->windowTitle();dialog->accept();}});
  timer.start(10);button->click();timer.stop();CHECK(seen && title=="KG-STV");
}
int main(int argc,char **argv)
{
  QApplication app(argc,argv);QTemporaryDir temporary;
  qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());app.setApplicationName(APPLICATION);app.setApplicationVersion(MAJORVERSION+MINORVERSION);
  try {
    CHECK(temporary.isValid());logFilePtr=new logFile;kgstvControllerPtr=new Kgstv::Controller;
    static_assert(TRXSSTV==0 && TRXDRM==1 && TRXKGSTV==2,"Persisted mode identities changed");
    transmissionModeIndex=TRXKGSTV;autoSave=false;repeaterEnabled=false;
    QWidget container;QHBoxLayout *layout=new QHBoxLayout(&container);
    galleryWidgetPtr=new galleryWidget;
    rxWidgetPtr=new rxWidget(&container);txWidgetPtr=new txWidget(&container);
    rxWidgetPtr->getImageViewerPtr()->setType(imageViewer::RXIMG);
    layout->addWidget(rxWidgetPtr);layout->addWidget(txWidgetPtr);
    auto *rxTabs=rxWidgetPtr->findChild<QTabWidget *>("settingsTableWidget");
    auto *txTabs=txWidgetPtr->findChild<QTabWidget *>("settingsTableWidget");
    CHECK(rxTabs && txTabs && rxTabs->count()==3 && txTabs->count()==3);
    CHECK(rxTabs->tabText(2).contains("experimental") && txTabs->tabText(2).contains("experimental"));
    rxWidgetPtr->setSettingsTab();txWidgetPtr->changeTransmissionMode(TRXKGSTV);
    CHECK(rxTabs->currentIndex()==2 && txTabs->currentIndex()==2);
    auto *txViewer=txWidgetPtr->getImageViewerPtr();txViewer->setType(imageViewer::TXIMG);
    QImage large(640,480,QImage::Format_RGB32);large.fill(Qt::red);CHECK(txViewer->openImage(large));
    txViewer->setParam(QString(),false,320,240);CHECK(txViewer->getDisplayedImage()->size()==QSize(320,240));
    txViewer->setSize(0,false);CHECK(txViewer->getDisplayedImage()->size()==QSize(320,240));

    auto *rxMod=rxWidgetPtr->findChild<QComboBox *>("kgstvRxModulation");
    auto *txMod=txWidgetPtr->findChild<QComboBox *>("kgstvTxModulation");
    auto *quality=txWidgetPtr->findChild<QSpinBox *>("kgstvQuality");
    auto *input=txWidgetPtr->findChild<QPlainTextEdit *>("kgstvTextInput");
    auto *send=txWidgetPtr->findChild<QPushButton *>("kgstvSendText");
    auto *history=rxWidgetPtr->findChild<QPlainTextEdit *>("kgstvMessageHistory");
    CHECK(rxMod && txMod && quality && input && send && history && history->isReadOnly());
    auto *save=rxWidgetPtr->findChild<QCheckBox *>("kgstvAutoSave");CHECK(save);
    save->setChecked(true);CHECK(autoSave);save->setChecked(false);CHECK(!autoSave);

    rxMod->setCurrentIndex(1);CHECK(QMetaObject::invokeMethod(rxMod,"activated",Q_ARG(int,1)));app.processEvents();
    CHECK(txMod->currentIndex()==1 && kgstvControllerPtr->options().fourLevel);
    quality->setValue(9);app.processEvents();CHECK(kgstvControllerPtr->options().quality==9);
    container.resize(1250,900);container.show();app.processEvents();
    input->setPlainText(QString::fromUtf8("Grüße"));clickInvalid(send);CHECK(!kgstvControllerPtr->busy());
    input->setPlainText(QString(201,'A'));clickInvalid(send);CHECK(!kgstvControllerPtr->busy());
    input->setPlainText(" ");clickInvalid(send);CHECK(!kgstvControllerPtr->busy());
    auto *request=txWidgetPtr->findChild<QPushButton *>("kgstvRequestBlocks");
    auto *repair=txWidgetPtr->findChild<QPushButton *>("kgstvSendRepairs");CHECK(request && repair);
    clickInvalid(request);clickInvalid(repair);CHECK(!kgstvControllerPtr->busy());

    Kgstv::Controller peer;auto options=peer.options();options.fourLevel=true;CHECK(peer.configure(options));
    CHECK(peer.prepareTextJob("<b>plain text, not markup</b>","OE6TEST"));CHECK(peer.beginTransmit());
    for(;;) {
      double samples[1024];const int count=peer.render(samples,1024);if(!count)break;
      QVector<double> downsampled;for(int i=3;i<count;i+=4)downsampled.append(samples[i]);
      CHECK(kgstvControllerPtr->receive(downsampled.constData(),downsampled.size()));
    }
    peer.completeTransmit(true);app.processEvents();
    CHECK(history->toPlainText().contains("OE6TEST: <b>plain text, not markup</b>") && history->toPlainText().contains("UTC"));
    QString error;CHECK(kgstvControllerPtr->prepareTextJob("Local canceled message","OE6PLD",&error));
    kgstvControllerPtr->cancelTransmit();app.processEvents();CHECK(history->toPlainText().contains("TX canceled/failed: Local canceled message"));
    txWidgetPtr->prepareTxComplete(true,0);CHECK(!kgstvControllerPtr->busy());
    txWidgetPtr->prepareTxComplete(true,std::numeric_limits<quint64>::max());CHECK(!kgstvControllerPtr->busy());
    dispatcher eventDispatcher;
    const quint64 oldToken=txWidgetPtr->functionsPtr()->kgstvCompletionToken();
    txWidgetPtr->functionsPtr()->startTX(txFunctions::TXIDLE);
    CHECK(txWidgetPtr->functionsPtr()->kgstvCompletionToken()!=oldToken);
    endImageTXEvent staleEnd(oldToken);eventDispatcher.customEvent(&staleEnd);
    progressTXEvent staleProgress(20.0,oldToken);eventDispatcher.customEvent(&staleProgress);
    CHECK(soundIOPtr==nullptr && rigControllerPtr==nullptr);

    QImage image(320,240,QImage::Format_RGB32);image.fill(Qt::green);
    CHECK(peer.prepareImageJob(image,"OE6TEST") && peer.beginTransmit());
    while(kgstvControllerPtr->receivedCount()==0) {
      double samples[1024];const int count=peer.render(samples,1024);CHECK(count>0);
      QVector<double> downsampled;for(int i=3;i<count;i+=4)downsampled.append(samples[i]);
      CHECK(kgstvControllerPtr->receive(downsampled.constData(),downsampled.size()));
    }
    peer.cancelTransmit();app.processEvents();
    CHECK(rxWidgetPtr->getImageViewerPtr()->getImagePtr()->size()==QSize(320,240));
    const QColor receivedPixel=rxWidgetPtr->getImageViewerPtr()->getImagePtr()->pixelColor(0,0);
    CHECK(receivedPixel.green()>240 && receivedPixel.red()<10 && receivedPixel.blue()<10);
    const quint64 staleEpoch=kgstvControllerPtr->receiveEpoch();
    kgstvControllerPtr->clearReceive();emit kgstvControllerPtr->imageChanged(image,1,false,staleEpoch);app.processEvents();
    CHECK(rxWidgetPtr->getImageViewerPtr()->getImagePtr()->pixelColor(0,0)==QColor(Qt::black));

    rxWidgetPtr->setKgstvBusy(true);CHECK(!rxMod->isEnabled());rxWidgetPtr->setKgstvBusy(false);CHECK(rxMod->isEnabled());
    const QByteArray artifacts=qgetenv("QSSTV_TEST_ARTIFACTS");
    if(!artifacts.isEmpty())CHECK(container.grab().save(QString::fromUtf8(artifacts)+"/kgstv-live-ui.png"));
    delete txWidgetPtr;txWidgetPtr=nullptr;delete rxWidgetPtr;rxWidgetPtr=nullptr;delete galleryWidgetPtr;galleryWidgetPtr=nullptr;
    delete kgstvControllerPtr;kgstvControllerPtr=nullptr;delete logFilePtr;logFilePtr=nullptr;
    CHECK(soundIOPtr==nullptr && rigControllerPtr==nullptr && xmlIntfPtr==nullptr);
    QTextStream(stdout)<<"PASS: actual KG-STV RX/TX tabs, synchronized modulation/quality, invalid text dialogs, native peer PCM into timestamped plain-text history, local cancellation status, stale prepare ticket and control locking; no device/radio/XML-RPC initialization.\n";
  }catch(const std::exception &error){QTextStream(stderr)<<"FAIL: "<<error.what()<<'\n';return 1;}
}
