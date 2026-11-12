// ProfileEditDialog - modal editor for the signed-in user's own profile.
//
// The parent owns the data layer: it assigns firstName/lastName/username/bio
// from the current profile, toggles `saving` while the request is in flight,
// and reports failures through `errorText`. When the user confirms, this file
// only emits `saveRequested` with trimmed values; it performs no network,
// logging or C++ calls of its own. The dialog stays open until the parent
// closes it (typically on success), so an error can be shown in place.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

DsDialog {
    id: root

    property string firstName: ""
    property string lastName: ""
    property string username: ""
    property string bio: ""
    property bool saving: false
    property string errorText: ""

    signal saveRequested(string firstName, string lastName, string username, string bio)

    readonly property string trimmedFirstName: firstField.text.trim()

    function resetFields() {
        firstField.text = root.firstName
        lastField.text = root.lastName
        usernameField.text = root.username
        bioField.text = root.bio
    }

    function attemptSave() {
        if (root.saving)
            return
        if (root.trimmedFirstName.length === 0)
            return
        root.saveRequested(root.trimmedFirstName,
                           lastField.text.trim(),
                           usernameField.text.trim(),
                           bioField.text.trim())
    }

    title: "Edit profile"
    description: "Update how others see you. The first name is required."
    busy: root.saving
    // A form should not close because of a stray click on the scrim; Escape
    // still dismisses it when no request is in flight.
    closePolicy: root.busy ? Popup.NoAutoClose : Popup.CloseOnEscape

    onOpened: {
        root.resetFields()
        firstField.forceActiveFocus()
    }

    DsTextField {
        id: firstField
        Layout.fillWidth: true
        label: "First name"
        placeholderText: "Required"
        maximumLength: 64
        clearable: true
        onAccepted: root.attemptSave()
    }

    DsTextField {
        id: lastField
        Layout.fillWidth: true
        label: "Last name"
        placeholderText: "Optional"
        maximumLength: 64
        clearable: true
        onAccepted: root.attemptSave()
    }

    DsTextField {
        id: usernameField
        Layout.fillWidth: true
        label: "Username"
        placeholderText: "Optional"
        maximumLength: 32
        clearable: true
        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
        onAccepted: root.attemptSave()
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: -Design.space1
        text: "Public @handle. Leave empty to remove it."
        color: BeyGramTheme.muted
        font.pixelSize: Design.fontCaption
        wrapMode: Text.WordWrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Design.space1

        Text {
            Layout.fillWidth: true
            text: "Bio"
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
        }

        TextArea {
            id: bioField
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            leftPadding: Design.space3
            rightPadding: Design.space3
            topPadding: Design.space2
            bottomPadding: Design.space2
            color: BeyGramTheme.text
            placeholderText: "A few words about you (optional)"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            font.pixelSize: Design.fontBody
            wrapMode: TextArea.Wrap
            selectByMouse: true

            background: Rectangle {
                radius: Design.radiusSm
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: bioField.activeFocus
                              ? BeyGramTheme.accent
                              : BeyGramTheme.line
                Behavior on border.color { ColorAnimation { duration: Design.motionFast } }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: root.errorText.length > 0
        implicitHeight: errorLabel.implicitHeight + Design.space3 * 2
        radius: Design.radiusSm
        color: Qt.alpha(BeyGramTheme.danger, 0.12)
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.danger, 0.35)

        Text {
            id: errorLabel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Design.space3
            text: root.errorText
            color: BeyGramTheme.danger
            font.pixelSize: Design.fontSmall
            wrapMode: Text.WordWrap
            lineHeight: Design.lineNormal
        }
    }

    // A custom footer keeps Save disabled until a first name is present and
    // keeps the dialog open after the request, so `errorText` can be shown
    // without losing the user's edits. DsDialog's shared footer always closes
    // on accept and cannot express that.
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
                running: root.saving
                visible: root.saving
                implicitWidth: 18
                implicitHeight: 18
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
            }

            DsButton {
                text: "Cancel"
                tone: "secondary"
                enabled: !root.saving
                onClicked: root.close()
            }

            DsButton {
                text: root.saving ? "Saving…" : "Save"
                enabled: !root.saving && root.trimmedFirstName.length > 0
                onClicked: root.attemptSave()
            }
        }
    }
}
