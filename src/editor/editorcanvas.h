/***************************************************************************
 *   The canvas of the template editor: shows the template, handles the    *
 *   mouse and keyboard, and turns what the user does into undo commands   *
 *   on the EditorDocument.                                                *
 *                                                                         *
 *   Every item is drawn by tpl::paintElement(), the same code that puts   *
 *   the template on the transmitted picture, so the canvas shows what is  *
 *   sent. The scene holds one ElementItem per element (a view, nothing    *
 *   more); selection is kept here, as a set of ids.                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#ifndef EDITORCANVAS_H
#define EDITORCANVAS_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSet>
#include "editordocument.h"

/*! One element on the canvas. The item sits at the element's position and paints the element with its own
    transform, so moving an element only moves the item. */
class ElementItem : public QGraphicsItem
{
public:
  ElementItem(const tpl::Element &e, const tpl::RenderOptions *opt);
  void setElement(const tpl::Element &e);
  const tpl::Element &element() const { return elem; }
  void optionsChanged();                       //!< macro preview switched: bounds may differ
  QRectF boundingRect() const override { return bounds; }
  void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
private:
  void refresh();
  tpl::Element elem;
  const tpl::RenderOptions *opt;
  QRectF bounds;
  // A picture is scaled to its rectangle on every paint by the renderer; keeping it scaled here makes that a no-op
  // (the renderer scales to the same integer size first, so the result is identical) and repaints cheap.
  QImage scaled;
  qint64 scaledKey = 0;
  QSize scaledSize;
};

class EditorCanvas : public QGraphicsView
{
  Q_OBJECT
public:
  enum Tool { ToolSelect, ToolText, ToolRect, ToolEllipse, ToolLine, ToolReplay };
  enum AlignMode { AlignLeft, AlignHCenter, AlignRight, AlignTop, AlignVCenter, AlignBottom };

  explicit EditorCanvas(EditorDocument *doc, QWidget *parent = nullptr);
  ~EditorCanvas() override;

  // --- selection ------------------------------------------------------------------------------------------
  QStringList selection() const;               //!< paint order
  QVector<tpl::Element> selectedElements() const;
  bool hasSelection() const { return !selected.isEmpty(); }
  void setSelection(const QStringList &ids);
  void selectAll();
  void clearSelection() { setSelection(QStringList()); }

  // --- tools and view -------------------------------------------------------------------------------------
  Tool tool() const { return currentTool; }
  void setTool(Tool t);
  double zoom() const { return zoomFactor; }
  void setZoom(double z, const QPointF &anchorView = QPointF(-1, -1));
  void zoomIn() { setZoom(zoomFactor * 1.25); }
  void zoomOut() { setZoom(zoomFactor / 1.25); }
  void zoomFit();
  void zoomActual() { setZoom(1.0); }

  void setShowGrid(bool on);
  void setSnapToGrid(bool on) { snapGrid = on; }
  void setSnapToObjects(bool on) { snapObjects = on; }
  void setGridSize(int px);
  bool showGrid() const { return gridVisible; }
  int gridSize() const { return gridPx; }
  void setExpandMacros(bool on);
  void setMacroVars(const QHash<QChar, QString> &vars);
  /*! lines to place things by, drawn over the canvas and never exported: the center lines, and a frame inset by 5 % / 10 % */
  void setGuides(bool center, bool margin5, bool margin10);
  void setPreviewImage(const QImage &img);     //!< shown under the template, stretched over the canvas
  void setShowPreview(bool on);
  bool hasPreview() const { return !preview.isNull(); }

  // --- editing (all undoable) ---------------------------------------------------------------------------------
  void deleteSelection();
  void duplicateSelection();
  void copySelection();
  void cutSelection();
  void paste();                                //!< our own clipboard format, else an image, else text
  bool canPaste() const;
  void nudge(double dx, double dy);
  void align(AlignMode mode);
  void distribute(Qt::Orientation o);
  void bringToFront();
  void bringForward();
  void sendBackward();
  void sendToBack();
  void toggleLock();
  bool insertImageFile(const QString &fileName, const QPointF &center = QPointF(qQNaN(), qQNaN()));
  bool insertImage(const QImage &img, const QPointF &center = QPointF(qQNaN(), qQNaN()));
  QString addTextAt(const QPointF &pos, const QString &text);   //!< returns the new id
  void addMacro(const QString &macro);         //!< a new text item showing the macro
  void editText(const QString &id, bool isNew = false);   //!< inline text editing; a new item left empty is taken back
  void commitTextEdit();                       //!< finishes a text being edited in place, if any
  QRectF sceneBounds(const tpl::Element &e) const;   //!< axis-aligned bounds of the item on the canvas
  /*! the items exactly as the canvas draws them, on a transparent image of the canvas at the given scale (no guides, grid or handles) */
  QImage renderItems(double scale = 1.0);
  QString describe(const tpl::Element &e) const;     //!< "Text: %c %m", "Rectangle", ... (layers panel, status)
  /*! reads a picture for an image item (EXIF orientation applied, shrunk to a size that keeps template files small); null with *error set on failure */
  static QImage loadPicture(const QString &fileName, QString *error);
  static tpl::Element defaultElement(tpl::ItemType type);

signals:
  void selectionChanged();
  void toolChanged(int tool);
  void zoomChanged(double zoom);
  void cursorMoved(const QPointF &canvasPos, bool inside);
  void contextMenuRequested(const QPoint &globalPos);
  void statusMessage(const QString &text);

protected:
  void drawBackground(QPainter *p, const QRectF &rect) override;
  void drawForeground(QPainter *p, const QRectF &rect) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void showEvent(QShowEvent *e) override;

private slots:
  void docReset();
  void docItemInserted(const QString &id);
  void docItemRemoved(const QString &id);
  void docItemChanged(const QString &id);
  void docOrderChanged();
  void docCanvasChanged();

private:
  enum Op { OpNone, OpMove, OpResize, OpRotate, OpBand, OpCreate, OpPan };
  enum Handle { HNone = -1, HTopLeft, HTop, HTopRight, HRight, HBottomRight, HBottom, HBottomLeft, HLeft, HRotate };

  EditorDocument *doc;
  QGraphicsScene scene;
  QHash<QString, ElementItem *> items;
  QSet<QString> selected;
  tpl::RenderOptions renderOpt;
  int varsGeneration = 0;                       //!< changes with the macro values: part of the key of expanded text bounds
  mutable QHash<QString, QRectF> boundsCache;   //!< text layout bounds by text/font/macro state
  Tool currentTool = ToolSelect;
  double zoomFactor = 1.0;
  bool autoFit = true;

  bool guideCenter = false, guideMargin5 = false, guideMargin10 = false;
  bool gridVisible = false, snapGrid = false, snapObjects = true;
  int gridPx = 16;
  QImage preview;
  bool previewVisible = false;

  // mouse operation in progress
  Op op = OpNone;
  QPointF opStartScene;
  QPoint opStartView;
  QVector<tpl::Element> opBefore;
  Handle opHandle = HNone;
  QPointF opAnchorLocal, opAnchorScene;
  double opStartAngle = 0;
  bool opMoved = false;
  bool opShiftToggled = false;                 //!< press toggled the item out/in of the selection: a click, not a drag
  bool opGroup = false;                        //!< the resize or rotate in progress works on the whole selection
  QRectF opGroupRect;                          //!< the selection's frame when the drag began (canvas coordinates)
  QString opPressedId;
  QRectF createRect;                           //!< rubber band / shape being drawn, canvas coordinates
  QVector<double> snapXs, snapYs;              //!< candidate snap lines while moving
  QVector<double> guideXs, guideYs;            //!< snap lines to draw
  bool spaceDown = false;
  QPoint panLast;
  QPointer<QPlainTextEdit> textEdit;
  int pasteCount = 0;

  // helpers
  ElementItem *itemFor(const QString &id) const { return items.value(id); }
  void syncZ();
  void updateSceneRect();
  QString hitTest(const QPointF &scenePt, bool includeLocked = false) const;   //!< topmost item under the point; locked items are click-through unless asked for
  QPointF toView(const QPointF &scenePt) const { return viewportTransform().map(scenePt); }
  Handle handleAt(const QPoint &viewPt) const;
  QRectF groupFrame() const;                                         //!< frame of the unlocked selected items when two or more are selected, else null
  QPointF groupHandleViewPoint(Handle h) const;
  void updateGroupResize(const QPointF &scenePt, Qt::KeyboardModifiers mods);
  void updateGroupRotate(const QPointF &scenePt, Qt::KeyboardModifiers mods);
  double snapEdge(const QVector<double> &candidates, double value, QVector<double> &guides) const;   //!< how far `value` must move to meet a candidate (0 = too far)
  QPointF handlePoint(const tpl::Element &e, Handle h) const;      //!< canvas coordinates
  QPointF handleViewPoint(const tpl::Element &e, Handle h) const;
  bool handleAllowed(const tpl::Element &e, Handle h) const;
  QRectF selectionBounds() const;
  void beginMove(const QPointF &scenePt);
  void updateMove(const QPointF &scenePt, Qt::KeyboardModifiers mods);
  void updateResize(const QPointF &scenePt, Qt::KeyboardModifiers mods);
  void updateRotate(const QPointF &scenePt, Qt::KeyboardModifiers mods);
  void finishOp(const QString &undoText);
  void cancelOp();
  void collectSnapLines();
  double snapDelta(const QVector<double> &candidates, const double (&edges)[3], QVector<double> &guides) const;
  QPointF snapPoint(const QPointF &p, Qt::KeyboardModifiers mods) const;
  void createShape(const QRectF &r);
  void updateCursor(const QPoint &viewPt);
  QString mimeType() const;
  QRectF localBoundsOf(const tpl::Element &e) const;   // tpl::localBounds with the text layout remembered
  QPolygonF outlineOf(const tpl::Element &e) const;     // tpl::sceneOutline likewise
  void anchorNew(tpl::Element &e) const;               // a new item in an anchored template gets the anchors its place suggests
  void insertElements(QVector<tpl::Element> els, const QPointF &offset, const QString &text);
  void setSelectionInternal(const QSet<QString> &ids);
};

#endif  // EDITORCANVAS_H
