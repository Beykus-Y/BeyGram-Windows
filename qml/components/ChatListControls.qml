// ChatListControls.qml — sort and archive controls for the chat list.
//
// The parent owns the ChatListFacade and assigns it to `facade`; this component
// only reads its properties and writes the sort settings back. Values are
// re-synced from facade.changed so a change made elsewhere (a shortcut, a
// restored setting, the archive being opened by the page) shows up here without
// binding loops. Nothing here touches Telegram directly.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

RowLayout {
    id: root

    property var facade: null

    // Index i of modeValues matches label i. These are exactly the strings
    // ChatListFacade::setSortMode accepts.
    readonly property var modeValues: ["position", "date", "unread", "name"]
    readonly property var modeLabels: ["Position", "Date", "Unread", "Name"]

    readonly property int archivedCount: root.facade ? Number(root.facade.archivedCount) : 0
    readonly property bool archiveOpen: root.facade ? Boolean(root.facade.archiveOpen) : false

    spacing: 8

    // Pulls the combo and the two toggles back to the facade's state. The
    // guards keep a user interaction from being overwritten and stop the
    // sync from feeding itself.
    function syncFromFacade() {
        if (!root.facade)
            return

        const index = root.modeValues.indexOf(String(root.facade.sortMode))
        if (index >= 0 && index !== sortCombo.currentIndex)
            sortCombo.currentIndex = index

        const ascending = Boolean(root.facade.ascending)
        if (ascendingToggle.checked !== ascending)
            ascendingToggle.checked = ascending

        const pinnedFirst = Boolean(root.facade.pinnedFirst)
        if (pinnedToggle.checked !== pinnedFirst)
            pinnedToggle.checked = pinnedFirst
    }

    Component.onCompleted: syncFromFacade()
    onFacadeChanged: syncFromFacade()

    Connections {
        target: root.facade
        function onChanged() { root.syncFromFacade() }
    }

    ComboBox {
        id: sortCombo

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredWidth: 132
        enabled: root.facade !== null
        model: root.modeLabels

        palette.text: BeyGramTheme.text
        palette.windowText: BeyGramTheme.text
        palette.button: BeyGramTheme.surface2
        palette.buttonText: BeyGramTheme.text
        palette.base: BeyGramTheme.surface2
        palette.highlight: BeyGramTheme.accentSoft
        palette.highlightedText: BeyGramTheme.accentInk

        Accessible.role: Accessible.ComboBox
        Accessible.name: "Chat list order"
        Accessible.description: "Sorts the chat list by " + currentText.toLowerCase()

        onActivated: function(index) {
            if (root.facade && index >= 0 && index < root.modeValues.length)
                root.facade.sortMode = root.modeValues[index]
        }
    }

    CheckBox {
        id: ascendingToggle

        Layout.alignment: Qt.AlignVCenter
        text: "Ascending"
        enabled: root.facade !== null

        palette.text: BeyGramTheme.text
        palette.windowText: BeyGramTheme.text

        Accessible.name: "Ascending order"
        Accessible.description: "Reverses the selected chat list order"

        onToggled: if (root.facade) root.facade.ascending = checked
    }

    CheckBox {
        id: pinnedToggle

        Layout.alignment: Qt.AlignVCenter
        text: "Pinned first"
        enabled: root.facade !== null

        palette.text: BeyGramTheme.text
        palette.windowText: BeyGramTheme.text

        Accessible.name: "Pinned first"
        Accessible.description: "Keeps pinned chats above the rest"

        onToggled: if (root.facade) root.facade.pinnedFirst = checked
    }

    // The archive chip doubles as a toggle: it shows how many chats the archive
    // holds and opens or closes the folder when clicked.
    Button {
        id: archiveChip

        Layout.alignment: Qt.AlignVCenter
        implicitHeight: 32
        leftPadding: 13
        rightPadding: 13
        enabled: root.facade !== null
        hoverEnabled: true
        checkable: false

        Accessible.role: Accessible.Button
        Accessible.name: root.archivedCount > 0
                         ? "Archive, " + root.archivedCount + " chats"
                         : "Archive"
        Accessible.checked: root.archiveOpen
        Accessible.description: root.archiveOpen ? "Returns to the chat list"
                                                 : "Opens the archived chats"

        onClicked: {
            if (!root.facade)
                return
            if (root.archiveOpen)
                root.facade.closeArchive()
            else
                root.facade.openArchive()
        }

        contentItem: Item {
            implicitWidth: chipRow.implicitWidth
            implicitHeight: chipRow.implicitHeight

            Row {
                id: chipRow

                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "Archive"
                    color: root.archiveOpen ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: 12
                    font.weight: Font.DemiBold

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Rectangle {
                    visible: root.archivedCount > 0
                    width: Math.max(16, countLabel.implicitWidth + 8)
                    height: 16
                    radius: 8
                    color: root.archiveOpen ? BeyGramTheme.accentInk : BeyGramTheme.accent

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        id: countLabel

                        anchors.centerIn: parent
                        text: root.archivedCount > 99 ? "99+" : root.archivedCount
                        color: root.archiveOpen ? BeyGramTheme.accentSoft : BeyGramTheme.onAccent
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

        background: Rectangle {
            radius: 16
            color: root.archiveOpen
                   ? BeyGramTheme.accentSoft
                   : (archiveChip.hovered ? BeyGramTheme.surface2 : "transparent")
            border.width: 1
            border.color: root.archiveOpen ? "transparent" : BeyGramTheme.line

            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    Item { Layout.fillWidth: true }
}
