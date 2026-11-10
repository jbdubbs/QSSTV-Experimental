import QtQuick
import QtQuick.Controls.Material
import QSSTV

// Settings > Advanced > Digital (DRM) profiles: the three saved sets of digital settings
SettingsPage {
    id: page
    title: qsTr("Digital (DRM) profiles")
    signal choose(int index)
    Text {
        width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 8
        text: qsTr("A profile is a saved set of digital settings. On the Transmit screen you choose one with a tap instead of setting six things.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
    }
    InlineBanner {
        visible: DrmProfiles.factory
        x: 16; width: parent.width - 32
        warning: false; icon: Icons.info
        text: qsTr("All three profiles are still the same. They have never been set up.")
    }
    Item {
        visible: DrmProfiles.factory
        width: parent.width; height: 64
        WideButton {
            x: 16; width: parent.width - 32; y: 8; height: 48
            text: qsTr("Use typical settings")
            onClicked: DrmProfiles.useTypical()
            Accessible.name: qsTr("Fill in the three profiles Robust, Balanced and Fast")
        }
    }
    Text {
        visible: DrmProfiles.factory
        width: parent.width; leftPadding: 24; rightPadding: 24; bottomPadding: 6
        text: qsTr("Fills in Robust, Balanced and Fast. You can change them after.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap
    }
    Repeater {
        model: DrmProfiles.profiles
        delegate: Rectangle {
            required property var modelData
            x: 16; width: parent.width - 32
            height: cardText.implicitHeight + 28
            radius: 16; color: Theme.surfaceContainer
            Column {
                id: cardText
                x: 16; y: 14; width: parent.width - 32 - 32
                Text { text: modelData.name; color: Theme.surfaceOn; font.pixelSize: Theme.sp(17); font.weight: Font.Medium; width: parent.width; elide: Text.ElideRight }
                Text { text: modelData.line1; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); width: parent.width; wrapMode: Text.WordWrap; topPadding: 2 }
                Text { text: modelData.line2; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12); width: parent.width; wrapMode: Text.WordWrap }
            }
            Icon { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; path: Icons.chevron }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: page.choose(parent.modelData.index) }
            Accessible.role: Accessible.Button
            Accessible.name: modelData.name + ", " + modelData.line1 + ", " + modelData.line2
            Accessible.onPressAction: page.choose(modelData.index)
        }
    }
    Item { width: 1; height: 8; visible: true }
    PreferenceRow {
        icon: Icons.image
        title: qsTr("Show the picture while it arrives"); summary: qsTr("Not with Reed-Solomon coded files.")
        checkable: true; checked: DrmProfiles.showPartial
        onToggled: (c) => DrmProfiles.showPartial = c
    }
}
