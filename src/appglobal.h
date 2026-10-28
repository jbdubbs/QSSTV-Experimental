#ifndef APPGLOBAL_H
#define APPGLOBAL_H
#include "logging.h"
//#include "mainwindow.h"
#include "appdefs.h"

class QSplashScreen;
class scopeView;
class rxWidget;
class txWidget;
class galleryWidget;
class waterfallText;
class rigControl;
class gpsReader;
class placeLookup;
class xmlInterface;
class logBook;
class dispatcher;
class fileWatcher;

enum etransmissionMode {TRXSSTV=0,TRXDRM=1,TRXKGSTV=2,TRXNOMODE=3};
namespace Kgstv {class Controller;}
extern Kgstv::Controller *kgstvControllerPtr;

extern const QString MAJORVERSION;
extern const QString CONFIGVERSION;
extern const QString MINORVERSION;
extern  const QString ORGANIZATION;
extern const QString APPLICATION;
extern const QString qsstvVersion;
extern const QString APPNAME;
#define MAGICNUMBER   (('4'<<24)+('Q'<<16)+('Z'<<8)+'S')

class soundBase;
class mainWindow;
class QStatusBar;
class configDialog;

extern mainWindow *mainWindowPtr;
extern soundBase *soundIOPtr;
extern QSplashScreen *splashPtr;
void splashMessage(const QString &msg);
extern dispatcher *dispatcherPtr;
extern QStatusBar *statusBarPtr;
extern fileWatcher *fileWatcherPtr;

extern rxWidget *rxWidgetPtr;
extern txWidget *txWidgetPtr;
extern galleryWidget *galleryWidgetPtr;
extern waterfallText *waterfallPtr;
extern rigControl *rigControllerPtr;
extern gpsReader *gpsReaderPtr;
extern placeLookup *placeLookupPtr;
extern xmlInterface *xmlIntfPtr;
extern configDialog *configDialogPtr;
extern logBook *logBookPtr;

extern int fftNumBlocks;


extern QPixmap *greenPXMPtr;
extern QPixmap *redPXMPtr;

extern logFile *logFilePtr;
extern bool useHybrid;
extern bool inStartup;



extern etransmissionMode transmissionModeIndex;  // SSTV , DRM


#ifndef QT_NO_DEBUG
extern scopeView *scopeViewerData;
extern scopeView *scopeViewerSyncNarrow;
extern scopeView *scopeViewerSyncWide;
#endif

void globalInit(void);
void globalEnd(void);

#endif // APPGLOBAL_H
