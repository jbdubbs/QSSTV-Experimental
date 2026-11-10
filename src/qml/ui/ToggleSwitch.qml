import QtQuick

// An on/off switch in the colors of the look (Material's own draws its "off" in a gray-white that the Night look cannot have). Taps: its own, or none
// when the row around it takes them (interactive: false).
Item {
    id: sw
    property bool checked: false
    property bool interactive: true
    signal toggled()
    function toggle() { checked = !checked }
    implicitWidth: 52
    implicitHeight: 32
    opacity: enabled ? 1 : 0.5
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: sw.checked ? Theme.primary : Theme.surfaceHigh
        border.width: sw.checked ? 0 : 2
        border.color: Theme.outline
        Behavior on color { ColorAnimation { duration: Theme.ms(100) } }
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: sw.checked ? 24 : 16; height: width; radius: width / 2
        x: sw.checked ? parent.width - width - 4 : 8
        color: sw.checked ? Theme.primaryOn : Theme.outline
        Behavior on x { NumberAnimation { duration: Theme.ms(100) } }
        Behavior on width { NumberAnimation { duration: Theme.ms(100) } }
    }
    TapHandler { enabled: sw.interactive && sw.enabled; margin: Theme.tapMargin(sw); onTapped: { sw.toggle(); sw.toggled() } }
    Accessible.role: Accessible.CheckBox
    Accessible.checked: sw.checked
    Accessible.onToggleAction: { sw.toggle(); sw.toggled() }
}
