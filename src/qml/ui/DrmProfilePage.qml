import QtQuick
import QtQuick.Controls.Material
import QSSTV

// One DRM profile: its name and the six settings, all the choices in view with a line that says what each does
SettingsPage {
    id: page
    property int profileIndex: 0
    readonly property var profile: DrmProfiles.profiles.length > profileIndex ? DrmProfiles.profiles[profileIndex] : ({name: qsTr("Profile"), options: []})
    title: profile.name
    property var options: profile.options
    TextRow {
        label: qsTr("Name"); hint: qsTr("Name")
        text: page.profile.name
        onCommitted: (t) => DrmProfiles.setName(page.profileIndex, t)
    }
    Repeater {
        model: page.options
        delegate: Column {
            id: group
            required property var modelData
            width: parent.width
            spacing: 6
            Text { text: modelData.title; color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; leftPadding: 24; topPadding: 12; Accessible.role: Accessible.Heading }
            Flow {
                x: 24; width: parent.width - 48; spacing: 8
                Repeater {
                    model: group.modelData.items
                    delegate: Chip {
                        required property int index
                        required property string modelData
                        readonly property bool selected: index === group.modelData.index
                        text: modelData
                        icon: selected ? Icons.check : ""
                        tone: selected ? "selected" : "plain"
                        onClicked: DrmProfiles.setOption(page.profileIndex, group.modelData.key, index)
                    }
                }
            }
            Text { text: group.modelData.hint; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12); leftPadding: 24; rightPadding: 24; width: parent.width; wrapMode: Text.WordWrap }
        }
    }
    Item { width: 1; height: 16 }
}
