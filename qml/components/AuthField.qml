import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: control

    property string label: ""
    property string hint: ""
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias maximumLength: field.maximumLength
    property alias inputMethodHints: field.inputMethodHints
    property alias readOnly: field.readOnly
    property alias validator: field.validator
    signal accepted()
    signal edited()

    implicitHeight: layout.implicitHeight
    implicitWidth: layout.implicitWidth

    function forceInputFocus() {
        field.forceActiveFocus()
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        Text {
            visible: control.label !== ""
            text: control.label
            color: BeyGramTheme.muted
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        TextField {
            id: field
            Layout.fillWidth: true
            implicitHeight: 44
            leftPadding: 14
            rightPadding: 14
            selectByMouse: true
            color: BeyGramTheme.text
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            font.pixelSize: 14
            onAccepted: control.accepted()
            onTextEdited: control.edited()

            background: Rectangle {
                radius: 10
                color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                border.width: field.activeFocus ? 2 : 1
                border.color: field.activeFocus
                              ? BeyGramTheme.accent
                              : Qt.alpha(BeyGramTheme.line, 1.0)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Text {
            visible: control.hint !== ""
            Layout.fillWidth: true
            text: control.hint
            color: BeyGramTheme.muted
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            lineHeight: 1.25
        }
    }
}
