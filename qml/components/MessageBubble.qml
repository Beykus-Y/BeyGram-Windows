pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// One chat message row, modeled after Android MessageBubbleLayout.kt.
//
// Preserves the complete MessageBubble contract (one property per
// MessageListModel role plus all action signals), honors BubbleStyle,
// DesignStyle, messageTextSize, AMOLED borders, and reduceMotion, and adds a
// subtle hover quick-action bar for desktop ergonomics.
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
    property bool mediaFailed: false
    property int mediaRevision: 0

    // --- Selection --------------------------------------------------------
    property bool selectable: false
    property bool selected: false

    signal editRequested(var messageId, string text)
    signal deleteRequested(var messageId)
    signal mediaOpenRequested(int fileId)
    signal mediaPlayRequested(int fileId)
    signal reactionRequested(string emoji)
    signal addReactionRequested(var messageId, Item anchorItem)
    signal forwardRequested(var messageId)
    signal pollOptionSelected(int index)
    signal replyRequested(var messageId, string preview)
    signal selectionToggled(var messageId, bool isSelected)

    // --- Geometry & style tokens ------------------------------------------
    readonly property int _hPad: 12
    readonly property int _vPad: 8
    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
    readonly property int _bubbleRadius: Design.bubbleRadiusFor(BeyGramAppearance.bubbleStyle)
    readonly property int _tailRadius: Design.bubbleTailRadiusFor(BeyGramAppearance.bubbleStyle)
    readonly property bool _isLiquidGlass: BeyGramAppearance.designStyle === "LiquidGlass"
    readonly property bool _isAmoled: BeyGramTheme.modeName === "Amoled"
    readonly property bool _isCompact: BeyGramAppearance.listDensity === "Compact"
    /// Content that keeps a media block even before its file is downloaded, so
    /// the bubble does not collapse into an empty pill.
    readonly property bool _hasMediaBlock: bubbleItem.contentType === "photo"
                                           || bubbleItem.contentType === "video"
                                           || bubbleItem.contentType === "animation"
                                           || bubbleItem.contentType === "document"
    readonly property string _mediaLabel: bubbleItem.contentType === "photo" ? qsTr("Photo")
                                          : bubbleItem.contentType === "video" ? qsTr("Video")
                                          : bubbleItem.contentType === "animation" ? qsTr("GIF")
                                          : qsTr("Document")

    // Sticker messages carry an emoji fallback in `text`; that text is never a
    // bubble body, so the content type alone decides whether to draw a bubble.
    readonly property bool _stickerOnly: bubbleItem.contentType === "sticker"
                                         && bubbleItem.replyPreview.length === 0
                                         && bubbleItem.linkUrl.length === 0
                                         && bubbleItem.pollQuestion.length === 0
                                         && (bubbleItem.outgoing || bubbleItem.senderName.length === 0)

    readonly property real availableWidth: ListView.view && ListView.view.width > 0
                                           ? ListView.view.width
                                           : (bubbleItem.parent && bubbleItem.parent.width > 0
                                              ? bubbleItem.parent.width : 360)
    readonly property real maxBubbleWidth: Math.max(96, availableWidth * Design.bubbleMaxWidthRatio)
    readonly property real mediaPreviewWidth: Math.min(320,
                                                       bubbleItem.mediaWidth > 0
                                                       ? bubbleItem.mediaWidth : 240)
    readonly property real mediaDisplayWidth: Math.max(1,
                                                       Math.min(mediaPreviewWidth,
                                                                maxBubbleWidth - _hPad * 2))

    // Inline timestamp fits only when the *plain* text is short and nothing else
    // shares the bubble. textHtml is deliberately ignored: the formatter fills
    // it for plain text too, so it says nothing about length or line count.
    readonly property bool canInlineMeta: {
        const body = bubbleItem.text
        return body.length > 0
               && body.length <= 32
               && body.indexOf("\n") < 0
               && bubbleItem.displayText().length > 0
               && (bubbleItem.outgoing || bubbleItem.senderName.length === 0)
               && bubbleItem.replyPreview.length === 0
               && bubbleItem.mediaFileId <= 0
               && !bubbleItem._hasMediaBlock
               && bubbleItem.contentType !== "sticker"
               && bubbleItem.contentType !== "voice"
               && bubbleItem.contentType !== "audio"
               && bubbleItem.contentType !== "video_note"
               && bubbleItem.linkUrl.length === 0
               && bubbleItem.pollQuestion.length === 0
               && !bubbleItem.failed
    }

    readonly property string statusGlyph: bubbleItem.failed ? "!"
                                          : bubbleItem.pending ? "\u2026"
                                          : bubbleItem.outgoing ? "\u2713" : ""

    readonly property bool showQuickActions: (rowHover.hovered || quickBarHover.hovered)
                                             && !bubbleItem.selectable
                                             && bubbleItem.messageId > 0

    implicitWidth: bubble.width
    implicitHeight: Math.max(bubble.height, bubbleItem.selectable ? 36 : 0)
                    + (bubbleItem._isCompact ? Design.space1 : Design.space2)
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
            parts.push(qsTr("Sticker"))
        else if (bubbleItem.mediaFileId > 0)
            parts.push(qsTr("Media"))
        if (bubbleItem.edited)
            parts.push(qsTr("edited"))
        if (bubbleItem.timeText().length > 0)
            parts.push(bubbleItem.timeText())
        if (bubbleItem.failed)
            parts.push(qsTr("Not sent"))
        else if (bubbleItem.pending)
            parts.push(qsTr("Sending"))
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

    function mediaHeightForWidth(w) {
        if (mediaWidth <= 0 || mediaHeight <= 0)
            return Math.round(w * 0.66)
        return Math.max(80, Math.min(340, Math.round(w * mediaHeight / mediaWidth)))
    }

    HoverHandler {
        id: rowHover
    }

    // Selection indicator on the leading edge in selection mode
    Rectangle {
        visible: bubbleItem.selectable
        width: 20
        height: 20
        radius: 10
        anchors.left: parent.left
        anchors.leftMargin: Design.space3
        anchors.verticalCenter: bubble.verticalCenter
        color: bubbleItem.selected ? BeyGramTheme.accent : "transparent"
        border.width: bubbleItem.selected ? 0 : 1.5
        border.color: BeyGramTheme.muted
        antialiasing: true

        Text {
            anchors.centerIn: parent
            visible: bubbleItem.selected
            text: "\u2713"
            color: BeyGramTheme.onAccent
            font.pixelSize: 11
            font.weight: Font.Bold
        }
    }

    Rectangle {
        id: bubble

        width: bubbleItem._stickerOnly
               ? Math.max(112, content.implicitWidth + bubbleItem._hPad * 2)
               : Math.min(bubbleItem.maxBubbleWidth,
                          Math.max(80, content.implicitWidth + bubbleItem._hPad * 2))
        height: content.implicitHeight + bubbleItem._vPad * 2
        // Position with an explicit x instead of toggling left/right anchors:
        // a delegate reused for the other direction kept both anchors active,
        // which stretched every bubble to the full row width.
        x: bubbleItem.outgoing
           ? bubbleItem.width - width - Design.space4
           : (bubbleItem.selectable ? Design.space10 : Design.space4)
        radius: bubbleItem._bubbleRadius
        bottomRightRadius: bubbleItem.outgoing ? bubbleItem._tailRadius
                                               : bubbleItem._bubbleRadius
        bottomLeftRadius: bubbleItem.outgoing ? bubbleItem._bubbleRadius
                                              : bubbleItem._tailRadius
        color: {
            if (bubbleItem._stickerOnly)
                return "transparent"
            const base = bubbleItem.outgoing ? BeyGramTheme.bubbleOut : BeyGramTheme.surface
            if (bubbleItem._isLiquidGlass)
                return Qt.alpha(base, BeyGramTheme.darkMode ? 0.88 : 0.92)
            return base
        }
        border.width: {
            if (bubbleItem._stickerOnly)
                return bubbleItem.selected ? 1 : 0
            if (bubbleItem.selected)
                return 1.5
            if (!bubbleItem.outgoing || bubbleItem._isAmoled || bubbleItem._isLiquidGlass)
                return 1
            return 0
        }
        border.color: {
            if (bubbleItem.selected)
                return BeyGramTheme.accent
            if (bubbleItem.outgoing)
                return Qt.alpha(BeyGramTheme.accent, bubbleItem._isAmoled ? 0.35 : 0.22)
            return bubbleItem._isLiquidGlass
                   ? Qt.alpha(BeyGramTheme.darkMode ? "#FFFFFF" : BeyGramTheme.line,
                              BeyGramTheme.darkMode ? 0.14 : 0.85)
                   : BeyGramTheme.line
        }
        opacity: bubbleItem.failed ? 0.88 : 1.0
        antialiasing: true

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: bubbleItem._hPad
            anchors.rightMargin: bubbleItem._hPad
            anchors.topMargin: bubbleItem._vPad
            spacing: Design.space1 + 1

            // Author line: incoming group/channel messages
            Text {
                Layout.fillWidth: true
                visible: !bubbleItem.outgoing && bubbleItem.senderName.length > 0
                text: bubbleItem.senderName
                color: BeyGramTheme.accentInk
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            // Reply quote: 3 px accent rail
            Rectangle {
                Layout.fillWidth: true
                visible: bubbleItem.replyPreview.length > 0
                implicitWidth: quoteText.implicitWidth + 20
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

            // Media frame
            Rectangle {
                id: mediaFrame
                Layout.alignment: Qt.AlignLeft
                Layout.preferredWidth: bubbleItem.mediaFileId > 0
                                       ? bubbleItem.mediaDisplayWidth
                                       : 220
                Layout.preferredHeight: bubbleItem.mediaFileId > 0
                                        ? bubbleItem.mediaHeightForWidth(
                                              bubbleItem.mediaDisplayWidth)
                                        : 140
                implicitWidth: bubbleItem.mediaDisplayWidth
                visible: (bubbleItem.mediaFileId > 0 || bubbleItem._hasMediaBlock)
                         && bubbleItem.contentType !== "sticker"
                radius: Design.radiusMd
                color: Qt.alpha(BeyGramTheme.surface2, 0.7)
                clip: true

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

                // Play indicator badge for video/animation
                Rectangle {
                    anchors.centerIn: parent
                    width: 44
                    height: 44
                    radius: 22
                    color: Qt.alpha("#000000", 0.55)
                    visible: bubbleItem.mediaReady
                             && (bubbleItem.contentType === "video"
                                 || bubbleItem.contentType === "animation"
                                 || bubbleItem.contentType === "video_note")

                    Text {
                        anchors.centerIn: parent
                        text: "\u25B6"
                        color: "white"
                        font.pixelSize: 16
                    }
                }

                DsSpinner {
                    anchors.centerIn: parent
                    running: bubbleItem.mediaFileId > 0 && !bubbleItem.mediaReady
                             && !bubbleItem.mediaFailed
                    visible: running
                }

                // Download failed (or timed out): stop spinning, explain why and
                // keep a retry affordance where the spinner was.
                Column {
                    anchors.centerIn: parent
                    spacing: Design.space1
                    visible: bubbleItem.mediaFileId > 0 && !bubbleItem.mediaReady
                             && bubbleItem.mediaFailed

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\u27F3"
                        color: BeyGramTheme.danger
                        font.pixelSize: 20
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(24, mediaFrame.width - Design.space4)
                        text: qsTr("Could not load media")
                        color: BeyGramTheme.danger
                        font.pixelSize: Design.fontSmall
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }

                // No file id yet (or the download never started): keep the block
                // recognisable instead of showing an empty outline.
                Column {
                    anchors.centerIn: parent
                    spacing: Design.space1
                    visible: bubbleItem.mediaFileId <= 0 && bubbleItem._hasMediaBlock

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "\u25A3"
                        color: BeyGramTheme.muted
                        font.pixelSize: 22
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: bubbleItem._mediaLabel
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                    }
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
                Layout.preferredWidth: Math.min(136, bubbleItem.mediaDisplayWidth)
                Layout.preferredHeight: Math.min(136, bubbleItem.mediaDisplayWidth)
                visible: bubbleItem.contentType === "sticker"
                fileId: 0
                thumbnailFileId: bubbleItem.mediaFileId
                ready: bubbleItem.mediaReady
                revision: bubbleItem.mediaRevision
                animated: false
                emoji: bubbleItem.text
                onClicked: bubbleItem.mediaOpenRequested(
                               bubbleItem.mediaFullFileId > 0
                               ? bubbleItem.mediaFullFileId
                               : bubbleItem.mediaFileId)
            }

            // Voice / audio / video_note compact row
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space2
                visible: bubbleItem.contentType === "voice"
                         || bubbleItem.contentType === "audio"
                         || bubbleItem.contentType === "video_note"

                DsIconButton {
                    icon: "\uE768" // Play
                    tone: "accent"
                    size: "sm"
                    tooltip: qsTr("Play")
                    enabled: bubbleItem.mediaFullFileId > 0
                    onClicked: bubbleItem.mediaPlayRequested(bubbleItem.mediaFullFileId)
                }

                Text {
                    Layout.fillWidth: true
                    text: bubbleItem.contentType === "voice" ? qsTr("Voice message")
                          : bubbleItem.contentType === "video_note" ? qsTr("Video message")
                          : qsTr("Audio")
                    color: BeyGramTheme.text
                    font.pixelSize: Design.fontSmall
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
            }

            // Short single-line message: inline timestamp + status beside text
            RowLayout {
                Layout.fillWidth: true
                visible: bubbleItem.canInlineMeta
                spacing: Design.space2

                Text {
                    Layout.fillWidth: true
                    text: bubbleItem.displayText()
                    color: BeyGramTheme.text
                    font.pixelSize: BeyGramAppearance.messageTextSize
                    lineHeight: Design.lineNormal
                    textFormat: Text.PlainText
                }

                Row {
                    Layout.alignment: Qt.AlignBottom
                    spacing: 3

                    Text {
                        visible: bubbleItem.edited
                        text: qsTr("edited")
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
                        color: bubbleItem.failed ? BeyGramTheme.danger
                                                 : (bubbleItem.pending ? BeyGramTheme.muted
                                                                       : BeyGramTheme.accentInk)
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            // Standard multi-line / rich text body
            Text {
                Layout.fillWidth: true
                visible: !bubbleItem.canInlineMeta && !bubbleItem._stickerOnly
                         && bubbleItem.displayText().length > 0
                text: bubbleItem.textHtml.length > 0
                      ? bubbleItem.textHtml
                      : bubbleItem.displayText()
                color: BeyGramTheme.text
                font.pixelSize: BeyGramAppearance.messageTextSize
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

            // Footer for non-inline messages
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space1
                visible: !bubbleItem.canInlineMeta
                         && (bubbleItem.timeText().length > 0
                             || bubbleItem.edited
                             || bubbleItem.statusGlyph.length > 0)

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    visible: bubbleItem._stickerOnly
                    implicitWidth: stickerMetaRow.implicitWidth + 10
                    implicitHeight: stickerMetaRow.implicitHeight + 4
                    radius: height / 2
                    color: Qt.alpha(BeyGramTheme.surface2, 0.85)

                    Row {
                        id: stickerMetaRow
                        anchors.centerIn: parent
                        spacing: 3

                        Text {
                            visible: bubbleItem.edited
                            text: qsTr("edited")
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
                            color: bubbleItem.failed ? BeyGramTheme.danger : BeyGramTheme.accentInk
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Row {
                    visible: !bubbleItem._stickerOnly
                    spacing: 3

                    Text {
                        visible: bubbleItem.edited
                        text: qsTr("edited")
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
                        color: bubbleItem.failed ? BeyGramTheme.danger
                                                 : (bubbleItem.pending ? BeyGramTheme.muted
                                                                       : BeyGramTheme.accentInk)
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            ReactionBar {
                Layout.fillWidth: true
                visible: bubbleItem.reactionList().length > 0
                reactions: bubbleItem.reactionList()
                canReact: !bubbleItem.selectable && bubbleItem.messageId > 0
                onReactionToggled: (emoji) => bubbleItem.reactionRequested(emoji)
                onAddReactionRequested: bubbleItem.addReactionRequested(
                                            bubbleItem.messageId, bubble)
            }
        }

        // Selection click intercept
        MouseArea {
            anchors.fill: parent
            visible: bubbleItem.selectable
            enabled: bubbleItem.selectable
            acceptedButtons: Qt.LeftButton
            cursorShape: Qt.PointingHandCursor
            onClicked: bubbleItem.selectionToggled(bubbleItem.messageId,
                                                   !bubbleItem.selected)
        }

        // Context menu on right click
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: (mouse) => menu.popup(bubble, mouse.x, mouse.y)
        }
    }

    // Floating hover quick action bar
    Rectangle {
        id: quickBar
        visible: opacity > 0
        opacity: bubbleItem.showQuickActions ? 1 : 0
        z: 5
        y: Math.max(0, bubble.y - height / 2 + 4)
        x: bubbleItem.outgoing
           ? Math.max(Design.space2, bubble.x - width - Design.space1)
           : Math.min(bubbleItem.width - width - Design.space2, bubble.x + bubble.width + Design.space1)
        width: quickRow.implicitWidth + Design.space2
        height: 30
        radius: height / 2
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
        antialiasing: true

        Behavior on opacity {
            NumberAnimation { duration: bubbleItem._animFast; easing.type: Easing.OutCubic }
        }

        HoverHandler {
            id: quickBarHover
        }

        RowLayout {
            id: quickRow
            anchors.centerIn: parent
            spacing: 0

            DsIconButton {
                icon: "\uE97A" // Reply
                size: "sm"
                tooltip: qsTr("Reply")
                onClicked: bubbleItem.replyRequested(bubbleItem.messageId, bubbleItem.text)
            }

            DsIconButton {
                icon: "\uE76E" // Emoji / React
                size: "sm"
                tooltip: qsTr("React")
                onClicked: bubbleItem.addReactionRequested(bubbleItem.messageId, quickBar)
            }

            DsIconButton {
                icon: "\uE72A" // Forward
                size: "sm"
                tooltip: qsTr("Forward")
                onClicked: bubbleItem.forwardRequested(bubbleItem.messageId)
            }

            DsIconButton {
                visible: bubbleItem.text.length > 0
                icon: "\uE8C8" // Copy
                size: "sm"
                tooltip: qsTr("Copy")
                onClicked: BeyGramApp.copyText(bubbleItem.text)
            }

            DsIconButton {
                visible: bubbleItem.outgoing && !bubbleItem.pending && !bubbleItem.failed
                         && bubbleItem.text.length > 0
                icon: "\uE70F" // Edit
                size: "sm"
                tooltip: qsTr("Edit")
                onClicked: bubbleItem.editRequested(bubbleItem.messageId, bubbleItem.text)
            }
        }
    }

    DsMenu {
        id: menu

        DsMenuItem {
            text: qsTr("Reply")
            glyph: "\u21A9"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.replyRequested(bubbleItem.messageId, bubbleItem.text)
        }
        DsMenuItem {
            text: qsTr("Add reaction")
            glyph: "\u263A"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.addReactionRequested(bubbleItem.messageId, bubble)
        }
        DsMenuItem {
            text: qsTr("Forward")
            glyph: "\u21AA"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.forwardRequested(bubbleItem.messageId)
        }
        DsMenuItem {
            text: qsTr("Copy text")
            glyph: "\u29C9"
            enabled: bubbleItem.text.length > 0
            onClicked: BeyGramApp.copyText(bubbleItem.text)
        }
        DsMenuItem {
            text: qsTr("Edit")
            glyph: "\u270E"
            visible: bubbleItem.outgoing && !bubbleItem.pending && !bubbleItem.failed
            onClicked: bubbleItem.editRequested(bubbleItem.messageId, bubbleItem.text)
        }
        DsMenuItem {
            text: bubbleItem.selected ? qsTr("Deselect") : qsTr("Select")
            glyph: "\u2611"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.selectionToggled(bubbleItem.messageId, !bubbleItem.selected)
        }
        DsMenuItem {
            text: qsTr("Delete")
            glyph: "\u2715"
            tone: "danger"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.deleteRequested(bubbleItem.messageId)
        }
    }
}
