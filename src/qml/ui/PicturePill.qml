import QtQuick
import QSSTV

// A small dark pill on a picture: the size of the picture, how it is framed, Reset, a hint. With tappable it is a button; the taps it takes reach
// tapGap beyond the pill, so pills that are tappable must be at least 2 x tapGap apart (the Column on the picture keeps them 12 dp apart).
Rectangle {
    id: pill
    property string text
    property string icon           // before the words
    property string trailingIcon   // after the words (a down arrow says that a list opens)
    property bool tappable: false
    readonly property real tapGap: 6
    signal clicked()
    radius: 10
    color: Theme.pictureShade
    implicitWidth: row.implicitWidth + 16
    implicitHeight: row.implicitHeight + (tappable ? 12 : 4)
    opacity: enabled ? 1 : 0.38
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Icon { visible: pill.icon !== ""; path: pill.icon; color: Theme.onPicture; size: 16; anchors.verticalCenter: parent.verticalCenter }
        Text { text: pill.text; color: Theme.onPicture; font.pixelSize: Theme.sp(12); anchors.verticalCenter: parent.verticalCenter }
        Icon { visible: pill.trailingIcon !== ""; path: pill.trailingIcon; color: Theme.onPicture; size: 16; anchors.verticalCenter: parent.verticalCenter }
    }
    TapHandler {
        enabled: pill.tappable
        margin: pill.tapGap
        onTapped: { pill.clicked(); AppModel.haptic(0) }
    }
    property bool announced: tappable   // a pill that only says something is read when it says more than the picture does (the hint), not for the size
    Accessible.ignored: !announced
    Accessible.role: tappable ? Accessible.Button : Accessible.StaticText
    Accessible.name: text
    Accessible.onPressAction: { if (tappable) pill.clicked() }
}
