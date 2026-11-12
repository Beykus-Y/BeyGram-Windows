// Compact reaction strip for a message bubble.
//
// Presentation only. The parent owns the reaction model — normally the result of
// beygram::telegram::reactionsFromMessage() mapped to { type, emoji,
// customEmojiId, count, chosen } — and turns the emitted signals into TDLib
// addMessageReaction / removeMessageReaction requests. Nothing in this file
// touches TDLib, the network, the message store, or logging.
//
// Chips are DsChip pills so the strip inherits the design-system geometry,
// hover/focus states and selected styling. The public API is unchanged.

import QtQuick
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

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
            return "\u2605"
        if (reaction.emoji && reaction.emoji.length > 0)
            return reaction.emoji
        // Paid (Stars) reactions carry no emoji; Telegram renders them as a star.
        return reaction.type === "paid" ? "\u2605" : ""
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
        spacing: Design.space1

        Repeater {
            model: reactionBar.reactions ? reactionBar.reactions : []

            delegate: DsChip {
                id: pill
                property var reaction: modelData
                property bool chosen: !!(reaction && reaction.chosen)
                property int count: reactionBar.countFor(reaction)
                property string glyph: reactionBar.glyphFor(reaction)

                text: pill.glyph.length > 0
                      ? (pill.count > 0 ? pill.glyph + " " + pill.count : pill.glyph)
                      : (pill.count > 0 ? String(pill.count) : "")
                selected: pill.chosen
                enabled: pill.glyph.length > 0

                onClicked: reactionBar.toggle(pill.reaction)
            }
        }

        // Trailing add-reaction affordance.
        DsChip {
            visible: reactionBar.canReact
            text: "+"

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Add reaction")

            onClicked: reactionBar.addReactionRequested()
        }
    }
}
