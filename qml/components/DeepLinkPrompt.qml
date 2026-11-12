// DeepLinkPrompt — small card that shows what a pasted or clicked Telegram link
// resolved to and lets the user open it or cancel.
//
// Presentation only: the card parses nothing, performs no lookup and owns no
// navigation. The parent hands it a DeepLinksFacade; Open asks the facade to
// confirm(), which emits navigateRequested() for the shell to act on, and
// Cancel clears the facade. A new link re-arms a dismissed card, so the next
// paste is shown again without the parent resetting anything.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: prompt

    // The DeepLinksFacade instance, or null while the shell is still loading.
    property var facade: null

    // True once the user opened the link. Reset whenever a different link
    // arrives, so one card can serve every link in a session.
    property bool dismissed: false

    readonly property bool ready: facade !== null && facade !== undefined
    readonly property bool valid: ready && facade.valid
    readonly property string link: ready ? facade.link : ""
    readonly property string action: ready ? facade.action : "none"
    readonly property string target: ready ? facade.target : ""
    readonly property string messageId: ready ? facade.messageId : ""

    readonly property string title: {
        switch (action) {
        case "openChat":
            return qsTr("Open chat")
        case "joinChat":
            return qsTr("Join chat")
        case "openMessage":
            return qsTr("Open message")
        case "openProfile":
            return qsTr("Open profile")
        case "openBot":
            return qsTr("Open bot")
        case "confirmPhone":
            return qsTr("Confirm phone number")
        case "openSettings":
            return qsTr("Open settings")
        case "openStickerSet":
            return qsTr("Open sticker set")
        default:
            return qsTr("Unsupported link")
        }
    }

    // The target as the user should read it. Usernames are shown with the '@'
    // the link itself omits; invite hashes, phone numbers and numeric ids are
    // shown verbatim because their case and leading '+' are significant.
    readonly property string detail: {
        switch (action) {
        case "openChat":
        case "openBot":
            return "@" + target
        case "openMessage":
            return messageId.length > 0
                    ? "@" + target + " · " + qsTr("post %1").arg(messageId)
                    : "@" + target
        case "openProfile":
            return qsTr("User %1").arg(target)
        case "confirmPhone":
            return target
        case "joinChat":
            return qsTr("Invite %1").arg(target)
        case "openSettings":
            return qsTr("Telegram settings")
        case "openStickerSet":
            return target
        default:
            return ""
        }
    }

    // A rejected link has nothing to open, so the card stays out of the way.
    visible: valid && !dismissed
    onLinkChanged: dismissed = false
    onFacadeChanged: dismissed = false

    implicitWidth: 320
    implicitHeight: layout.implicitHeight + 32
    radius: 16
    color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.94 : 0.98)
    border.width: 1
    border.color: Qt.alpha(BeyGramTheme.line, 0.95)

    Accessible.role: Accessible.Grouping
    Accessible.name: title + ", " + detail

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: prompt.title
            color: BeyGramTheme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }

        Text {
            Layout.fillWidth: true
            text: prompt.detail
            color: BeyGramTheme.muted
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
            visible: text.length > 0
        }

        // What the user actually pasted, so a surprising target can be traced
        // back to the link that produced it.
        Text {
            Layout.fillWidth: true
            text: prompt.link
            color: BeyGramTheme.muted
            font.pixelSize: 11
            elide: Text.ElideMiddle
            maximumLineCount: 1
            wrapMode: Text.NoWrap
            visible: text.length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            SecondaryButton {
                text: qsTr("Cancel")
                onClicked: {
                    prompt.dismissed = true
                    if (prompt.ready)
                        prompt.facade.reset()
                }
            }

            PrimaryButton {
                text: qsTr("Open")
                enabled: prompt.valid
                onClicked: {
                    // confirm() reports the action for the shell; the card only
                    // hides itself and keeps the facade state for the caller.
                    if (prompt.ready && prompt.facade.confirm())
                        prompt.dismissed = true
                }
            }
        }
    }
}
