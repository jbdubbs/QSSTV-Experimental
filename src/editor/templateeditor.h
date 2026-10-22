/***************************************************************************
 *   The template editor window: menus, tools, docks and file handling      *
 *   around an EditorDocument and its EditorCanvas.                         *
 *                                                                          *
 *   The window knows nothing about the rest of the application: whoever    *
 *   opens it passes in the templates folder, the mode sizes, sample macro  *
 *   values and the picture to show under the template, and listens to      *
 *   templateSaved().                                                       *
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 ***************************************************************************/
#ifndef TEMPLATEEDITOR_H
#define TEMPLATEEDITOR_H

#include <QHash>
#include <QMainWindow>
#include <functional>
#include "canvassizedialog.h"
#include "editorcanvas.h"
#include "editordocument.h"

class QActionGroup;
class QLabel;
class QMenu;
class LayersPanel;
class MacroPalette;
class PropertiesPanel;

class TemplateEditor : public QMainWindow
{
  Q_OBJECT
public:
  explicit TemplateEditor(QWidget *parent = nullptr);
  ~TemplateEditor() override;

  EditorDocument *document() { return &doc; }
  EditorCanvas *canvas() { return view; }

  // --- what the application passes in ---------------------------------------------------------------------
  void setTemplatesDir(const QString &dir) { templatesDir = dir; }
  void setSizePresets(const QVector<SizePreset> &presets) { sizePresets = presets; }
  void setDefaultCanvasSize(const QSize &size) { defaultSize = size; }
  void setSampleValues(const QHash<QChar, QString> &values);
  void setPreviewPicture(const QImage &img);

  // --- files; all of them ask about unsaved changes first --------------------------------------------------
  bool openFile(const QString &fileName);      //!< false if cancelled or unreadable (the user is told why)
  bool newTemplate(const QSize &size);
  bool newTemplateDialog();                    //!< asks for the size
  bool save();
  bool saveAs();
  bool maybeSave();                            //!< true when it is safe to discard the document
  bool saveTo(const QString &fileName);        //!< no dialogs
  /*! the canvas gets a new size, the items optionally scaled along: one undo step */
  void applyCanvasSize(const QSize &size, bool scaleItems);
  /*! unsaved-changes prompt; tests replace it */
  enum Choice { Save, Discard, Cancel };
  void setUnsavedHandler(std::function<Choice()> h) { unsavedHandler = h; }
  /*! where errors are shown (title, text); a message box by default, tests collect them */
  void setMessageHandler(std::function<void(const QString &, const QString &)> h) { messageHandler = h; }

signals:
  void templateSaved(const QString &fileName);   //!< a file was written: the gallery and the TX template list should refresh
  void helpRequested();

protected:
  void closeEvent(QCloseEvent *e) override;
  void changeEvent(QEvent *e) override;

private slots:
  void updateTitle();
  void updateActions();
  void updateStatus();
  void showContextMenu(const QPoint &globalPos);
  void insertImage();
  void changeCanvasSize();
  void revert();
  void macroChosen(const QString &macro);
  void toolChanged(int tool);
  void cursorMoved(const QPointF &pos, bool inside);

private:
  void createActions();
  void createMenus();
  void createToolBars();
  void createDocks();
  void refreshIcons();
  void readSettings();
  void writeSettings();
  void addRecent(const QString &fileName);
  void updateRecentMenu();
  QAction *makeAction(const QString &text, const QString &icon, const QKeySequence &shortcut, const QString &tip);
  QAction *makeAction(const QString &text, const QString &icon, QKeySequence::StandardKey key, const QString &tip);
  QString defaultFileName() const;
  Choice askUnsaved();

  EditorDocument doc;
  EditorCanvas *view;
  PropertiesPanel *properties;
  LayersPanel *layers;
  MacroPalette *macros;
  QWidget *banner;
  QLabel *bannerLabel;
  QLabel *cursorLabel, *zoomLabel, *sizeLabel, *selectionLabel;
  QMenu *recentMenu, *contextMenu;
  QActionGroup *toolGroup;

  QAction *newAct, *openAct, *saveAct, *saveAsAct, *revertAct, *closeAct;
  QAction *undoAct, *redoAct, *cutAct, *copyAct, *pasteAct, *deleteAct, *duplicateAct, *selectAllAct, *clearAllAct;
  QAction *selectTool, *textTool, *rectTool, *ellipseTool, *lineTool, *replayTool, *imageAct;
  QAction *alignActs[6], *distributeH, *distributeV;
  QAction *frontAct, *forwardAct, *backwardAct, *backAct, *lockAct;
  QAction *zoomInAct, *zoomOutAct, *zoomFitAct, *zoom100Act;
  QAction *gridAct, *snapGridAct, *snapObjectsAct, *previewMacrosAct, *previewPictureAct, *canvasSizeAct, *helpAct;
  QList<QAction *> recentActs;

  QString templatesDir;
  QVector<SizePreset> sizePresets;
  QSize defaultSize = QSize(320, 256);
  QImage previewPicture;
  std::function<Choice()> unsavedHandler;
  std::function<void(const QString &, const QString &)> messageHandler;
  void problem(const QString &title, const QString &text);
};

#endif  // TEMPLATEEDITOR_H
