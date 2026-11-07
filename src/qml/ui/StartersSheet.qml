import QtQuick
import QSSTV

// The ways to begin a new template: blank, or one of the starters
BottomSheet {
    id: sheet
    title: qsTr("Start a new template")
    Repeater {
        model: TemplateEditor.starterNames()
        delegate: PreferenceRow {
            required property string modelData
            required property int index
            icon: index === 0 ? Icons.rectItem : index === 1 ? Icons.barItem : index === 2 ? Icons.textItem : Icons.replayItem
            title: modelData
            summary: index === 0 ? qsTr("Add everything yourself") : index === 1 ? qsTr("A bar along the bottom with your call sign")
                   : index === 2 ? qsTr("Your call sign in white letters with a dark edge") : qsTr("Room for the picture you reply to")
            onClicked: { sheet.open = false; TemplateEditor.newTemplate(index) }
        }
    }
    Item { width: 1; height: 12 }
}
