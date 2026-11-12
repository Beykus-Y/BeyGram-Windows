import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// One chat message row.
//
// Presentation only: the page owns the message state, editing, replying,
// selection and playback. This component keeps the established MessageBubble
// contract (one property per MessageListModel role plus the action signals) and
// lays the content out with the BeyGram design system: 16 px bubble radius,
// 4 px tail, 72% max width, a 12 px sender line, 14 px body and a compact
// time / edited / status footer.
Item {
    id: bubbleItem

    // --- Message contract -------------------------------------------------
    property var messageId: 0
    property string text: ""
    property string textHtml: ""
    property string replyPreview: ""
    property string reactionsJson: ""
    property string linkUrl: ""
    property string linkTitle: ""
    property string linkDescription: ""
    property string linkSite: ""
    property int linkFileId: 0
    property bool linkVideo: false
    property string pollQuestion: ""
    property string pollOptionsJson: ""
    property int pollTotal: 0
    property bool pollClosed: false
    property bool pollQuiz: false
    property bool pollMultiple: false
    property bool outgoing: false
    property string senderName: ""
    property var date: 0
    property bool pending: false
    property bool failed: false
    property bool edited: false
    property string contentType: "text"
    property int mediaFileId: 0
    property int mediaFullFileId: 0
    property int mediaWidth: 0
    property int mediaHeight: 0
    property bool mediaReady: false
    property int mediaRevision: 0

    // --- Selection (optional; off by default) -----------------------------
    property bool selectable: false
    property bool selected: false

    signal editRequested(var messageId, string text)
    signal deleteRequested(var messageId)
    signal mediaOpenRequested(int fileId)
    signal mediaPlayRequested(int fileId)
    signal reactionRequested(string emoji)
    signal forwardRequested(var messageId)
    signal pollOptionSelected(int index)
    signal replyRequested(var messageId, string preview)
    signal selectionToggled(var messageId, bool isSelected)

    // --- DS bubble geometry ------------------------------------------------
    readonly property int _hPad: 10
    readonly property int _vPad: 8
    readonly property real availableWidth: ListView.view && ListView.view.width > 0
                                           ? ListView.view.width
                                           : (bubbleItem.parent && bubbleItem.parent.width > 0
                                              ? bubbleItem.parent.width : 320)
    readonly property real maxBubbleWidth: Math.max(72, availableWidth * Design.bubbleMaxWidthRatio)
    readonly property real mediaPreviewWidth: Math.min(320,
                                                       bubbleItem.mediaWidth > 0
                                                       ? bubbleItem.mediaWidth : 240)
    // Clamp the media block to the bubble so its height always matches its
    // width and the picture keeps its aspect ratio.
    readonly property real mediaDisplayWidth: Math.max(1,
                                                       Math.min(mediaPreviewWidth,
                                                                maxBubbleWidth - _hPad * 2))
    readonly property string statusGlyph: failed ? "!"
                                          : pending ? "\u2026"
                                          : outgoing ? "\u2713" : ""

    implicitWidth: bubble.width
    implicitHeight: bubble.height + Design.space2
    width: ListView.view ? ListView.view.width : (parent ? parent.width : implicitWidth)

    Accessible.role: Accessible.ListItem
    Accessible.name: {
        const parts = []
        if (bubbleItem.senderName.length > 0)
            parts.push(bubbleItem.senderName)
        const body = bubbleItem.displayText()
        if (body.length > 0)
            parts.push(body)
        else if (bubbleItem.contentType === "sticker")
            parts.push("Sticker")
        else if (bubbleItem.mediaFileId > 0)
            parts.push("Media")
        if (bubbleItem.edited)
            parts.push("edited")
        if (bubbleItem.timeText().length > 0)
            parts.push(bubbleItem.timeText())
        if (bubbleItem.failed)
            parts.push("Not sent")
        else if (bubbleItem.pending)
            parts.push("Sending")
        return parts.join(", ")
    }
    Accessible.selected: bubbleItem.selected

    function pollOptions() {
        if (!pollOptionsJson)
            return []
        try {
            return JSON.parse(pollOptionsJson)
        } catch (error) {
            return []
        }
    }

    function reactionList() {
        if (!reactionsJson)
            return []
        try {
            const root = JSON.parse(reactionsJson)
            const items = root.reactions || []
            return items.map(function(item) {
                const type = item.type || {}
                const kind = type["@type"] || ""
                const count = Number(item.total_count !== undefined
                                     ? item.total_count
                                     : (item.count || 0))
                return {
                    type: kind === "reactionTypeEmoji" ? "emoji"
                          : kind === "reactionTypeCustomEmoji" ? "custom_emoji"
                          : kind === "reactionTypePaid" ? "paid" : "",
                    emoji: type.emoji || "",
                    customEmojiId: Number(type.custom_emoji_id || 0),
                    count: count,
                    chosen: item.is_chosen === true
                }
            }).filter(function(item) { return item.count > 0 })
        } catch (error) {
            return []
        }
    }

    function timeText() {
        if (!date || date <= 0)
            return ""
        return Qt.formatTime(new Date(date * 1000), "hh:mm")
    }

    function displayText() {
        if (mediaFileId > 0 && (text === "[Photo]" || text === "[Video]"
                                || text === "[Document]" || text === "[Sticker]"))
            return ""
        if (contentType === "voice" || contentType === "audio"
            || contentType === "video_note")
            return ""
        return text
    }

    function mediaHeightForWidth(width) {
        if (mediaWidth <= 0 || mediaHeight <= 0)
            return Math.round(width * 0.66)
        return Math.round(width * mediaHeight / mediaWidth)
    }

    Rectangle {
        id: bubble

        width: Math.min(bubbleItem.maxBubbleWidth,
                        Math.max(72, content.implicitWidth + bubbleItem._hPad * 2))
        height: content.implicitHeight + bubbleItem._vPad * 2
        anchors.right: bubbleItem.outgoing ? bubbleItem.right : undefined
        anchors.left: bubbleItem.outgoing ? undefined : bubbleItem.left
        anchors.rightMargin: Design.space3
        anchors.leftMargin: Design.space3
        radius: Design.bubbleRadius
        bottomRightRadius: bubbleItem.outgoing ? Design.bubbleRadiusTail
                                               : Design.bubbleRadius
        bottomLeftRadius: bubbleItem.outgoing ? Design.bubbleRadius
                                              : Design.bubbleRadiusTail
        color: bubbleItem.outgoing ? BeyGramTheme.bubbleOut : BeyGramTheme.surface
        border.width: bubbleItem.selected ? 1 : 0
        border.color: BeyGramTheme.accent
        opacity: bubbleItem.failed ? 0.85 : 1.0

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: bubbleItem._hPad
            anchors.rightMargin: bubbleItem._hPad
            anchors.topMargin: bubbleItem._vPad
            spacing: Design.space1

            // Author line: 12 px accent ink, incoming only.
            Text {
                Layout.fillWidth: true
                visible: !bubbleItem.outgoing && bubbleItem.senderName.length > 0
                text: bubbleItem.senderName
                color: BeyGramTheme.accentInk
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            // Reply quote: 3 px accent rail plus a 12 px preview.
            Rectangle {
                Layout.fillWidth: true
                visible: bubbleItem.replyPreview.length > 0
                implicitWidth: quoteText.implicitWidth + 18
                implicitHeight: quoteText.implicitHeight + 10
                radius: Design.radiusXs
                color: Qt.alpha(BeyGramTheme.accent, 0.10)
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: BeyGramTheme.accent
                }

                Text {
                    id: quoteText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 9
                    anchors.rightMargin: 8
                    text: bubbleItem.replyPreview
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    lineHeight: Design.lineNormal
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bubbleItem.replyRequested(bubbleItem.messageId,
                                                         bubbleItem.replyPreview)
                }
            }

            // Media: 12 px frame with a 4 px matte around the picture.
            Rectangle {
                id: mediaFrame
                Layout.alignment: Qt.AlignLeft
                Layout.preferredWidth: bubbleItem.mediaDisplayWidth
                Layout.preferredHeight: bubbleItem.mediaHeightForWidth(
                                            bubbleItem.mediaDisplayWidth)
                implicitWidth: bubbleItem.mediaDisplayWidth
                visible: bubbleItem.mediaFileId > 0
                         && bubbleItem.contentType !== "sticker"
                radius: Design.radiusMd
                color: Qt.alpha(BeyGramTheme.surface2, 0.6)
                clip: true

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Design.space1
                    radius: Design.radiusXs + 2
                    clip: true
                    color: Qt.alpha(BeyGramTheme.bg, 0.18)

                    Image {
                        anchors.fill: parent
                        source: bubbleItem.mediaReady && bubbleItem.mediaFileId > 0
                                ? "image://beygrammedia/" + bubbleItem.mediaFileId
                                  + "?v=" + bubbleItem.mediaRevision
                                : ""
                        cache: true
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                    }
                }

                DsSpinner {
                    anchors.centerIn: parent
                    running: bubbleItem.mediaFileId > 0 && !bubbleItem.mediaReady
                    visible: running
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: bubbleItem.mediaReady ? Qt.PointingHandCursor
                                                       : Qt.ArrowCursor
                    enabled: bubbleItem.mediaReady
                    onClicked: {
                        const fullId = bubbleItem.mediaFullFileId > 0
                                       ? bubbleItem.mediaFullFileId
                                       : bubbleItem.mediaFileId
                        if (bubbleItem.contentType === "video"
                            || bubbleItem.contentType === "voice"
                            || bubbleItem.contentType === "audio"
                            || bubbleItem.contentType === "video_note"
                            || bubbleItem.contentType === "animation")
                            bubbleItem.mediaPlayRequested(fullId)
                        else
                            bubbleItem.mediaOpenRequested(fullId)
                    }
                }
            }

            StickerBubble {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(128, bubbleItem.mediaDisplayWidth)
                Layout.preferredHeight: Math.min(128, bubbleItem.mediaDisplayWidth)
                visible: bubbleItem.contentType === "sticker"
                fileId: 0
                thumbnailFileId: bubbleItem.mediaFileId
                ready: bubbleItem.mediaReady
                revision: bubbleItem.mediaRevision
                animated: false
                onClicked: bubbleItem.mediaOpenRequested(
                               bubbleItem.mediaFullFileId > 0
                               ? bubbleItem.mediaFullFileId
                               : bubbleItem.mediaFileId)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space2
                visible: bubbleItem.contentType === "voice"
                         || bubbleItem.contentType === "audio"
                         || bubbleItem.contentType === "video_note"

                DsButton {
                    text: "Play"
                    size: "sm"
                    tone: "secondary"
                    enabled: bubbleItem.mediaFullFileId > 0
                    onClicked: bubbleItem.mediaPlayRequested(bubbleItem.mediaFullFileId)
                }

                Text {
                    Layout.fillWidth: true
                    text: bubbleItem.contentType === "voice" ? "Voice message"
                          : bubbleItem.contentType === "video_note" ? "Video message"
                          : "Audio"
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    elide: Text.ElideRight
                }
            }

            // Body: 14 px on a 1.35 line height.
            Text {
                Layout.fillWidth: true
                visible: bubbleItem.displayText().length > 0
                text: bubbleItem.textHtml.length > 0
                      ? bubbleItem.textHtml
                      : bubbleItem.displayText()
                color: BeyGramTheme.text
                font.pixelSize: Design.fontBody
                wrapMode: Text.Wrap
                lineHeight: Design.lineNormal
                textFormat: bubbleItem.textHtml.length > 0 ? Text.RichText
                                                           : Text.PlainText
                linkColor: BeyGramTheme.accentInk
                onLinkActivated: (link) => BeyGramApp.openLink(link)
            }

            LinkPreviewCard {
                Layout.fillWidth: true
                visible: bubbleItem.linkUrl.length > 0
                title: bubbleItem.linkTitle
                description: bubbleItem.linkDescription
                siteName: bubbleItem.linkSite
                url: bubbleItem.linkUrl
                thumbnailFileId: bubbleItem.linkFileId
                thumbnailReady: false
                revision: 0
                onClicked: BeyGramApp.openLink(bubbleItem.linkUrl)
            }

            PollBubble {
                Layout.fillWidth: true
                visible: bubbleItem.pollQuestion.length > 0
                question: bubbleItem.pollQuestion
                options: bubbleItem.pollOptions()
                totalVoterCount: bubbleItem.pollTotal
                closed: bubbleItem.pollClosed
                quiz: bubbleItem.pollQuiz
                multipleChoice: bubbleItem.pollMultiple
                onOptionSelected: (index) => bubbleItem.pollOptionSelected(index)
            }

            // Footer: edited + time + delivery glyph, right aligned.
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space1
                visible: bubbleItem.timeText().length > 0
                         || bubbleItem.edited
                         || bubbleItem.statusGlyph.length > 0

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    visible: bubbleItem.edited
                    text: "edited"
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                }

                Text {
                    text: bubbleItem.timeText()
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                }

                Text {
                    visible: bubbleItem.statusGlyph.length > 0
                    text: bubbleItem.statusGlyph
                    color: bubbleItem.failed ? BeyGramTheme.danger : BeyGramTheme.muted
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            ReactionBar {
                Layout.fillWidth: true
                visible: bubbleItem.reactionList().length > 0
                reactions: bubbleItem.reactionList()
                canReact: false
                onReactionToggled: (emoji) => bubbleItem.reactionRequested(emoji)
            }
        }

        // Selection intercepts left clicks only while selection mode is on;
        // otherwise media, replies and links keep their normal click paths.
        MouseArea {
            anchors.fill: parent
            visible: bubbleItem.selectable
            enabled: bubbleItem.selectable
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: bubbleItem.selectionToggled(bubbleItem.messageId,
                                                   !bubbleItem.selected)
        }

        // Right click opens the message actions without disturbing left clicks.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: (mouse) => menu.popup(bubble, mouse.x, mouse.y)
        }
    }

    DsMenu {
        id: menu

        DsMenuItem {
            text: "Reply"
            glyph: "\u21A9"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.replyRequested(bubbleItem.messageId, bubbleItem.text)
        }
        DsMenuItem {
            text: "Forward"
            glyph: "\u21AA"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.forwardRequested(bubbleItem.messageId)
        }
        DsMenuItem {
            text: "Copy text"
            glyph: "\u29C9"
            enabled: bubbleItem.text.length > 0
            onClicked: BeyGramApp.copyText(bubbleItem.text)
        }
        DsMenuItem {
            text: "Edit"
            glyph: "\u270E"
            visible: bubbleItem.outgoing && !bubbleItem.pending && !bubbleItem.failed
            onClicked: bubbleItem.editRequested(bubbleItem.messageId, bubbleItem.text)
        }
        DsMenuItem {
            text: "Delete"
            glyph: "\u2715"
            tone: "danger"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.deleteRequested(bubbleItem.messageId)
        }
    }
}
