import QtQuick
import QtQuick.Controls.Material
import QSSTV

// What goes on the picture besides the picture: whom it is for, the operator's name and the report
BottomSheet {
    id: sheet
    title: qsTr("Details on the picture")
    Column {
        width: parent.width
        spacing: 8
        TextField {
            x: 24; width: parent.width - 48
            placeholderText: qsTr("To (call sign)")
            text: TxController.toCall
            inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
            onEditingFinished: TxController.toCall = text
        }
        TextField {
            x: 24; width: parent.width - 48
            placeholderText: qsTr("Operator name")
            text: TxController.toOperator
            onEditingFinished: TxController.toOperator = text
        }
        TextField {
            x: 24; width: parent.width - 48
            placeholderText: qsTr("Report, for example 59")
            text: TxController.report
            onEditingFinished: TxController.report = text
        }
        Item { width: 1; height: 16 }
    }
}
