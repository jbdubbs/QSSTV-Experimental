import QtQuick
import QSSTV

// The digital settings one by one: robustness, QAM, bandwidth, interleaving, protection, Reed-Solomon, picture format
BottomSheet {
    id: sheet
    title: qsTr("Digital settings")
    property var options: { TxController.drmRevision; return TxController.drmOptions() }
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
            Item { width: 1; height: 16 }
        }
    }
}
