#ifndef CALIBRATIONPLOT_H
#define CALIBRATIONPLOT_H

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/*!
  What a calibration measures, drawn: the two traces of the internet time measurement, the tick waterfall of the radio time signal, the received
  picture with the fitted line. The painting is the one of the desktop pages (calibrationntp.cpp, calibrationwwv.cpp, calibrationsstv.cpp), from
  the data of CalibrationController. Dark in both color schemes, as the desktop plots are.
*/
class CalibrationPlot : public QQuickPaintedItem
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int kind READ kind WRITE setKind NOTIFY kindChanged)                   //!< 0 internet time, 1 radio time signal, 2 another SSTV station
  Q_PROPERTY(int revision READ revision WRITE setRevision NOTIFY revisionChanged)   //!< bind it to Calibration.plotRevision: a change repaints
public:
  explicit CalibrationPlot(QQuickItem *parent=nullptr);
  void paint(QPainter *painter) override;
  int kind() const {return m_kind;}
  void setKind(int k);
  int revision() const {return m_revision;}
  void setRevision(int r);
signals:
  void kindChanged();
  void revisionChanged();
private:
  int m_kind=0;
  int m_revision=0;
};

#endif // CALIBRATIONPLOT_H
