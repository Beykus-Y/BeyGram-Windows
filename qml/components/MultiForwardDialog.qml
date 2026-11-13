pragma ComponentBehavior: Bound

// MultiForwardDialog - modal picker for forwarding one message set to several
// chats. Presentation only: a MultiForwardFacade owns the session, the search
// text and the selection, and the page connects forwardRequested().
//
// The page calls facade.begin()/setChats() before opening the dialog and may
// set messageCount for the title. Cancel only closes; the page owns the facade
// and starts the next session itself.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

DsDialog {
    id: root

    /// The MultiForwardFacade instance owned by the page.
    property var facade: null
    /// Optional message count used by the title; 0 keeps the generic title.
    property int messageCount: 0

    readonly property bool ready: root.facade !== null
    readonly property int maxTargets: 20
    readonly property int selectedCount: root.ready ? root.facade.selectedIds.length : 0
    readonly property int chatCount: root.ready ? root.facade.chatTitles.length : 0
    readonly property bool filtering: root.ready && root.facade.filter.length > 0

    readonly property string dialogTitle: {
        if (root.messageCount > 1)
            return qsTr("Forward %n message(s)", "", root.messageCount)
        if (root.messageCount === 1)
            return qsTr("Forward message")
        return qsTr("Forward messages")
    }

    function isSelected(chatId) {
        if (!root.ready)
            return false
        const ids = root.facade.selectedIds
        for (let i = 0; i < ids.length; ++i) {
            if (ids[i] === chatId)
                return true
        }
        return false
    }

    function initialsFor(title) {
        const text = (title === undefined || title === null) ? "" : String(title).trim()
        if (text.length === 0)
            return "?"
        const words = text.split(/\s+/)
        if (words.length === 1)
            return words[0].substring(0, 2).toUpperCase()
        return (words[0].charAt(0) + words[1].charAt(0)).toUpperCase()
    }

    function toggleAt(index) {
        if (!root.ready)
            return
        const chatId = root.facade.chatIdAt(index)
        if (chatId.length > 0)
            root.facade.toggleTarget(chatId)
    }

    function attemptForward() {
        if (!root.ready || !root.facade.canForward)
            return
        if (root.facade.confirm())
            root.close()
    }

    title: root.dialogTitle
    description: qsTr("Choose one or more chats to forward to.")
    width: 480

    onOpened: {
        searchField.text = root.ready ? root.facade.filter : ""
        searchField.forceActiveFocus()
        chatList.positionViewAtBeginning()
    }

    Connections {
        target: root.facade
        enabled: root.ready

        function onChanged() {
            // The facade is the single source of truth for the query; the field
            // only writes to it, so a programmatic reset also reaches the text.
            if (searchField.text !== root.facade.filter)
                searchField.text = root.facade.filter
        }
    }

    DsSearchField {
        id: searchField

        Layout.fillWidth: true
        placeholderText: qsTr("Search chats...")
        Accessible.name: qsTr("Search chats")

        onTextChanged: {
            if (root.ready && root.facade.filter !== searchField.text)
                root.facade.setFilter(searchField.text)
        }

        onAccepted: chatList.forceActiveFocus()

        Keys.onDownPressed: (event) => {
            chatList.forceActiveFocus()
            event.accepted = true
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 300

        ListView {
            id: chatList

            anchors.fill: parent
            clip: true
            model: root.ready ? root.facade.chatTitles : []
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Chats")

            delegate: DsListRow {
                id: chatRow

                required property string modelData
                required property int index

                readonly property string chatId: root.ready
                                                      ? root.facade.chatIdAt(index)
                                                      : ""
                readonly property bool picked: root.isSelected(chatId)

                width: chatList.width
                title: chatRow.modelData
                selected: chatRow.picked
                activeFocusOnTab: true

                Accessible.role: Accessible.CheckBox
                Accessible.name: chatRow.modelData
                Accessible.checked: chatRow.picked
                Accessible.onPressAction: root.toggleAt(chatRow.index)

                Keys.onSpacePressed: root.toggleAt(chatRow.index)
                Keys.onReturnPressed: root.toggleAt(chatRow.index)
                Keys.onEnterPressed: root.toggleAt(chatRow.index)

                leading: DsAvatar {
                    size: Design.avatarSmall
                    initials: root.initialsFor(chatRow.modelData)
                    accentIndex: chatRow.index
                }

                trailing: Item {
                    width: 22
                    height: 22

                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: chatRow.picked ? BeyGramTheme.accent : "transparent"
                        border.width: chatRow.picked ? 0 : 1.5
                        border.color: chatRow.picked ? BeyGramTheme.accent
                                                     : BeyGramTheme.line
                        antialiasing: true

                        Behavior on color {
                            ColorAnimation {
                                duration: Design.motion(Design.motionFast,
                                                        BeyGramAppearance.reduceMotion)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: chatRow.picked
                        text: "\u2713"
                        color: BeyGramTheme.onAccent
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                onClicked: {
                    chatRow.forceActiveFocus()
                    root.toggleAt(chatRow.index)
                }
            }
        }

        DsEmptyState {
            anchors.centerIn: parent
            width: Math.min(parent.width - Design.space8, implicitWidth)
            visible: root.chatCount === 0
            title: root.filtering ? qsTr("No matches") : qsTr("No chats available")
            description: root.filtering
                         ? qsTr("Try a different name or clear the search.")
                         : qsTr("Chats you can forward to will appear here.")
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Design.space2

        Text {
            Layout.fillWidth: true
            text: qsTr("%1 of %2 selected").arg(root.selectedCount).arg(root.maxTargets)
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            elide: Text.ElideRight
        }

        Text {
            visible: root.selectedCount >= root.maxTargets
            text: qsTr("Limit reached")
            color: BeyGramTheme.warnInk
            font.pixelSize: Design.fontCaption
            font.weight: Font.Medium
        }
    }

    Text {
        Layout.fillWidth: true
        visible: root.ready && root.facade.error.length > 0
        text: root.ready ? root.facade.error : ""
        color: BeyGramTheme.danger
        font.pixelSize: Design.fontSmall
        wrapMode: Text.WordWrap
        lineHeight: Design.lineNormal
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

            DsButton {
                text: qsTr("Cancel")
                tone: "secondary"
                onClicked: root.close()
            }

            DsButton {
                text: qsTr("Forward")
                enabled: root.ready && root.facade.canForward
                onClicked: root.attemptForward()
            }
        }
    }
}
