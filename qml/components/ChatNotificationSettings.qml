// ChatNotificationSettings - per-chat notification options dialog.
//
// The parent owns persistence: it assigns `muted`, `sound` and `preview`
// (typically from the chat's settings) and applies the values once
// `saveRequested(muted, sound, preview)` is emitted. Edits are kept in local
// draft properties so closing with Cancel leaves the bound values untouched.
// This file only renders the form; it has no network, logging or C++ calls.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: root

    property bool muted: false
    property bool sound: true
    property bool preview: true

    signal saveRequested(bool muted, bool sound, bool preview)

    // Draft state: what the checkboxes currently show. Copied from the
    // incoming properties each time the dialog opens.
    property bool draftMuted: false
    property bool draftSound: true
    property bool draftPreview: true

    function resetDraft() {
        root.draftMuted = root.muted
        root.draftSound = root.sound
        root.draftPreview = root.preview
        // A user toggle replaces the `checked` binding, so sync the boxes
        // explicitly as well: reopening the dialog must show current values.
        muteBox.checked = root.draftMuted
        soundBox.checked = root.draftSound
        previewBox.checked = root.draftPreview
    }

    function save() {
        root.saveRequested(root.draftMuted, root.draftSound, root.draftPreview)
        root.close()
    }

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 380
    padding: 0
    modal: true
    focus: true
    title: qsTr("Notifications")
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: resetDraft()

    Overlay.modal: Rectangle {
        color: Qt.alpha("#000000", 0.35)
    }

    background: Rectangle {
        radius: 16
        color: BeyGramTheme.surface
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 1.0)
    }

    header: Item {
        implicitHeight: 52

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    contentItem: ColumnLayout {
        spacing: 4
        implicitWidth: 380

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 2
            text: qsTr("Choose how this chat notifies you.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        CheckBox {
            id: muteBox
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            text: qsTr("Mute notifications")
            checked: root.draftMuted
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: root.draftMuted = checked
            Accessible.name: text
        }

        CheckBox {
            id: soundBox
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            text: qsTr("Play notification sound")
            checked: root.draftSound
            enabled: !root.draftMuted
            opacity: enabled ? 1.0 : 0.5
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: root.draftSound = checked
            Accessible.name: text
        }

        CheckBox {
            id: previewBox
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 8
            text: qsTr("Show message preview")
            checked: root.draftPreview
            enabled: !root.draftMuted
            opacity: enabled ? 1.0 : 0.5
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: root.draftPreview = checked
            Accessible.name: text
        }
    }

    footer: Item {
        implicitHeight: 58

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SecondaryButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            PrimaryButton {
                text: qsTr("Save")
                onClicked: root.save()
            }
        }
    }
}
