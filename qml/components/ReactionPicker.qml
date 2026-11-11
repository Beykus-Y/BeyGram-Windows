// Small emoji picker popup for adding a reaction to a message.
//
// Presentation only. The parent opens it next to its own add-reaction
// affordance and turns the emitted reactionSelected(emoji) into a TDLib
// addMessageReaction request for the message it is currently showing. Nothing
// in this file touches TDLib, the network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Popup {
    id: reactionPicker

    /// Emoji offered by the grid. The default is the common Telegram reaction
    /// set; the parent may replace it with the list returned by TDLib.
    property var emojis: [
        "👍", "❤️", "🔥", "🥰", "👏", "😁",
        "🤔", "🤯", "😱", "😢", "🎉", "🤩",
        "💯", "🙏", "👌", "🫡", "😍", "🆒",
        "💔", "🤣", "😡", "🌚", "🤝", "🥴"
    ]

    signal reactionSelected(string emoji)

    /// Number of emoji per row.
    readonly property int columns: 6
    /// Edge length of one emoji button.
    readonly property int cellSize: 36
    /// Gap between the anchor item and the popup.
    readonly property int anchorGap: 6

    padding: 8
    modal: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    implicitWidth: grid.implicitWidth + leftPadding + rightPadding
    implicitHeight: grid.implicitHeight + topPadding + bottomPadding

    // Places the popup directly under the parent item, flipping above it when
    // the window bottom is too close, and keeps it horizontally on screen.
    // Called from aboutToShow so open() needs no arguments from the caller.
    function placeNearParent() {
        const anchor = reactionPicker.parent
        const overlay = Overlay.overlay
        if (!anchor || !overlay)
            return

        const anchorTopLeft = anchor.mapToItem(overlay, 0, 0)
        const anchorBottom = anchorTopLeft.y + anchor.height
        const spaceBelow = overlay.height - anchorBottom
        const spaceAbove = anchorTopLeft.y

        let y = anchor.height + reactionPicker.anchorGap
        if (spaceBelow < reactionPicker.height + reactionPicker.anchorGap
                && spaceAbove > spaceBelow)
            y = -reactionPicker.height - reactionPicker.anchorGap

        // Centre on the anchor, then clamp so the popup cannot leave the window.
        let x = Math.round((anchor.width - reactionPicker.width) / 2)
        const minX = reactionPicker.anchorGap - anchorTopLeft.x
        const maxX = overlay.width - anchorTopLeft.x - reactionPicker.width
                - reactionPicker.anchorGap
        if (maxX >= minX)
            x = Math.min(Math.max(x, minX), maxX)

        reactionPicker.x = x
        reactionPicker.y = y
    }

    // Emits the chosen emoji and dismisses the picker.
    function select(emoji) {
        if (!emoji || emoji.length === 0)
            return
        reactionPicker.reactionSelected(emoji)
        reactionPicker.close()
    }

    onAboutToShow: reactionPicker.placeNearParent()

    background: Rectangle {
        radius: 12
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
    }

    GridLayout {
        id: grid
        anchors.fill: parent
        columns: reactionPicker.columns
        rowSpacing: 2
        columnSpacing: 2

        Repeater {
            model: reactionPicker.emojis ? reactionPicker.emojis : []

            delegate: Rectangle {
                id: cell
                property string emoji: modelData

                Layout.preferredWidth: reactionPicker.cellSize
                Layout.preferredHeight: reactionPicker.cellSize
                radius: 10
                color: cellArea.containsMouse ? BeyGramTheme.surface2 : "transparent"

                Accessible.role: Accessible.Button
                Accessible.name: cell.emoji

                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: cell.emoji
                    color: BeyGramTheme.text
                    font.pixelSize: 18
                }

                MouseArea {
                    id: cellArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: reactionPicker.select(cell.emoji)
                }
            }
        }
    }
}
