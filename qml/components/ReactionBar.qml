// Compact reaction strip for a message bubble.
//
// Presentation only. The parent owns the reaction model — normally the result of
// beygram::telegram::reactionsFromMessage() mapped to { type, emoji,
// customEmojiId, count, chosen } — and turns the emitted signals into TDLib
// addMessageReaction / removeMessageReaction requests. Nothing in this file
// touches TDLib, the network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: reactionBar

    /// Array of { type, emoji, customEmojiId, count, chosen } entries.
    /// Counts are shown exactly as provided; a count of 0 is not rendered.
    property var reactions: []
    /// Whether the account may add a reaction to this message.
    property bool canReact: true

    signal reactionToggled(string emoji)
    signal customReactionToggled(int customEmojiId)
    signal addReactionRequested()

    implicitWidth: flow.implicitWidth
    implicitHeight: flow.implicitHeight

    // Custom emoji have no printable glyph here, so a neutral star stands in.
    function glyphFor(reaction) {
        if (!reaction)
            return ""
        if (reaction.customEmojiId > 0)
            return "★"
        if (reaction.emoji && reaction.emoji.length > 0)
            return reaction.emoji
        // Paid (Stars) reactions carry no emoji; Telegram renders them as a star.
        return reaction.type === "paid" ? "★" : ""
    }

    // Returns 0 for a missing or non-positive count so the label can stay hidden.
    function countFor(reaction) {
        if (!reaction)
            return 0
        const value = Number(reaction.count)
        return value > 0 ? Math.floor(value) : 0
    }

    // Emits the matching toggle signal; paid or malformed entries stay inert.
    function toggle(reaction) {
        if (!reaction)
            return
        if (reaction.customEmojiId > 0) {
            reactionBar.customReactionToggled(reaction.customEmojiId)
            return
        }
        if (reaction.emoji && reaction.emoji.length > 0)
            reactionBar.reactionToggled(reaction.emoji)
    }

    Flow {
        id: flow
        // The parent supplies the width (bubble layout / Layout.fillWidth); the
        // strip wraps at that width and reports the resulting height.
        width: reactionBar.width
        spacing: 4

        Repeater {
            model: reactionBar.reactions ? reactionBar.reactions : []

            delegate: Rectangle {
                id: pill
                property var reaction: modelData
                property bool chosen: !!(reaction && reaction.chosen)
                property int count: reactionBar.countFor(reaction)
                property string glyph: reactionBar.glyphFor(reaction)

                width: content.implicitWidth + 18
                height: 28
                radius: 14
                color: pill.chosen ? BeyGramTheme.accentSoft : BeyGramTheme.surface2

                Accessible.role: Accessible.CheckBox
                Accessible.name: pill.count > 0 ? pill.glyph + " " + pill.count : pill.glyph
                Accessible.checked: pill.chosen

                // Hover tint sits over the base colour so chosen/unchosen keep
                // their exact accentSoft / surface2 fills.
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: Qt.alpha(BeyGramTheme.accent, pill.chosen ? 0.10 : 0.08)
                    visible: hoverArea.containsMouse
                }

                RowLayout {
                    id: content
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        text: pill.glyph
                        visible: text.length > 0
                        color: BeyGramTheme.text
                        font.pixelSize: 13
                    }

                    Text {
                        text: pill.count
                        visible: pill.count > 0
                        color: pill.chosen ? BeyGramTheme.accentInk : BeyGramTheme.muted
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    id: hoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    enabled: pill.glyph.length > 0
                    onClicked: reactionBar.toggle(pill.reaction)
                }
            }
        }

        // Trailing add-reaction affordance.
        Rectangle {
            visible: reactionBar.canReact
            width: 28
            height: 28
            radius: 14
            color: plusHover.containsMouse ? Qt.alpha(BeyGramTheme.accent, 0.14)
                                           : BeyGramTheme.surface2

            Accessible.role: Accessible.Button
            Accessible.name: "Add reaction"

            Text {
                anchors.centerIn: parent
                text: "+"
                color: BeyGramTheme.muted
                font.pixelSize: 15
            }

            MouseArea {
                id: plusHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: reactionBar.addReactionRequested()
            }
        }
    }
}
