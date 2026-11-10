import QtQuick
import QSSTV

// The digital settings one by one: robustness, QAM, bandwidth, interleaving, protection, Reed-Solomon, picture format
BottomSheet {
    id: sheet
    title: qsTr("Digital settings")
    signal editProfiles()
    // the revision is used (and not just read): a statement that only reads it is dropped by the QML compiler and the binding would not follow
    property var options: { const r = TxController.drmRevision; return r >= 0 ? TxController.drmOptions() : [] }
    Flickable {
        width: parent.width
        height: Math.min(col.height, sheet.height * 0.6)
        contentWidth: width
        contentHeight: col.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: col
            width: parent.width
            spacing: 4
            Repeater {
                model: sheet.options
                delegate: Column {
                    id: optionColumn
                    required property var modelData
                    width: col.width
                    spacing: 6
                    Text {
                        text: modelData.title
                        color: Theme.surfaceVariantOn
                        font.pixelSize: Theme.sp(13)
                        leftPadding: 24; topPadding: 8
                    }
                    Flow {
                        x: Theme.margin + 8
                        width: parent.width - 2 * (Theme.margin + 8)
                        spacing: 8
                        Repeater {
                            model: modelData.items
                            delegate: Chip {
                                required property int index
                                required property string modelData
                                readonly property bool selected: index === optionColumn.modelData.index
                                text: modelData
                                icon: selected ? Icons.check : ""
                                tone: selected ? "selected" : "plain"
                                onClicked: TxController.setDrmOption(optionColumn.modelData.key, index)
                            }
                        }
                    }
                }
            }
            PreferenceRow {
                icon: Icons.edit; title: qsTr("Edit the profiles"); summary: qsTr("Names and settings"); chevron: true
                onClicked: sheet.editProfiles()
            }
            Item { width: 1; height: 16 }
        }
    }
}
