// Compact reaction picker popup.
//
// Presentation only. `facade` is a beygram::ui::ReactionsFacade, which owns the
// catalog, the selection and the target message; this popup reads facade.top,
// facade.recent, facade.selected and facade.expanded, and forwards clicks to
// facade.toggle(), facade.setExpanded() and facade.clearSelection(). The facade
// emits reactionRequested(chatId, messageId, emoji) for the owner to turn into a
// TDLib addMessageReaction request (an empty emoji removes the reaction).
//
// Nothing in this file touches TDLib, the network, the message store, or
// logging. A null facade leaves the popup empty and inert.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Popup {
    id: picker

    /// beygram::ui::ReactionsFacade instance. May be null while the owner wires
    /// the popup up; every binding below tolerates that.
    property var facade: null

    readonly property bool ready: picker.facade !== null && picker.facade !== undefined
    /// Emoji offered as the primary row.
    readonly property var topEmoji: picker.ready ? picker.facade.top : []
    /// Recently used emoji, revealed by the more toggle.
    readonly property var recentEmoji: picker.ready ? picker.facade.recent : []
    /// The emoji the user has currently chosen, or an empty string.
    readonly property string selectedEmoji: picker.ready ? picker.facade.selected : ""
    /// Whether the recent row is shown.
    readonly property bool showMore: picker.ready ? picker.facade.expanded : false

    /// Edge length of one emoji button.
    readonly property int cellSize: 34
    /// Emoji per row in the primary grid.
    readonly property int columns: 6

    padding: 8
    modal: false
    dim: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    implicitWidth: Math.max(topGrid.implicitWidth, recentRow.implicitWidth,
                            footer.implicitWidth) + leftPadding + rightPadding
    implicitHeight: body.implicitHeight + topPadding + bottomPadding

    // A dismissed popup must not leave a highlight behind for the next message.
    onClosed: if (picker.facade) picker.facade.clearSelection()

    background: Rectangle {
        radius: 12
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
    }

    contentItem: ColumnLayout {
        id: body
        spacing: 4

        GridLayout {
            id: topGrid
            columns: picker.columns
            rowSpacing: 2
            columnSpacing: 2
            Layout.alignment: Qt.AlignHCenter

            Repeater {
                model: picker.topEmoji

                delegate: Rectangle {
                    id: topCell
                    property string emoji: modelData

                    Layout.preferredWidth: picker.cellSize
                    Layout.preferredHeight: picker.cellSize
                    radius: 10
                    color: picker.selectedEmoji === topCell.emoji
                           ? BeyGramTheme.accentSoft
                           : (topArea.containsMouse ? BeyGramTheme.surface2 : "transparent")

                    Accessible.role: Accessible.Button
                    Accessible.name: topCell.emoji
                    Accessible.checked: picker.selectedEmoji === topCell.emoji

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: topCell.emoji
                        color: BeyGramTheme.text
                        font.pixelSize: 18
                    }

                    MouseArea {
                        id: topArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (picker.facade) picker.facade.toggle(topCell.emoji)
                    }
                }
            }
        }

        RowLayout {
            id: recentRow
            spacing: 2
            visible: picker.showMore && picker.recentEmoji.length > 0
            Layout.alignment: Qt.AlignHCenter

            Repeater {
                model: picker.showMore ? picker.recentEmoji : []

                delegate: Rectangle {
                    id: recentCell
                    property string emoji: modelData

                    Layout.preferredWidth: picker.cellSize
                    Layout.preferredHeight: picker.cellSize
                    radius: 10
                    color: picker.selectedEmoji === recentCell.emoji
                           ? BeyGramTheme.accentSoft
                           : (recentArea.containsMouse ? BeyGramTheme.surface2 : "transparent")

                    Accessible.role: Accessible.Button
                    Accessible.name: recentCell.emoji
                    Accessible.checked: picker.selectedEmoji === recentCell.emoji

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: recentCell.emoji
                        color: BeyGramTheme.text
                        font.pixelSize: 18
                    }

                    MouseArea {
                        id: recentArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (picker.facade) picker.facade.toggle(recentCell.emoji)
                    }
                }
            }
        }

        RowLayout {
            id: footer
            spacing: 2
            Layout.fillWidth: true

            // Reveals or hides the recent row.
            Rectangle {
                id: moreButton
                Layout.preferredWidth: moreLabel.implicitWidth + 12
                Layout.preferredHeight: 22
                radius: 11
                color: moreArea.containsMouse ? BeyGramTheme.surface2 : "transparent"

                Accessible.role: Accessible.Button
                Accessible.name: picker.showMore ? qsTr("Fewer reactions")
                                               : qsTr("More reactions")
                Accessible.checked: picker.showMore

                Text {
                    id: moreLabel
                    anchors.centerIn: parent
                    text: picker.showMore ? qsTr("Less") : qsTr("More")
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }

                MouseArea {
                    id: moreArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (picker.facade)
                                   picker.facade.setExpanded(!picker.showMore)
                }
            }

            Item { Layout.fillWidth: true }

            // Dismisses the popup without choosing anything.
            Rectangle {
                id: closeButton
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                radius: 11
                color: closeArea.containsMouse ? BeyGramTheme.surface2 : "transparent"

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close reaction picker")

                Text {
                    anchors.centerIn: parent
                    text: "\u2715"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: picker.close()
                }
            }
        }
    }
}
