#ifndef QMLUI_H
#define QMLUI_H

#include <QString>

/*!
  The Qt Quick user interface for phones and tablets (src/qml, built with QSSTV_QML_UI). Without it these are empty stand-ins, so the
  shared code can call them unconditionally.

  mainWindow and its pages (rxWidget, txWidget, galleryWidget) are still constructed when this interface runs, but never shown: they
  own the receiver and transmitter state, and the controllers in this directory (rxController, ...) read and drive them.
*/
namespace qmlUi
{

#ifdef QSSTV_QML_UI
  //! load the main QML page: its window is the program's visible window (call once, after mainWindow::init())
  void show();
  //! the analog / digital (SSTV / DRM) mode changed (mainWindow::slotModeChange): the screens follow
  void notifyModeChanged();
  // what the dispatcher tells the receive screen (GUI thread, from dispatcher::customEvent)
  void rxImageStarted(int width,int height);   //!< a picture is about to be received: the canvas has this size
  void rxLine(int line);                       //!< another line of it has been drawn
  void rxImageEnded();                         //!< the picture is complete (or given up)
  void rxImageSaved(const QString &file);      //!< a received picture was saved to the gallery
  void problem(const QString &title,const QString &text);   //!< something went wrong that the person has to be told (showProblem())
  void micDenied(bool denied);                 //!< the microphone permission was refused (or given later in the system settings)
  void rxImageShown();                         //!< a finished picture (DRM) was put on the canvas
  void sendPicture(const QString &file);       //!< a picture came from outside (shared, opened with): it goes on the Transmit screen
  void rxStatus(bool drm,const QString &text); //!< the receiver's status line
  void rxSync(int quality);                    //!< sync quality, 0 to 25
  void rxFftInit(int fftLength,int samplingRate);   //!< the spectrum display was set up
  void rxFft(const double *fft);               //!< another spectrum (half-complex, fftLength values)
#else
  inline void show() {}
  inline void notifyModeChanged() {}
  inline void rxImageStarted(int,int) {}
  inline void rxLine(int) {}
  inline void rxImageEnded() {}
  inline void rxImageSaved(const QString &) {}
  inline void problem(const QString &,const QString &) {}
  inline void micDenied(bool) {}
  inline void rxImageShown() {}
  inline void sendPicture(const QString &) {}
  inline void rxStatus(bool,const QString &) {}
  inline void rxSync(int) {}
  inline void rxFftInit(int,int) {}
  inline void rxFft(const double *) {}
#endif
}

#endif // QMLUI_H
