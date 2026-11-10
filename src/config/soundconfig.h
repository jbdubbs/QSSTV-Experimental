#ifndef soundConfig_H
#define soundConfig_H

#include "baseconfig.h"
#include "soundbase.h"

extern int samplingrate;
extern double rxClock;
extern double txClock;
extern bool pulseSelected;
extern bool alsaSelected;
extern bool swapChannel;
extern bool duplicateChannel;
extern bool pttToneOtherChannel;
extern QString inputAudioDevice;
extern QString outputAudioDevice;
extern soundBase::edataSrc soundRoutingInput;
extern soundBase::edataDst soundRoutingOutput;
extern quint32 recordingSize;
//! Android only (the other platforms never read them): which microphone processing the system may apply, and a digital gain
//! for a microphone that is too quiet for the signal it is held next to. micPresetAuto tries unprocessed, then voice
//! recognition, then the plain default; the others ask for exactly that one (falling back to the default if refused).
enum {micPresetAuto=0,micPresetUnprocessed=1,micPresetVoiceRecognition=2,micPresetStandard=3};
extern int micPreset;
extern double rxGainDb;   //!< 0 .. +30 dB


namespace Ui {
  class soundConfig;
  }

#ifdef Q_OS_ANDROID
class QComboBox;
class QLabel;
class QSlider;
#endif

class soundConfig : public baseConfig
{
  Q_OBJECT
  
public:
  explicit soundConfig(QWidget *parent = 0);
  ~soundConfig();
  void readSettings();
  void writeSettings();
  void getParams();
  void setParams();



#ifdef Q_OS_ANDROID
protected:
  void showEvent(QShowEvent *e) override;   //!< the route in use and the media volume, as they are now
#endif

private slots:
  void refreshDeviceLists();
  void updateChannelEnables();

private:
  Ui::soundConfig *ui;
#ifdef Q_OS_ANDROID
  void buildPhoneAudioBox();   //!< the phone's own controls, made in code so the shared form stays as it is
  QComboBox *micPresetCombo;
  QSlider *rxGainSlider;
  QLabel *rxGainLabel;
  QSlider *volumeSlider;
  QLabel *routeLabel;
#endif

};

#endif // soundConfig_H
