// ForumTopicsPanel - the topic list of a forum supergroup.
//
// The parent owns the data layer: it assigns a ForumTopicsFacade to `facade`
// and maps topicActivated()/pinRequested() onto the requests in
// telegram/ForumTopics.h. The panel only renders the rows and writes the
// user's intent back into the facade; it performs no network, logging or C++
// calls of its own. The facade has no create method, so the create dialog
// collects a valid name and asks the parent with createTopicRequested(title);
// the parent should answer by calling setTopics() with the refreshed list.
//
// Layout comes from the design system: one DsSection, one DsCard, DsListRow per
// topic (with its unread badge and a pin DsIconButton), DsEmptyState when the
// forum has no topics, and DsDialog with DsTextField for creation.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: panel

    property var facade: null

    /// The user confirmed a topic name. The facade has no create method, so the
    /// parent owns createForumTopicRequest() and the following refresh.
    signal createTopicRequested(string title)

    readonly property bool hasFacade: panel.facade !== null
    readonly property var topicIds: panel.hasFacade ? panel.facade.ids : []
    readonly property var topicTitles: panel.hasFacade ? panel.facade.titles : []
    readonly property var topicUnread: panel.hasFacade ? panel.facade.unreadCounts : []
    readonly property var topicPinned: panel.hasFacade ? panel.facade.pinnedFlags : []
    readonly property int topicCount: panel.hasFacade ? panel.facade.count : 0
    readonly property bool hasTopics: panel.topicCount > 0

    // Matches the 1..128 code point rule in telegram/ForumTopics.h.
    readonly property int titleLimit: 128

    implicitWidth: 520
    implicitHeight: content.implicitHeight

    function topicIdAt(index) {
        const values = panel.topicIds
        if (!values || index < 0 || index >= values.length)
            return ""
        const id = values[index]
        return id === undefined || id === null ? "" : String(id)
    }

    function topicTitle(index) {
        const values = panel.topicTitles
        if (!values || index < 0 || index >= values.length)
            return ""
        const title = values[index]
        return title === undefined || title === null ? "" : String(title)
    }

    function topicUnreadCount(index) {
        const values = panel.topicUnread
        if (!values || index < 0 || index >= values.length)
            return 0
        const count = Number(values[index])
        return isNaN(count) ? 0 : Math.max(0, Math.floor(count))
    }

    function topicIsPinned(index) {
        const values = panel.topicPinned
        if (!values || index < 0 || index >= values.length)
            return false
        // QVariantList of bools reaches QML as booleans; 1 covers a numeric
        // spelling from a future facade change.
        return values[index] === true || values[index] === 1
    }

    function openTopic(topicId) {
        if (!panel.hasFacade || topicId === undefined || topicId === null)
            return
        panel.facade.openTopic(String(topicId))
    }

    function togglePinned(topicId) {
        if (!panel.hasFacade || topicId === undefined || topicId === null)
            return
        panel.facade.togglePinned(String(topicId))
    }

    /// Counts Unicode code points, the unit createForumTopicRequest() uses.
    /// String.length would count UTF-16 units and reject valid emoji titles.
    function codePointLength(value) {
        return Array.from(String(value)).length
    }

    function trimmedTitle() {
        return createField.text.trim()
    }

    function canCreate() {
        const title = panel.trimmedTitle()
        return title.length > 0 && panel.codePointLength(title) <= panel.titleLimit
    }

    function titleError() {
        const title = panel.trimmedTitle()
        if (title.length === 0)
            return qsTr("Enter a topic name.")
        if (panel.codePointLength(title) > panel.titleLimit)
            return qsTr("Use at most %1 characters.").arg(panel.titleLimit)
        return ""
    }

    function requestCreate() {
        if (!panel.hasFacade)
            return
        // Clearing the text also clears the bound error line; assigning to
        // `error` directly would break that binding.
        createField.text = ""
        createDialog.open()
    }

    function submitCreate() {
        if (!panel.hasFacade || !panel.canCreate())
            return
        const title = panel.trimmedTitle()
        createDialog.close()
        panel.createTopicRequested(title)
    }

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Topics")
            description: qsTr("Conversations inside this forum. Pin the ones you follow most closely.")

            trailing: DsButton {
                text: qsTr("New topic")
                icon: "\uE710" // Segoe MDL2 Assets: Add
                size: "sm"
                enabled: panel.hasFacade
                accessibleName: qsTr("Create a topic")
                onClicked: panel.requestCreate()
            }

            content: DsCard {
                Layout.fillWidth: true
                padding: Design.space1

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsEmptyState {
                        Layout.fillWidth: true
                        Layout.topMargin: Design.space5
                        Layout.bottomMargin: Design.space5
                        visible: !panel.hasTopics
                        glyph: "\uD83D\uDCAC" // 💬 speech balloon
                        title: qsTr("No topics yet")
                        description: qsTr("Topics keep long conversations apart. Create the first one to get started.")
                        actionText: panel.hasFacade ? qsTr("Create a topic") : ""
                        onActionRequested: panel.requestCreate()
                    }

                    Repeater {
                        // The count, not the id array, so a pin toggle updates
                        // the existing delegates instead of rebuilding the list.
                        model: panel.topicCount

                        delegate: ColumnLayout {
                            id: topicRow

                            required property int index

                            Layout.fillWidth: true
                            spacing: 0

                            readonly property string topicId: panel.topicIdAt(topicRow.index)
                            readonly property bool pinned: panel.topicIsPinned(topicRow.index)

                            DsListRow {
                                Layout.fillWidth: true
                                title: panel.topicTitle(topicRow.index)
                                unread: panel.topicUnreadCount(topicRow.index)
                                onClicked: panel.openTopic(topicRow.topicId)

                                trailing: DsIconButton {
                                    icon: "\uE718" // Segoe MDL2 Assets: Pin
                                    size: "sm"
                                    tone: topicRow.pinned ? "accent" : "plain"
                                    selected: topicRow.pinned
                                    tooltip: topicRow.pinned
                                             ? qsTr("Unpin topic")
                                             : qsTr("Pin topic")
                                    onClicked: panel.togglePinned(topicRow.topicId)
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: topicRow.index < panel.topicCount - 1
                            }
                        }
                    }
                }
            }
        }
    }

    DsDialog {
        id: createDialog

        title: qsTr("New topic")
        description: qsTr("Give the topic a name. You can rename it later.")

        onOpened: createField.forceActiveFocus()

        DsTextField {
            id: createField

            Layout.fillWidth: true
            label: qsTr("Name")
            placeholderText: qsTr("Topic name")
            Accessible.name: qsTr("Topic name")
            Accessible.description: createField.error
            error: createField.text.length > 0 ? panel.titleError() : ""
            onAccepted: panel.submitCreate()
        }

        // DsDialog's default footer only confirms; creation needs a button that
        // stays disabled until the name is valid, so the footer is replaced.
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
                    onClicked: createDialog.close()
                }

                DsButton {
                    text: qsTr("Create")
                    enabled: panel.hasFacade && panel.canCreate()
                    onClicked: panel.submitCreate()
                }
            }
        }
    }
}
