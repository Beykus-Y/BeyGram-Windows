// ProfileEditDialog - modal editor for the signed-in user's own profile.
//
// The parent owns the data layer: it assigns firstName/lastName/username/bio
// from the current profile, toggles `saving` while the request is in flight,
// and reports failures through `errorText`. When the user confirms, this file
// only emits `saveRequested` with trimmed values; it performs no network,
// logging or C++ calls of its own. The dialog stays open until the parent
// closes it (typically on success).

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
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

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 420
    padding: 0
    modal: true
    focus: true
    title: "Edit profile"
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        resetFields()
        firstField.forceInputFocus()
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
        implicitWidth: 420

        AuthField {
            id: firstField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 2
            label: "First name"
            placeholderText: "First name"
            maximumLength: 64
            onAccepted: root.attemptSave()
        }

        AuthField {
            id: lastField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: "Last name"
            placeholderText: "Last name (optional)"
            maximumLength: 64
            onAccepted: root.attemptSave()
        }

        AuthField {
            id: usernameField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: "Username"
            placeholderText: "username (optional)"
            hint: "Public @handle. Leave empty to remove it."
            maximumLength: 32
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            onAccepted: root.attemptSave()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            spacing: 6

            Text {
                text: "Bio"
                color: BeyGramTheme.muted
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            TextArea {
                id: bioField
                Layout.fillWidth: true
                Layout.preferredHeight: 88
                leftPadding: 14
                rightPadding: 14
                topPadding: 10
                bottomPadding: 10
                color: BeyGramTheme.text
                placeholderText: "A few words about you (optional)"
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: 14
                wrapMode: TextArea.Wrap
                selectByMouse: true

                background: Rectangle {
                    radius: 10
                    color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                    border.width: bioField.activeFocus ? 2 : 1
                    border.color: bioField.activeFocus
                                  ? BeyGramTheme.accent
                                  : Qt.alpha(BeyGramTheme.line, 1.0)
                    Behavior on border.color { ColorAnimation { duration: 120 } }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            visible: root.errorText !== ""
            implicitHeight: errorLabel.implicitHeight + 20
            radius: 10
            color: Qt.alpha(BeyGramTheme.danger, 0.12)
            border.width: 1
            border.color: Qt.alpha(BeyGramTheme.danger, 0.35)

            Text {
                id: errorLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 10
                text: root.errorText
                color: BeyGramTheme.danger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                lineHeight: 1.3
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 6
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
                text: "Cancel"
                onClicked: root.close()
            }

            PrimaryButton {
                text: root.saving ? "Saving..." : "Save"
                enabled: !root.saving && root.trimmedFirstName.length > 0
                onClicked: root.attemptSave()
            }
        }
    }
}
