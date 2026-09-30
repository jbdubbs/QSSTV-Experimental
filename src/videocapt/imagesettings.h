#ifndef IMAGESETTINGS_H
#define IMAGESETTINGS_H

// Reduced, Qt-only replacement for the old V4L2-driven hardware-controls panel -- see
// Phase 5 of the cross-platform plan. QCamera has no portable equivalent to V4L2's
// generic VIDIOC_QUERYCTRL enumeration (arbitrary driver-specific controls), only a
// small fixed set of properties it standardizes itself; this dialog exposes exactly
// those (exposure compensation, zoom, white balance), each shown only when the active
// camera actually reports support for it.

#include <QDialog>
#include <QCamera>

namespace Ui {
class imageSettingsUi;
}

class imageSettings : public QDialog
{
  Q_OBJECT

public:
  explicit imageSettings(QCamera *camera, QWidget *parent = 0);
  ~imageSettings();

private slots:
  void slotExposureChanged(int sliderValue);
  void slotZoomChanged(int sliderValue);
  void slotWhiteBalanceChanged(int index);

private:
  Ui::imageSettingsUi *ui;
  QCamera *cameraPtr;
  void loadCapabilities();
};

#endif // IMAGESETTINGS_H
