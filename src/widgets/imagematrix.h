#ifndef IMAGEMATRIX_H
#define IMAGEMATRIX_H

#include <QWidget>
#include <QList>
#include <QDir>
#include <QFileInfoList>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include "widgets/imageviewer.h"



class imageMatrix : public QWidget
{
  Q_OBJECT
  
public:
  explicit imageMatrix(QWidget *parent = 0);
  ~imageMatrix();
  void init(int numRows, int numColumns, QString dir, imageViewer::thumbType tt);
  void changed();
  QFileInfoList getFileList(){return fileList;}
  QString getLastFile();
  void setSortFlag(QDir::SortFlags sf) {sortFlags=sf;}
  //! touch shell: Select is on (taps choose pictures instead of opening them)
  bool selecting() const {return selectMode;}
  //! touch shell: turns Select off; true when it was on
  bool leaveSelectMode();

private:
  void getList();
  void displayFiles();
  void clearSelection();
  QList<imageViewer *> selectedViewers();   // in page order
  imageViewer *anchorViewer;   // shift-click range start
  void setupLayout();
  QVBoxLayout *verticalLayout;
  QHBoxLayout *horizontalLayout;
  QGridLayout *gridLayout;
  QSpacerItem *horizontalSpacer;
  QSpacerItem *horizontalSpacer_2;
  QSpacerItem *horizontalSpacer_3;
  QString dirPath;
  imageViewer::thumbType matrixType=imageViewer::RXIMG;
  QToolButton *prevPushButton;
  QToolButton *nextPushButton;
  QToolButton *beginPushButton;
  QToolButton *endPushButton;
  QLabel *pageLabel;
  QFileInfoList fileList;
  int rows;
  int columns;
  int numPages;
  int currentPage;
  QWidget *parentPtr;
  // touch shell: a Select button in the page row and, while it is on, a bar with what can be done with the pictures chosen
  bool selectMode=false;
  QPushButton *selectButton=nullptr;
  QWidget *selectionBar=nullptr;
  QLabel *selectionLabel=nullptr;
  QPushButton *selectAllButton=nullptr;
  QPushButton *selectToTxButton=nullptr;
  QPushButton *selectDeleteButton=nullptr;
  void updateSelectionBar();
  QDir::SortFlags sortFlags;

private slots:
  void slotPrev();
  void slotNext();
  void slotBegin();
  void slotEnd();
  void slotLayoutChanged();
  void slotThumbClicked(imageViewer *iv,Qt::KeyboardModifiers mods);
  void slotDeleteSelected();
  void slotToTxSelected(int seg);
  void slotSelectToggled(bool on);
  void slotSelectAll();
  void slotSelectionToTx();

};

#endif // IMAGEMATRIX_H
