// AuthField.qml — labelled text field for the auth flow and the editors.
//
// A thin wrapper over DsTextField that keeps the historical AuthField API:
// `hint` is a muted helper line under the box (hidden while `error` is set),
// and `accepted()` / `edited()` re-expose the TextField signals under the
// names the callers already use. Geometry, focus ring and colours all come
// from DsTextField.

import QtQuick
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: control

    property string label: ""
    property string hint: ""
    property string error: ""

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias maximumLength: field.maximumLength
    property alias inputMethodHints: field.inputMethodHints
    property alias readOnly: field.readOnly
    property alias validator: field.validator

    signal accepted()
    signal edited()

    readonly property bool _hasHint: control.hint !== "" && !control.error

    implicitWidth: field.implicitWidth
    implicitHeight: field.implicitHeight
                    + (control._hasHint ? hintText.implicitHeight + Design.space1 : 0)

    function forceInputFocus() {
        field.forceActiveFocus()
    }

    DsTextField {
        id: field

        anchors.left: parent.left
        anchors.right: parent.right
        label: control.label
        error: control.error

        onAccepted: control.accepted()
        onTextEdited: control.edited()
    }

    Text {
        id: hintText

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: field.bottom
        anchors.topMargin: Design.space1
        visible: control._hasHint
        text: control.hint
        color: BeyGramTheme.muted
        font.pixelSize: Design.fontCaption
        wrapMode: Text.WordWrap
    }
}
