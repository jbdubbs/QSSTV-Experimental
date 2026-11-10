#ifndef TOUCHTEMPLATEEDITOR_H
#define TOUCHTEMPLATEEDITOR_H

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include "editordocument.h"
#include "templaterender.h"

class QQmlEngine;
class QJSEngine;

/*!
  The template editor of the Qt Quick interface: a template (src/template) being edited by finger. It owns an EditorDocument, which is the
  same document model and undo stack the desktop editor uses, so every change is one undo step and the files are the same .qtempl.
  QML asks it what is selected and where its handles are (in canvas units: the units of the template, 320 x 256 for Martin 1), and
  tells it what the fingers did. A singleton for QML ("TouchTemplateEditor"); the picture of the template is drawn by TemplateCanvas.
*/
class TouchTemplateEditor : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(TemplateEditor)
  QML_SINGLETON
  Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)                       //!< the editor page is showing
  Q_PROPERTY(QString name READ name NOTIFY fileChanged)                      //!< the template's name (file name without the extension), or empty for a new one
  Q_PROPERTY(bool isNew READ isNew NOTIFY fileChanged)
  Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoChanged)
  Q_PROPERTY(int revision READ revision NOTIFY changed)                      //!< changes with everything that is drawn or listed
  Q_PROPERTY(qreal canvasWidth READ canvasWidth NOTIFY changed)
  Q_PROPERTY(qreal canvasHeight READ canvasHeight NOTIFY changed)
  Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
  Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
  Q_PROPERTY(QVariantMap sel READ sel NOTIFY selectionChanged)               //!< what the selected item has: kind, text, fontFamily, fontSize, bold, italic, fill, opacity, ...
  Q_PROPERTY(QVariantList handles READ handles NOTIFY selectionChanged)      //!< 8 points {x,y} in canvas units: TL, T, TR, R, BR, B, BL, L (of the item, turned with it)
  Q_PROPERTY(QPointF turnHandle READ turnHandle NOTIFY selectionChanged)    //!< where the turn handle is, in canvas units
  Q_PROPERTY(QVariantList layers READ layers NOTIFY changed)                  //!< [{id, kind, label, locked}] the one in front first
  Q_PROPERTY(bool sampleData READ sampleData WRITE setSampleData NOTIFY changed)   //!< the macros show their sample values (true) or as typed (false)
  Q_PROPERTY(QVariantList guides READ guides NOTIFY guidesChanged)           //!< while an item is dragged: {vertical, at} lines it snapped to, in canvas units

public:
  static TouchTemplateEditor *instance();
  static TouchTemplateEditor *create(QQmlEngine *,QJSEngine *);

  bool isOpen() const {return m_open;}
  QString name() const;
  bool isNew() const {return m_doc.fileName().isEmpty();}
  bool dirty() const {return m_doc.isModified();}
  bool canUndo() const {return m_doc.undoStack()->canUndo();}
  bool canRedo() const {return m_doc.undoStack()->canRedo();}
  int revision() const {return m_revision;}
  qreal canvasWidth() const {return m_doc.doc().canvas.width();}
  qreal canvasHeight() const {return m_doc.doc().canvas.height();}
  QString selectedId() const {return m_selected;}
  bool hasSelection() const {return !m_selected.isEmpty();}
  QVariantMap sel() const;
  QVariantList handles() const;
  QPointF turnHandle() const;
  QVariantList layers() const;
  bool sampleData() const {return m_sample;}
  void setSampleData(bool on);
  QVariantList guides() const {return m_guides;}

  //! what the canvas draws: the document and how its macros are shown
  const tpl::TemplateDocument &document() const {return m_doc.doc();}
  tpl::RenderOptions renderOptions() const;

  // ---- opening and closing ----
  Q_INVOKABLE bool openFile(const QString &path);      //!< edit this template (a legacy .templ opens too; saving writes a .qtempl)
  Q_INVOKABLE void newTemplate(int starter,int width=320,int height=256);   //!< a new one from one of the starters (0 blank)
  Q_INVOKABLE QStringList starterNames() const;
  Q_INVOKABLE bool save(const QString &name);          //!< to the templates folder; false with lastError() when it cannot
  Q_INVOKABLE QString lastError() const {return m_error;}
  Q_INVOKABLE void close();                            //!< leave the editor (the caller has asked about unsaved changes)
  //! a copy of a template file under a new name, in the templates folder
  Q_INVOKABLE bool duplicateFile(const QString &path,const QString &newName);

  // ---- selecting ----
  Q_INVOKABLE QString itemAt(qreal x,qreal y,qreal tolerance) const;   //!< the topmost item at a canvas point ("" for none)
  Q_INVOKABLE void select(const QString &id);
  Q_INVOKABLE bool pointInSelection(qreal x,qreal y,qreal tolerance) const;

  // ---- what the fingers do ----
  Q_INVOKABLE void moveBegin();
  Q_INVOKABLE void moveBy(qreal dx,qreal dy);          //!< from the place the move began, canvas units; snaps to the middle and the edges
  Q_INVOKABLE void moveEnd();
  Q_INVOKABLE void resizeBegin(int handle);            //!< 0..7 as in handles
  Q_INVOKABLE void resizeTo(qreal x,qreal y);          //!< the handle is at this canvas point
  Q_INVOKABLE void resizeEnd();
  Q_INVOKABLE void turnBegin(qreal x,qreal y);          //!< the turn handle was touched at this canvas point
  Q_INVOKABLE void turnTo(qreal x,qreal y);            //!< the turn handle is at this canvas point
  Q_INVOKABLE void turnEnd();
  Q_INVOKABLE void pinchBegin();
  Q_INVOKABLE void pinchBy(qreal scale,qreal degrees); //!< two fingers: the item as it was, scaled and turned about its center
  Q_INVOKABLE void pinchEnd();

  // ---- edits ----
  Q_INVOKABLE void addItem(const QString &kind,qreal centerX,qreal centerY);   //!< text, rect, ellipse, line, replay, bar
  Q_INVOKABLE void addImage(const QString &file,qreal centerX,qreal centerY);
  Q_INVOKABLE void pickImage();                        //!< asks for a picture (the phone's photo picker); it is added to the template when it comes
  bool takePendingImage() {const bool p=m_pendingImage; m_pendingImage=false; return p;}   //!< the next picture picked is for the template
  Q_INVOKABLE void deleteSelected();
  Q_INVOKABLE void duplicateSelected();
  Q_INVOKABLE void setLocked(const QString &id,bool locked);
  Q_INVOKABLE void setProp(const QString &name,const QVariant &value);         //!< one property of the selected item
  // the stops of the gradient of the selected item (fillKind is not solid); colors are "#aarrggbb", positions 0..1 and stay between the neighbors
  Q_INVOKABLE void setGradientStopColor(int index,const QString &color);
  Q_INVOKABLE void setGradientStopPos(int index,qreal pos);
  Q_INVOKABLE int addGradientStop(int afterIndex);     //!< a stop between this one and the next (the last: and the one before); the index of the new one
  Q_INVOKABLE int removeGradientStop(int index);       //!< never fewer than two; the index to show next
  Q_INVOKABLE void reverseGradient();                  //!< the same colors the other way round
  Q_INVOKABLE void moveLayer(int from,int to);         //!< in the list of layers (the front one is 0)
  Q_INVOKABLE void undo();
  Q_INVOKABLE void redo();
  Q_INVOKABLE void clearAll();
  Q_INVOKABLE QVariantList macros() const;             //!< [{tag, name, description, sample}]
  Q_INVOKABLE QVariantList fontFamilies() const;       //!< [{name, other}] the fonts of this device that draw Latin letters; other: made for another script (listed last)

signals:
  void openChanged();
  void fileChanged();
  void dirtyChanged();
  void undoChanged();
  void changed();
  void selectionChanged();
  void guidesChanged();
  void snapped();                                      //!< an item came to rest against the middle or an edge: a tick
  void openNote(const QString &text);                  //!< something was changed in the template that was just opened (the editor shows it)

private:
  explicit TouchTemplateEditor(QObject *parent=nullptr);
  void bump();
  void refreshSelection();
  tpl::Element *selectedElement();
  const tpl::Element *selectedElementConst() const;
  QPolygonF outlineOf(const tpl::Element &e) const;
  QString kindName(tpl::ItemType t) const;
  QString layerLabel(const tpl::Element &e) const;
  void setOpen(bool on);
  QString pathFor(const QString &name) const;

  mutable EditorDocument m_doc;
  bool m_open=false;
  bool m_pendingImage=false;
  bool m_sample=true;
  int m_revision=0;
  QString m_selected;
  QString m_error;
  QVariantList m_guides;
  mutable QVariantList m_fontFamilies;   //!< fontFamilies(), worked out on first use
  // a gesture in progress
  tpl::Element m_before;                 //!< the selected item as it was when the gesture began
  bool m_gesture=false;
  QPointF m_anchorLocal;                 //!< resize: the point of the item that stays where it is (item coordinates)
  int m_handle=-1;
  QPointF m_turnCenter;                  //!< turn: the center of the item on the canvas
  double m_turnStart=0;
};

#endif // TOUCHTEMPLATEEDITOR_H
