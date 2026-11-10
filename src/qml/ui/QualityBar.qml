import QtQuick

// How far a measurement is: the bar, a mark where the result can first be saved, and a word at each end
Item {
    id: bar
    property real value: 0
    property real mark: 0.4
    property string leftText
    property string rightText
    implicitHeight: 8 + 6 + Math.max(l.implicitHeight, r.implicitHeight)
    Rectangle {
        id: track
        x: 24; width: parent.width - 48; height: 8; radius: 4
        color: Theme.outlineVariant
        clip: true
        Rectangle { width: parent.width * Math.max(0, Math.min(1, bar.value)); height: parent.height; radius: 4; color: Theme.primary }
        Rectangle { x: parent.width * bar.mark - 1; width: 2; height: parent.height; color: Theme.surfaceOn; opacity: 0.5 }
    }
    Text { id: l; x: 24; y: 14; text: bar.leftText; color: Theme.surfaceOn; font.pixelSize: Theme.sp(12); font.weight: Font.Medium }
    Text { id: r; anchors.right: parent.right; anchors.rightMargin: 24; y: 14; text: bar.rightText; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
    Accessible.role: Accessible.ProgressBar
    Accessible.name: leftText + ", " + rightText
}
