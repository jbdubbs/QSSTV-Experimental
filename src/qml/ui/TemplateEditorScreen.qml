import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Shapes
import QSSTV

// The template editor: the template over a sample picture, items chosen by a tap, moved, resized and turned with the fingers, their properties
// in a panel under the picture (beside it on a wide screen).
Rectangle {
    id: screen
    color: Theme.surface
    visible: TemplateEditor.open
    z: 56
    property real topInset: 0
    property real bottomInset: 0
    property real leftInset: 0
    property real rightInset: 0
    property bool panelOpen: false
    readonly property bool wide: width >= 720 && width > height
    signal notice(string text)
    MouseArea { anchors.fill: parent }

    readonly property real cw: Math.max(1, TemplateEditor.canvasWidth)
    readonly property real ch: Math.max(1, TemplateEditor.canvasHeight)
    readonly property real fitScale: Math.max(0.1, Math.min((stage.width - 24) / cw, (stage.height - 24) / ch))
    readonly property var s: TemplateEditor.sel
    readonly property bool locked: s.locked === true

    function requestClose() {
        if (TemplateEditor.dirty) keepDialog.open = true
        else { panelOpen = false; TemplateEditor.close() }
    }
    function goBack() {
        if (!TemplateEditor.open) return false
        if (macroSheet.open) macroSheet.open = false
        else if (addSheet.open) addSheet.open = false
        else if (layersSheet.open) layersSheet.open = false
        else if (moreSheet.open) moreSheet.open = false
        else if (nameDialog.open) nameDialog.open = false
        else if (keepDialog.open) keepDialog.open = false
        else if (panelOpen) panelOpen = false
        else requestClose()
        return true
    }
    function saveNow() {
        if (TemplateEditor.isNew) { nameDialog.inputText = ""; nameDialog.open = true }
        else if (TemplateEditor.save(TemplateEditor.name)) notice(qsTr("Template saved"))
        else notice(TemplateEditor.lastError())
    }
    // pointer position (scene) -> canvas units
    function toCanvas(scenePoint) { const p = overlay.mapFromItem(null, scenePoint.x, scenePoint.y); return Qt.point(p.x / fitScale, p.y / fitScale) }
    onVisibleChanged: if (!visible) closeAll()
    function closeAll() {
        panelOpen = false
        macroSheet.open = false; addSheet.open = false; layersSheet.open = false; moreSheet.open = false
        nameDialog.open = false; keepDialog.open = false; clearDialog.open = false
    }

    // for the screenshots of the test harness (tests/qmlui): open a part of the editor
    function harness(what) {
        closeAll()
        if (what === "text") { panelOpen = true; panel.tab = 0 }
        else if (what === "style") { panelOpen = true; panel.tab = 1 }
        else if (what === "place") { panelOpen = true; panel.tab = 2 }
        else if (what === "add") addSheet.open = true
        else if (what === "layers") layersSheet.open = true
        else if (what === "more") moreSheet.open = true
        else if (what === "macros") macroSheet.open = true
        else if (what === "keep") keepDialog.open = true
        else if (what === "name") { nameDialog.inputText = "My bar"; nameDialog.open = true }
    }
    // is a point of the window (scene) on a handle or the turn handle of the chosen item?
    function onHandle(scenePt) {
        if (!TemplateEditor.hasSelection || locked || TemplateEditor.handles.length !== 8) return false
        const p = holder.mapFromItem(null, scenePt.x, scenePt.y)
        const near = (h) => Math.abs(p.x - h.x * fitScale) <= 24 && Math.abs(p.y - h.y * fitScale) <= 24
        const kind = s.kind
        for (let i = 0; i < 8; i++) {
            const shown = kind === "text" ? i % 2 === 0 : kind === "line" ? (i === 3 || i === 7) : true
            if (shown && near(TemplateEditor.handles[i])) return true
        }
        return kind !== "line" && near(TemplateEditor.turnHandle)
    }
    // for the gesture checks of the test harness: where a handle is on the window (-1: the turn handle)
    function harnessPoint(i) {
        const h = i < 0 ? TemplateEditor.turnHandle : TemplateEditor.handles[i]
        return holder.mapToItem(null, h.x * fitScale, h.y * fitScale)
    }
    // ---- the bar on top ----
    Item {
        id: topBar
        y: screen.topInset; x: screen.leftInset; width: parent.width - screen.leftInset - screen.rightInset; height: Theme.appBarHeight
        Item {
            x: 4; width: Theme.tap; height: parent.height
            Icon { anchors.centerIn: parent; path: Icons.close; color: Theme.surfaceOn }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: screen.requestClose() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Close the editor")
            Accessible.onPressAction: screen.requestClose()
        }
        Column {
            x: 56; width: parent.width - 56 - 3 * Theme.tap - 8; anchors.verticalCenter: parent.verticalCenter
            Text { width: parent.width; text: TemplateEditor.isNew ? qsTr("New template") : TemplateEditor.name; color: Theme.surfaceOn; font.pixelSize: Theme.sp(17); font.weight: Font.Medium; elide: Text.ElideRight; Accessible.role: Accessible.Heading }
            Text { width: parent.width; text: TemplateEditor.isNew ? qsTr("Not saved yet") : TemplateEditor.dirty ? qsTr("Edited") : qsTr("Saved"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
        }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 4; height: parent.height
            Item {
                width: Theme.tap; height: parent.height; opacity: TemplateEditor.canUndo ? 1 : 0.4
                Icon { anchors.centerIn: parent; path: Icons.undo; color: Theme.surfaceOn }
                TapHandler { margin: Theme.tapMargin(parent); enabled: TemplateEditor.canUndo; onTapped: TemplateEditor.undo() }
                Accessible.role: Accessible.Button; Accessible.name: qsTr("Undo")
                Accessible.onPressAction: if (TemplateEditor.canUndo) TemplateEditor.undo()
            }
            Item {
                width: Theme.tap; height: parent.height; opacity: TemplateEditor.canRedo ? 1 : 0.4
                Icon { anchors.centerIn: parent; path: Icons.redo; color: Theme.surfaceOn }
                TapHandler { margin: Theme.tapMargin(parent); enabled: TemplateEditor.canRedo; onTapped: TemplateEditor.redo() }
                Accessible.role: Accessible.Button; Accessible.name: qsTr("Redo")
                Accessible.onPressAction: if (TemplateEditor.canRedo) TemplateEditor.redo()
            }
            Item {
                width: Theme.tap; height: parent.height
                Icon { anchors.centerIn: parent; path: Icons.check; color: Theme.primary }
                TapHandler { margin: Theme.tapMargin(parent); onTapped: screen.saveNow() }
                Accessible.role: Accessible.Button; Accessible.name: qsTr("Save the template")
                Accessible.onPressAction: screen.saveNow()
            }
        }
    }

    // ---- the bar at the bottom ----
    Item {
        id: bottomBar
        anchors.bottom: parent.bottom; x: screen.leftInset; width: (screen.wide && screen.panelOpen ? parent.width - 360 : parent.width) - screen.leftInset
        height: 72 + screen.bottomInset
        Rectangle { anchors.fill: parent; color: Theme.surfaceContainer }
        Row {
            y: 8; width: parent.width; height: 56
            Repeater {
                model: [{id: "add", t: qsTr("Add"), ic: Icons.add}, {id: "edit", t: qsTr("Edit"), ic: Icons.edit}, {id: "layers", t: qsTr("Layers"), ic: Icons.layers}, {id: "more", t: qsTr("More"), ic: Icons.more}]
                delegate: Item {
                    required property var modelData
                    width: parent.width / 4; height: 56
                    readonly property bool on: modelData.id === "edit" && screen.panelOpen
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 0; width: 56; height: 32; radius: 16; color: parent.on ? Theme.primaryContainer : "transparent" }
                    Icon { anchors.horizontalCenter: parent.horizontalCenter; y: 4; path: modelData.ic; color: parent.on ? Theme.primaryContainerOn : Theme.surfaceVariantOn }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 36; text: modelData.t; font.pixelSize: Theme.sp(12); font.weight: Font.Medium; color: Theme.surfaceVariantOn }
                    TapHandler {
                        margin: Theme.tapMargin(parent)
                        onTapped: screen.barAction(parent.modelData.id)
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.t
                    Accessible.onPressAction: screen.barAction(modelData.id)
                }
            }
        }
    }
    function barAction(id) {
        if (id === "add") addSheet.open = true
        else if (id === "layers") layersSheet.open = true
        else if (id === "more") moreSheet.open = true
        else if (id === "edit") {
            if (TemplateEditor.hasSelection) panelOpen = !panelOpen
            else notice(qsTr("Tap an item to edit it, or add one"))
        }
    }

    // ---- the panel: under the picture on a phone, beside it on a wide screen ----
    EditPanel {
        id: panel
        visible: screen.panelOpen && TemplateEditor.hasSelection
        z: 3
        readonly property real phoneHeight: Math.min(screen.height > 1000 ? 460 : 330, screen.height * 0.46)
        x: screen.wide ? screen.width - 360 : screen.leftInset
        y: screen.wide ? screen.topInset : bottomBar.y - phoneHeight
        width: screen.wide ? 360 : screen.width - screen.leftInset - screen.rightInset
        height: screen.wide ? screen.height - screen.topInset - 0 : phoneHeight
        bottomInset: screen.wide ? screen.bottomInset : 0
        onOpenMacros: macroSheet.open = true
    }

    // ---- the stage: the picture ----
    Item {
        id: stage
        x: screen.leftInset; y: topBar.y + topBar.height
        width: (screen.wide && screen.panelOpen ? screen.width - 360 : screen.width) - screen.leftInset - screen.rightInset
        height: (panel.visible && !screen.wide ? panel.y : bottomBar.y) - y
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.surfaceHigh }

        Item {
            id: holder
            // at the top on a phone (the sheets and the panel come up from below and must not cover it), in the middle beside the panel
            x: (parent.width - width) / 2
            y: screen.wide ? (parent.height - height) / 2 : 12
            width: screen.cw * screen.fitScale; height: screen.ch * screen.fitScale
            TemplateCanvas { id: canvas; anchors.fill: parent; revision: TemplateEditor.revision }

            // the touches that are not on a handle: choose, move
            Item {
                id: overlay
                anchors.fill: parent
                property point grab: Qt.point(0, 0)
                TapHandler {
                    onTapped: (pt) => {
                        const c = screen.toCanvas(pt.scenePosition)
                        TemplateEditor.select(TemplateEditor.itemAt(c.x, c.y, 10 / screen.fitScale))
                        if (TemplateEditor.hasSelection === false) screen.panelOpen = false
                    }
                    onDoubleTapped: (pt) => {
                        const c = screen.toCanvas(pt.scenePosition)
                        const id = TemplateEditor.itemAt(c.x, c.y, 10 / screen.fitScale)
                        TemplateEditor.select(id)
                        if (id !== "" && screen.s.kind === "text") { screen.panelOpen = true; panel.tab = 0 }
                    }
                }
                DragHandler {
                    id: mover
                    maximumPointCount: 1
                    target: null
                    property bool moving: false
                    onActiveChanged: {
                        if (active) {
                            if (screen.onHandle(centroid.scenePressPosition)) { moving = false; return }   // a handle has the finger, not the item under it
                            const c = screen.toCanvas(centroid.scenePressPosition)
                            const id = TemplateEditor.itemAt(c.x, c.y, 10 / screen.fitScale)
                            if (id === "") { moving = false; return }
                            TemplateEditor.select(id)
                            moving = !screen.locked
                            if (moving) TemplateEditor.moveBegin()
                        } else if (moving) {
                            TemplateEditor.moveEnd()
                            moving = false
                        }
                    }
                    onTranslationChanged: if (moving) TemplateEditor.moveBy(translation.x / screen.fitScale, translation.y / screen.fitScale)
                }
                PinchHandler {
                    id: pincher
                    minimumPointCount: 2; maximumPointCount: 2
                    target: null
                    property bool on: false
                    onActiveChanged: {
                        if (active) {
                            const c = screen.toCanvas(centroid.scenePosition)
                            on = TemplateEditor.hasSelection && !screen.locked && TemplateEditor.pointInSelection(c.x, c.y, 40 / screen.fitScale)
                            if (on) TemplateEditor.pinchBegin()
                        } else if (on) { TemplateEditor.pinchEnd(); on = false }
                    }
                    onActiveScaleChanged: if (on) TemplateEditor.pinchBy(activeScale, rotation)
                    onRotationChanged: if (on) TemplateEditor.pinchBy(activeScale, rotation)
                }
            }

            // the guides an item snapped to
            Repeater {
                model: TemplateEditor.guides
                delegate: Rectangle {
                    required property var modelData
                    visible: modelData.at !== undefined
                    color: Theme.primary
                    x: modelData.vertical ? modelData.at * screen.fitScale - 0.5 : 0
                    y: modelData.vertical ? 0 : modelData.at * screen.fitScale - 0.5
                    width: modelData.vertical ? 1 : holder.width
                    height: modelData.vertical ? holder.height : 1
                }
            }

            // the selection: a frame, the handles, the turn handle
            Shape {
                visible: TemplateEditor.hasSelection
                anchors.fill: parent
                ShapePath {
                    strokeColor: screen.locked ? Theme.outline : Theme.primary
                    strokeWidth: 2
                    fillColor: "transparent"
                    startX: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[0].x * screen.fitScale : 0
                    startY: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[0].y * screen.fitScale : 0
                    PathLine { x: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[2].x * screen.fitScale : 0; y: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[2].y * screen.fitScale : 0 }
                    PathLine { x: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[4].x * screen.fitScale : 0; y: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[4].y * screen.fitScale : 0 }
                    PathLine { x: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[6].x * screen.fitScale : 0; y: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[6].y * screen.fitScale : 0 }
                    PathLine { x: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[0].x * screen.fitScale : 0; y: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[0].y * screen.fitScale : 0 }
                }
            }
            // the model is the number 8, not the list of points: a new list at every change of the item would rebuild the handles, and the one under the
            // finger would be gone at the first movement
            Repeater {
                model: TemplateEditor.hasSelection && !screen.locked ? 8 : 0
                delegate: Item {
                    id: hnd
                    required property int index
                    readonly property point spot: TemplateEditor.handles.length === 8 ? TemplateEditor.handles[index] : Qt.point(-100, -100)
                    // free text has only corners; a line only its two ends
                    readonly property bool shown: screen.s.kind === "text" ? index % 2 === 0 : screen.s.kind === "line" ? (index === 3 || index === 7) : true
                    visible: shown
                    x: spot.x * screen.fitScale - 24; y: spot.y * screen.fitScale - 24
                    width: 48; height: 48
                    Rectangle { anchors.centerIn: parent; width: 14; height: 14; radius: 7; color: "white"; border.width: 2; border.color: Theme.primary }
                    property point grab: Qt.point(0, 0)
                    DragHandler {
                        target: null
                        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.CanTakeOverFromHandlersOfSameType   // and nothing takes the finger from the handle
                        onActiveChanged: {
                            if (active) {
                                const c = screen.toCanvas(centroid.scenePressPosition)
                                hnd.grab = Qt.point(c.x - hnd.spot.x, c.y - hnd.spot.y)
                                TemplateEditor.resizeBegin(hnd.index)
                            } else TemplateEditor.resizeEnd()
                        }
                        onCentroidChanged: if (active) {
                            const c = screen.toCanvas(centroid.scenePosition)
                            TemplateEditor.resizeTo(c.x - hnd.grab.x, c.y - hnd.grab.y)
                        }
                    }
                    Accessible.role: Accessible.Slider
                    Accessible.name: qsTr("Resize handle")
                    Accessible.ignored: true
                }
            }
            Item {
                id: turner
                visible: TemplateEditor.hasSelection && !screen.locked && screen.s.kind !== "line"
                x: TemplateEditor.turnHandle.x * screen.fitScale - 24; y: TemplateEditor.turnHandle.y * screen.fitScale - 24
                width: 48; height: 48
                Rectangle { anchors.centerIn: parent; width: 26; height: 26; radius: 13; color: Theme.primary
                            Icon { anchors.centerIn: parent; path: Icons.rotate; color: Theme.primaryOn; size: 16 } }
                DragHandler {
                    target: null
                    grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.CanTakeOverFromHandlersOfSameType   // and nothing takes the finger from the handle
                    onActiveChanged: {
                        if (active) { const c = screen.toCanvas(centroid.scenePressPosition); TemplateEditor.turnBegin(c.x, c.y) }
                        else TemplateEditor.turnEnd()
                    }
                    onCentroidChanged: if (active) { const c = screen.toCanvas(centroid.scenePosition); TemplateEditor.turnTo(c.x, c.y) }
                }
                Accessible.ignored: true
            }
            // Duplicate, Delete, Lock beside the selection
            Rectangle {
                id: pill
                visible: TemplateEditor.hasSelection
                readonly property var hs: TemplateEditor.handles
                readonly property real topY: hs.length === 8 ? Math.min(hs[0].y, hs[2].y, hs[4].y, hs[6].y) * screen.fitScale : 0
                readonly property real botY: hs.length === 8 ? Math.max(hs[0].y, hs[2].y, hs[4].y, hs[6].y) * screen.fitScale : 0
                readonly property real leftX: hs.length === 8 ? Math.min(hs[0].x, hs[2].x, hs[4].x, hs[6].x) * screen.fitScale : 0
                x: Math.max(0, Math.min(holder.width - width, leftX))
                y: (topY - height - 36 >= 0) ? topY - height - 36 : Math.min(holder.height - height, botY + 14)
                width: 3 * 48 + 12; height: 48; radius: 24
                color: "#2f3133"
                Row {
                    x: 6; height: parent.height
                    Repeater {
                        model: [{id: "dup", ic: Icons.copy, t: qsTr("Duplicate")}, {id: "del", ic: Icons.remove, t: qsTr("Delete")}, {id: "lock", ic: Icons.lock, t: screen.locked ? qsTr("Unlock") : qsTr("Lock")}]
                        delegate: Item {
                            required property var modelData
                            width: 48; height: 48
                            Icon { anchors.centerIn: parent; path: modelData.ic; color: modelData.id === "lock" && screen.locked ? "#8fdcf5" : "#f1f0f4" }
                            TapHandler { onTapped: screen.pillAction(parent.modelData.id) }
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.t
                            Accessible.onPressAction: screen.pillAction(modelData.id)
                        }
                    }
                }
            }
        }
    }
    function pillAction(id) {
        if (id === "dup") TemplateEditor.duplicateSelected()
        else if (id === "del") { TemplateEditor.deleteSelected(); panelOpen = false }
        else if (id === "lock") TemplateEditor.setLocked(TemplateEditor.selectedId, !locked)
    }

    // ---- sheets and dialogs ----
    BottomSheet {
        id: addSheet
        title: qsTr("Add")
        Flow {
            width: parent.width; leftPadding: 20; rightPadding: 20; topPadding: 4; bottomPadding: 12; spacing: 12
            Repeater {
                model: [{k: "text", t: qsTr("Text"), ic: Icons.textItem}, {k: "rect", t: qsTr("Rectangle"), ic: Icons.rectItem}, {k: "ellipse", t: qsTr("Ellipse"), ic: Icons.ellipseItem},
                        {k: "line", t: qsTr("Line"), ic: Icons.lineItem}, {k: "image", t: qsTr("Picture"), ic: Icons.image}, {k: "replay", t: qsTr("Replay window"), ic: Icons.replayItem},
                        {k: "bar", t: qsTr("Color bar"), ic: Icons.barItem}]
                delegate: Item {
                    required property var modelData
                    width: 80; height: 92
                    Rectangle { x: 12; width: 56; height: 56; radius: 16; color: Theme.primaryContainer
                                Icon { anchors.centerIn: parent; path: modelData.ic; color: Theme.primaryContainerOn } }
                    Text { y: 62; width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData.t; color: Theme.surfaceOn; font.pixelSize: Theme.sp(12); wrapMode: Text.WordWrap }
                    TapHandler { margin: 4; onTapped: screen.addKind(parent.modelData.k) }
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.t
                    Accessible.onPressAction: screen.addKind(modelData.k)
                }
            }
        }
    }
    function addKind(k) {
        addSheet.open = false
        if (k === "image") { TemplateEditor.pickImage(); return }
        TemplateEditor.addItem(k, cw / 2, ch / 2)
        panelOpen = true
        panel.tab = k === "text" ? 0 : 1
    }
    BottomSheet {
        id: layersSheet
        title: qsTr("Layers")
        ListView {
            id: layerList
            width: parent.width; height: Math.min(contentHeight, 320)
            interactive: contentHeight > height
            clip: true
            model: TemplateEditor.layers
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: layerList.width; height: 56
                color: modelData.id === TemplateEditor.selectedId ? Theme.primaryContainer : "transparent"
                Row {
                    anchors.fill: parent; leftPadding: 16; spacing: 8
                    Rectangle { width: 40; height: 32; radius: 6; color: Theme.surfaceHigh; anchors.verticalCenter: parent.verticalCenter
                                Icon { anchors.centerIn: parent; size: 20
                                       path: modelData.kind === "text" ? Icons.textItem : modelData.kind === "rect" ? Icons.rectItem : modelData.kind === "ellipse" ? Icons.ellipseItem : modelData.kind === "line" ? Icons.lineItem : modelData.kind === "image" ? Icons.image : Icons.replayItem } }
                    Text { width: parent.width - 16 - 40 - 3 * 48 - 4 * 8 - 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.surfaceOn; font.pixelSize: Theme.sp(15); elide: Text.ElideRight }
                    Item { width: 48; height: 56; opacity: index > 0 ? 1 : 0.3
                           Icon { anchors.centerIn: parent; path: Icons.up }
                           TapHandler { enabled: index > 0; onTapped: TemplateEditor.moveLayer(index, index - 1) }
                           Accessible.role: Accessible.Button; Accessible.name: qsTr("Bring forward")
                           Accessible.onPressAction: if (index > 0) TemplateEditor.moveLayer(index, index - 1) }
                    Item { width: 48; height: 56; opacity: index < layerList.count - 1 ? 1 : 0.3
                           Icon { anchors.centerIn: parent; path: Icons.down }
                           TapHandler { enabled: index < layerList.count - 1; onTapped: TemplateEditor.moveLayer(index, index + 1) }
                           Accessible.role: Accessible.Button; Accessible.name: qsTr("Send backward")
                           Accessible.onPressAction: if (index < layerList.count - 1) TemplateEditor.moveLayer(index, index + 1) }
                    Item { width: 48; height: 56
                           Icon { anchors.centerIn: parent; path: Icons.lock; color: modelData.locked ? Theme.primary : Theme.outlineVariant }
                           TapHandler { onTapped: TemplateEditor.setLocked(modelData.id, !modelData.locked) }
                           Accessible.role: Accessible.CheckBox; Accessible.name: qsTr("Lock"); Accessible.checked: modelData.locked
                           Accessible.onPressAction: TemplateEditor.setLocked(modelData.id, !modelData.locked) }
                }
                TapHandler { onTapped: { TemplateEditor.select(modelData.id); layersSheet.open = false } }
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Accessible.onPressAction: { TemplateEditor.select(modelData.id); layersSheet.open = false }
            }
        }
        Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 12; wrapMode: Text.WordWrap
               text: qsTr("The top of the list is in front. Tap an item to select it."); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
    }
    BottomSheet {
        id: moreSheet
        title: qsTr("More")
        PreferenceRow {
            title: qsTr("Preview with sample data"); summary: qsTr("Shows N0CALL, W1AW and 595 instead of the codes")
            checkable: true; checked: TemplateEditor.sampleData
            onToggled: (on) => TemplateEditor.sampleData = on
        }
        PreferenceRow {
            title: qsTr("Save a copy as..."); chevron: true
            onClicked: { moreSheet.open = false; nameDialog.inputText = ""; nameDialog.open = true }
        }
        PreferenceRow {
            title: qsTr("Clear everything"); chevron: false
            onClicked: { moreSheet.open = false; clearDialog.open = true }
        }
    }
    BottomSheet {
        id: macroSheet
        title: qsTr("Insert a macro")
        ListView {
            id: macroList
            width: parent.width; height: Math.min(contentHeight, screen.height * 0.55)
            clip: true
            model: TemplateEditor.macros()
            delegate: Item {
                required property var modelData
                width: macroList.width; height: Math.max(56, macroCol.implicitHeight + 16)
                Rectangle { x: 24; anchors.verticalCenter: parent.verticalCenter; width: 40; height: 26; radius: 6; color: Theme.primaryContainer
                            Text { anchors.centerIn: parent; text: "%" + modelData.tag; color: Theme.primaryContainerOn; font.pixelSize: Theme.sp(13); font.family: "monospace" } }
                Column {
                    id: macroCol
                    x: 76; width: parent.width - 76 - 110; anchors.verticalCenter: parent.verticalCenter
                    Text { width: parent.width; text: modelData.name; color: Theme.surfaceOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap }
                    Text { width: parent.width; text: modelData.description; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12); wrapMode: Text.WordWrap }
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter; width: 80; horizontalAlignment: Text.AlignRight
                       text: modelData.sample; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); elide: Text.ElideRight }
                TapHandler { onTapped: screen.insertMacro(parent.modelData.tag) }
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name + ", %" + modelData.tag
                Accessible.onPressAction: screen.insertMacro(modelData.tag)
            }
        }
    }
    function insertMacro(tag) {
        macroSheet.open = false
        const t = (s.text !== undefined ? s.text : "") + "%" + tag
        TemplateEditor.setProp("text", t)
    }
    AskDialog {
        id: keepDialog
        title: qsTr("Keep the changes?")
        text: qsTr("This template has changes that are not saved.")
        buttons: [{text: qsTr("Keep editing")}, {text: qsTr("Discard")}, {text: qsTr("Save"), primary: true}]
        onChosen: (i) => {
            if (i === 1) { screen.panelOpen = false; TemplateEditor.close() }
            else if (i === 2) {
                if (TemplateEditor.isNew) { nameDialog.inputText = ""; nameDialog.afterSave = true; nameDialog.open = true }
                else if (TemplateEditor.save(TemplateEditor.name)) { screen.panelOpen = false; TemplateEditor.close() }
                else screen.notice(TemplateEditor.lastError())
            }
        }
    }
    AskDialog {
        id: nameDialog
        property bool afterSave: false
        title: qsTr("Name the template")
        text: qsTr("It is kept with your other templates and shows in the Gallery and on the Transmit screen.")
        input: true
        buttons: [{text: qsTr("Cancel")}, {text: qsTr("Save"), primary: true}]
        onChosen: (i) => {
            if (i !== 1) { afterSave = false; return }
            if (TemplateEditor.save(inputText)) {
                screen.notice(qsTr("Template saved"))
                if (afterSave) { screen.panelOpen = false; TemplateEditor.close() }
            } else screen.notice(TemplateEditor.lastError())
            afterSave = false
        }
    }
    AskDialog {
        id: clearDialog
        title: qsTr("Clear everything?")
        text: qsTr("Every item is removed. Undo brings them back.")
        buttons: [{text: qsTr("Cancel")}, {text: qsTr("Clear"), primary: true}]
        onChosen: (i) => { if (i === 1) { TemplateEditor.clearAll(); screen.panelOpen = false } }
    }
    Connections {
        target: TemplateEditor
        function onSnapped() { AppModel.haptic(0) }
    }
}
