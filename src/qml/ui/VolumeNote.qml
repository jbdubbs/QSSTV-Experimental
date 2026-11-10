import QtQuick
import QSSTV

// The volume keys of the phone change another device than the one the sound goes to (#182): says which, and where it may offers the system default,
// where the sound follows the keys
Column {
    id: note
    objectName: "volumeNote"
    property bool canSwitch: true
    property int textAlign: Text.AlignLeft
    visible: AppModel.volumeMismatch
    spacing: 4
    Text {
        objectName: "volumeNoteText"
        width: parent.width
        wrapMode: Text.WordWrap
        horizontalAlignment: note.textAlign
        text: qsTr("The volume keys change %1, not %2.").arg(AppModel.volumeKeysDevice).arg(AppModel.routeText)
        color: Theme.warn
        font.pixelSize: Theme.sp(13)
        Accessible.role: Accessible.StaticText
    }
    ThemedButton {
        objectName: "volumeNoteSwitch"
        visible: note.canSwitch
        flat: true
        text: qsTr("Use system default")
        onClicked: AppModel.useSystemDefaultOutput()
    }
}
