import QtQuick
import QtQuick.Controls.Material
import QSSTV

// About: who made it, the version, where the source is, the licenses of what it is built on
BottomSheet {
    id: sheet
    title: qsTr("QSSTV-Experimental")
    property bool licenses: false
    onOpenChanged: if (open) licenses = false

    Column {
        width: parent.width
        visible: !sheet.licenses
        spacing: 2
        Text { x: 24; width: parent.width - 48; text: qsTr("Version %1").arg(AppModel.versionText); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14) }
        Text {
            x: 24; width: parent.width - 48; topPadding: 12; wrapMode: Text.WordWrap
            text: qsTr("Maintained by Jason Weisberger, NT0Y. Built upon QSSTV 9.5.11 by Johan Maes, ON4QZ (copyright 2000 to 2019); the digital pictures use software based on RX/TXAMADRM by PA0MBO.")
            color: Theme.surfaceOn; font.pixelSize: Theme.sp(14)
        }
        Text {
            x: 24; width: parent.width - 48; topPadding: 12; wrapMode: Text.WordWrap
            text: qsTr("Free software under the GNU General Public License version 3. It contains code from Dream (DRM), Phil Karn KA9Q, MiXViews, Maia XML-RPC, the Papirus icons and others.")
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13)
        }
        Item { width: 1; height: 8 }
        PreferenceRow { title: qsTr("Source code and releases"); summary: "github.com/jbdubbs/QSSTV-Experimental"; chevron: true; onClicked: Qt.openUrlExternally("https://github.com/jbdubbs/QSSTV-Experimental") }
        PreferenceRow { title: qsTr("Report a problem"); summary: qsTr("Opens the issue tracker"); chevron: true; onClicked: Qt.openUrlExternally("https://github.com/jbdubbs/QSSTV-Experimental/issues") }
        PreferenceRow { title: qsTr("Privacy policy"); summary: qsTr("Nothing leaves your phone unless you ask"); chevron: true; onClicked: Qt.openUrlExternally("https://jbdubbs.github.io/QSSTV-Experimental/pages/privacy.html") }
        PreferenceRow { title: qsTr("Licenses of the libraries"); chevron: true; onClicked: sheet.licenses = true }
    }
    Column {
        width: parent.width
        visible: sheet.licenses
        Item {
            width: parent.width; height: 40
            Row {
                x: 16; anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Icon { path: Icons.back; size: 20; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("About"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: sheet.licenses = false }
            Accessible.onPressAction: { sheet.licenses = false }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back to About")
        }
        Flickable {
            width: parent.width
            height: sheet.height * 0.55
            contentWidth: width
            contentHeight: licenseText.height + 24
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Text {
                id: licenseText
                x: 24; width: parent.width - 48
                text: AppModel.licensesText()
                textFormat: Text.MarkdownText
                wrapMode: Text.WordWrap
                color: Theme.surfaceOn
                linkColor: Theme.primary
                font.pixelSize: Theme.sp(12)
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
    Item { width: 1; height: 16 }
}
