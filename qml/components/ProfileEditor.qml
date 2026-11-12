// ProfileEditor - inline editor for the signed-in user's own profile.
//
// The parent owns the data layer: it assigns `facade` (a ProfileFacade) and
// connects facade.saveRequested and facade.usernameCheckRequested to the
// transport, then calls facade.load() with the profile the server reported.
// This component only edits the facade's properties; it performs no network,
// logging or C++ calls of its own, and it never decides whether a value is
// valid - that answer comes back through usernameStatus and canSave.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Card {
    id: root

    property var facade: null

    implicitWidth: 420

    // The username line is worded here, not in the facade: usernameStatus is
    // the machine-readable code ("idle", "invalid", "checking", "available",
    // "taken") and the view is what turns it into a sentence.
    readonly property string usernameMessage: {
        if (!root.facade)
            return ""
        switch (root.facade.usernameStatus) {
        case "invalid":
            return "5-32 characters: letters, digits and underscores, starting with a letter."
        case "checking":
            return "Checking availability..."
        case "available":
            return "Username is available."
        case "taken":
            return "Username is already taken."
        default:
            return ""
        }
    }

    readonly property color usernameColor: {
        if (!root.facade)
            return BeyGramTheme.muted
        switch (root.facade.usernameStatus) {
        case "invalid":
        case "taken":
            return BeyGramTheme.danger
        case "available":
            return BeyGramTheme.ok
        default:
            return BeyGramTheme.muted
        }
    }

    readonly property bool presenceVisible: root.facade !== null
                                            && root.facade.statusText !== ""

    readonly property bool presenceOnline: root.facade !== null
                                           && root.facade.statusText === "online"

    function syncFromFacade() {
        if (!root.facade)
            return
        if (firstField.text !== root.facade.firstName)
            firstField.text = root.facade.firstName
        if (lastField.text !== root.facade.lastName)
            lastField.text = root.facade.lastName
        if (usernameField.text !== root.facade.username)
            usernameField.text = root.facade.username
        if (bioField.text !== root.facade.bio)
            bioField.text = root.facade.bio
    }

    function save() {
        if (root.facade)
            root.facade.save()
    }

    function checkUsername() {
        if (root.facade)
            root.facade.checkUsername()
    }

    Component.onCompleted: syncFromFacade()

    // facade.load() (or any other facade-side change) rewrites the fields.
    // While the user types, the field already holds the value the facade
    // stored, so the guard above makes this a no-op and typing is not
    // interrupted.
    Connections {
        target: root.facade
        function onChanged() {
            root.syncFromFacade()
        }
    }

    Text {
        Layout.fillWidth: true
        text: "Edit profile"
        color: BeyGramTheme.text
        font.pixelSize: 15
        font.weight: Font.DemiBold
    }

    AuthField {
        id: firstField
        Layout.fillWidth: true
        label: "First name"
        placeholderText: "First name"
        maximumLength: 64
        onEdited: {
            if (root.facade)
                root.facade.firstName = firstField.text
        }
        onAccepted: root.save()
    }

    AuthField {
        id: lastField
        Layout.fillWidth: true
        label: "Last name"
        placeholderText: "Last name (optional)"
        maximumLength: 64
        onEdited: {
            if (root.facade)
                root.facade.lastName = lastField.text
        }
        onAccepted: root.save()
    }

    AuthField {
        id: usernameField
        Layout.fillWidth: true
        label: "Username"
        placeholderText: "username (optional)"
        hint: "Public @handle. Leave empty to remove it."
        maximumLength: 32
        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
        onEdited: {
            if (root.facade)
                root.facade.username = usernameField.text
        }
        onAccepted: root.checkUsername()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            visible: root.usernameMessage !== ""
            text: root.usernameMessage
            color: root.usernameColor
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            lineHeight: 1.25
        }

        SecondaryButton {
            text: "Check"
            visible: root.facade !== null && root.facade.username !== ""
                     && (root.facade.usernameStatus === "idle"
                         || root.facade.usernameStatus === "taken")
            onClicked: root.checkUsername()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
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
            onTextChanged: {
                if (root.facade && bioField.text !== root.facade.bio)
                    root.facade.bio = bioField.text
            }

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

    RowLayout {
        Layout.fillWidth: true
        visible: root.presenceVisible
        spacing: 6

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            width: 6
            height: 6
            radius: 3
            color: root.presenceOnline ? BeyGramTheme.ok : BeyGramTheme.muted
        }

        Text {
            Layout.fillWidth: true
            text: root.facade ? root.facade.statusText : ""
            color: BeyGramTheme.muted
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: root.facade !== null && root.facade.dirty ? "Unsaved changes" : ""
            color: BeyGramTheme.muted
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        PrimaryButton {
            text: "Save"
            enabled: root.facade !== null && root.facade.canSave
            onClicked: root.save()
        }
    }
}
