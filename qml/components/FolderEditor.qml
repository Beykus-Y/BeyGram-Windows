// FolderEditor - modal card for creating, renaming and deleting a chat folder.
//
// The parent owns the FoldersFacade and opens this card (typically after
// beginCreate()/beginEdit() set `editing`). The facade holds the draft, decides
// whether Save is possible and emits createRequested/renameRequested/
// deleteRequested for the transport; this file only forwards the user's
// actions and performs no network, logging or C++ calls of its own.
//
// The name field is deliberately not two-way bound to `facade.draftTitle`:
// the facade normalises and caps the draft, so syncing on every keystroke would
// delete the space the user just typed. It is filled on open and whenever the
// facade changes while the field is unfocused.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: root

    property var facade: null

    // Set before Save/Cancel/Delete closes the dialog, so the close that
    // follows does not cancel the edit a second time.
    property bool actionTaken: false

    readonly property bool ready: root.facade !== null
    readonly property bool canDelete: {
        if (!root.ready || root.facade.currentIndex < 0)
            return false
        const ids = root.facade.folderIds
        if (!ids || root.facade.currentIndex >= ids.length)
            return false
        // The main chat list is numbered 0 and is not a deletable folder.
        return String(ids[root.facade.currentIndex]) !== "0"
    }

    function syncFromFacade() {
        if (!root.ready)
            return
        if (!titleField.activeFocus && titleField.text !== root.facade.draftTitle)
            titleField.text = root.facade.draftTitle
        archiveBox.checked = root.facade.archiveAndMuteNewChats
    }

    function attemptSave() {
        if (!root.ready || !root.facade.canSave)
            return
        root.actionTaken = true
        if (root.facade.save()) {
            // The card closes on success, so end the edit session with it.
            // FoldersFacade::save() itself keeps the edit open (marked saved);
            // that behaviour is for callers that stay on the field.
            root.facade.cancelEdit()
            root.close()
        } else {
            root.actionTaken = false
        }
    }

    function attemptCancel() {
        root.actionTaken = true
        if (root.ready)
            root.facade.cancelEdit()
        root.close()
    }

    function attemptDelete() {
        if (!root.canDelete)
            return
        root.actionTaken = true
        root.facade.requestDelete(root.facade.currentIndex)
        root.close()
    }

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 400
    padding: 0
    modal: true
    focus: true
    title: "Chat folder"
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        root.actionTaken = false
        root.syncFromFacade()
        titleField.forceInputFocus()
    }

    // Closing without choosing an action (Escape) leaves no edit behind.
    onClosed: {
        if (!root.actionTaken && root.ready && root.facade.editing)
            root.facade.cancelEdit()
    }

    onFacadeChanged: root.syncFromFacade()

    Connections {
        target: root.facade

        function onChanged() {
            root.syncFromFacade()
        }
    }

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
        spacing: 12
        implicitWidth: 400

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 2
            text: "Folders group your chats. The name is shown above the chat list."
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        AuthField {
            id: titleField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: "Name"
            placeholderText: "Folder name"
            hint: "Up to 12 characters."
            // The facade counts code points and caps at 12; the field allows
            // more so an emoji is not counted twice while typing.
            maximumLength: 64
            onEdited: if (root.ready) root.facade.setDraftTitle(titleField.text)
            onAccepted: root.attemptSave()
        }

        CheckBox {
            id: archiveBox
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            text: "Archive and mute new chats"
            enabled: root.ready
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: if (root.ready) root.facade.archiveAndMuteNewChats = checked
            Accessible.name: text
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
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
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter

            SecondaryButton {
                text: "Delete"
                visible: root.canDelete
                onClicked: root.attemptDelete()
                Accessible.description: "Delete this folder"
            }
        }

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SecondaryButton {
                text: "Cancel"
                onClicked: root.attemptCancel()
            }

            PrimaryButton {
                text: "Save"
                enabled: root.ready && root.facade.canSave
                onClicked: root.attemptSave()
            }
        }
    }
}
