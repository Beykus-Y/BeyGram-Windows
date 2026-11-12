pragma ComponentBehavior: Bound

// ChatListControls.qml — sort and archive controls for the chat list.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

RowLayout {
    id: root

    property var facade: null

    readonly property var modeValues: ["position", "date", "unread", "name"]
    readonly property var modeLabels: [qsTr("Position"), qsTr("Date"), qsTr("Unread"), qsTr("Name")]

    readonly property int archivedCount: root.facade ? Number(root.facade.archivedCount) : 0
    readonly property bool archiveOpen: root.facade ? Boolean(root.facade.archiveOpen) : false
    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)

    spacing: 8

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
        palette.window: BeyGramTheme.surface
        palette.highlight: BeyGramTheme.accentSoft
        palette.highlightedText: BeyGramTheme.accentInk

        Accessible.role: Accessible.ComboBox
        Accessible.name: qsTr("Chat list order")
        Accessible.description: qsTr("Sorts the chat list by %1").arg(currentText.toLowerCase())

        onActivated: function(index) {
            if (root.facade && index >= 0 && index < root.modeValues.length)
                root.facade.sortMode = root.modeValues[index]
        }
    }

    CheckBox {
        id: ascendingToggle

        Layout.alignment: Qt.AlignVCenter
        text: qsTr("Ascending")
        enabled: root.facade !== null

        palette.text: BeyGramTheme.text
        palette.windowText: BeyGramTheme.text
        palette.buttonText: BeyGramTheme.text

        Accessible.name: qsTr("Ascending order")
        Accessible.description: qsTr("Reverses the selected chat list order")

        onToggled: if (root.facade) root.facade.ascending = checked
    }

    CheckBox {
        id: pinnedToggle

        Layout.alignment: Qt.AlignVCenter
        text: qsTr("Pinned first")
        enabled: root.facade !== null

        palette.text: BeyGramTheme.text
        palette.windowText: BeyGramTheme.text
        palette.buttonText: BeyGramTheme.text

        Accessible.name: qsTr("Pinned first")
        Accessible.description: qsTr("Keeps pinned chats above the rest")

        onToggled: if (root.facade) root.facade.pinnedFirst = checked
    }

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
                         ? qsTr("Archive, %n chat(s)", "", root.archivedCount)
                         : qsTr("Archive")
        Accessible.checked: root.archiveOpen
        Accessible.description: root.archiveOpen
                                ? qsTr("Returns to the chat list")
                                : qsTr("Opens the archived chats")

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
                    text: qsTr("Archive")
                    color: root.archiveOpen ? BeyGramTheme.accentInk : BeyGramTheme.muted
                    font.pixelSize: 12
                    font.weight: Font.DemiBold

                    Behavior on color { ColorAnimation { duration: root._animFast } }
                }

                Rectangle {
                    visible: root.archivedCount > 0
                    width: Math.max(16, countLabel.implicitWidth + 8)
                    height: 16
                    radius: 8
                    color: root.archiveOpen ? BeyGramTheme.accentInk : BeyGramTheme.accent

                    Behavior on color { ColorAnimation { duration: root._animFast } }

                    Text {
                        id: countLabel

                        anchors.centerIn: parent
                        text: root.archivedCount > 99 ? "99+" : String(root.archivedCount)
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

            Behavior on color { ColorAnimation { duration: root._animFast } }
        }
    }

    Item { Layout.fillWidth: true }
}
