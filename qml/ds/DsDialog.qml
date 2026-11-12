// DsDialog - modal confirmation dialog for the BeyGram design system.
//
// Presentation only: the parent owns the action and opens the dialog. The
// primary button emits accepted() and closes the dialog unless the handler
// sets `busy` (for example while an async request is in flight); the secondary
// button emits rejected() and closes. The default `content` property places
// arbitrary controls between the description and the footer. Escape and a
// press on the scrim reject the dialog.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

Dialog {
    id: root

    property string description: ""
    property string primaryText: "OK"
    property string secondaryText: "Cancel"
    property string tone: "primary" // primary | danger
    property bool busy: false

    default property alias content: contentColumn.data

    function _accept() {
        if (root.busy)
            return
        root.result = Dialog.Accepted
        root.accepted()
        // A handler that starts an async action sets `busy` synchronously, in
        // which case the dialog stays open until the caller closes it.
        if (!root.busy)
            root.close()
    }

    function _reject() {
        if (root.busy)
            return
        root.reject()
    }

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 460
    padding: Design.space5
    spacing: 0
    modal: true
    focus: true
    closePolicy: root.busy ? Popup.NoAutoClose
                           : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    Overlay.modal: Rectangle {
        color: Qt.alpha("#000000", Design.opacityScrim)
    }

    background: Rectangle {
        radius: Design.radiusXl
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
    }

    // The title is rendered in the content column, not in a style header.
    header: null

    contentItem: ColumnLayout {
        spacing: Design.space3
        implicitWidth: 420 // 460 - 2 * Design.space5

        Text {
            Layout.fillWidth: true
            visible: root.title.length > 0
            text: root.title
            color: BeyGramTheme.text
            font.pixelSize: Design.fontTitle
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            lineHeight: Design.lineTight
        }

        Text {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            wrapMode: Text.WordWrap
            lineHeight: Design.lineNormal
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: Design.space2
        }
    }

    footer: Item {
        implicitHeight: 36 + Design.space5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space5
            anchors.rightMargin: Design.space5
            anchors.bottomMargin: Design.space5
            spacing: Design.space2

            Item { Layout.fillWidth: true }

            BusyIndicator {
                running: root.busy
                visible: root.busy
                implicitWidth: 18
                implicitHeight: 18
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }

            SecondaryButton {
                visible: root.secondaryText.length > 0
                text: root.secondaryText
                enabled: !root.busy
                onClicked: root._reject()
            }

            PrimaryButton {
                visible: root.primaryText.length > 0
                text: root.primaryText
                tone: root.tone === "danger" ? "danger" : "neutral"
                enabled: !root.busy
                onClicked: root._accept()
            }
        }
    }
}
