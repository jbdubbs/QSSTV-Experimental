#ifndef GALLERYWIDGET_H
#define GALLERYWIDGET_H

#include <QWidget>
#include "imageviewer.h"



namespace Ui {
    class galleryWidget;
}

class imageMatrix;

class galleryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit galleryWidget(QWidget *parent = 0);
    ~galleryWidget();

  void init();
  void writeSettings();
  void readSettings();
  void changedMatrix();
  void changedMatrix(imageViewer::thumbType itype);
  void putRxImage(QString fn);
  void txImageChanged ();
  void txStockImageChanged();
  QString getTemplateFileName(int);
  const QStringList &getFilenames();

public slots:
  void slotDirChanged(QString);



private:
    Ui::galleryWidget *ui;
    QStringList sl;
    // The six pages, in the order of the tabs. A phone builds a page when it is first looked at (each page is a grid of
    // viewers and their thumbnails, six of them at start-up is a lot for a phone): m_ready says which have been built.
    // The templates are built at once, the template list of the Transmit page comes from them.
    imageMatrix *matrixAt(int index) const;
    imageViewer::thumbType typeAt(int index) const;
    QString pathAt(int index) const;
    int indexOf(imageViewer::thumbType itype) const;
    void buildMatrix(int index);
    void matrixListChanged(int index);   //!< new or removed files: rescan, if the page has been built
    bool m_lazy=false;
    bool m_ready[6]={false,false,false,false,false,false};
private slots:
    void slotTabShown(int index);
};

extern galleryWidget *galleryWidgetPtr;

#endif // GALLERYWIDGET_H
