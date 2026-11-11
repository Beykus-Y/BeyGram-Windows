import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: bubbleItem
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
    signal editRequested(var messageId, string text)
    signal deleteRequested(var messageId)
    signal mediaOpenRequested(int fileId)
    signal mediaPlayRequested(int fileId)
    signal reactionRequested(string emoji)
    signal forwardRequested(var messageId)
    signal pollOptionSelected(int index)

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
    signal replyRequested(var messageId, string preview)

    implicitHeight: bubble.height + 8
    width: ListView.view ? ListView.view.width : parent.width

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
        width: Math.min(420, Math.min(bubbleItem.width * 0.78, content.implicitWidth + 26))
        height: content.implicitHeight + 14
        anchors.right: bubbleItem.outgoing ? bubbleItem.right : undefined
        anchors.left: bubbleItem.outgoing ? undefined : bubbleItem.left
        anchors.rightMargin: 14
        anchors.leftMargin: 14
        radius: 20
        bottomRightRadius: bubbleItem.outgoing ? 6 : 20
        bottomLeftRadius: bubbleItem.outgoing ? 20 : 6
        color: bubbleItem.outgoing ? BeyGramTheme.bubbleOut : BeyGramTheme.surface
        border.width: 0
        opacity: bubbleItem.failed ? 0.8 : 1.0

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 8
            anchors.bottomMargin: 6
            spacing: 3

            Text {
                Layout.fillWidth: true
                visible: !bubbleItem.outgoing && bubbleItem.senderName !== ""
                text: bubbleItem.senderName
                color: BeyGramTheme.accentInk
                font.pixelSize: 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                visible: bubbleItem.replyPreview.length > 0
                implicitHeight: quoteText.implicitHeight + 10
                radius: 8
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
                    font.pixelSize: 11
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredWidth: Math.min(320, bubbleItem.mediaWidth > 0
                                                ? bubbleItem.mediaWidth : 240)
                Layout.preferredHeight: bubbleItem.mediaHeightForWidth(
                                            Math.min(320, bubbleItem.mediaWidth > 0
                                                     ? bubbleItem.mediaWidth : 240))
                visible: bubbleItem.mediaFileId > 0
                         && bubbleItem.contentType !== "sticker"
                radius: 10
                clip: true
                color: Qt.alpha(BeyGramTheme.surface2, 0.6)

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
                BusyIndicator {
                    anchors.centerIn: parent
                    running: bubbleItem.mediaFileId > 0
                             && !bubbleItem.mediaReady
                    visible: running
                    implicitWidth: 24
                    implicitHeight: 24
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
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
                spacing: 8
                visible: bubbleItem.contentType === "voice"
                         || bubbleItem.contentType === "audio"
                         || bubbleItem.contentType === "video_note"

                PrimaryButton {
                    text: "Play"
                    implicitHeight: 30
                    enabled: bubbleItem.mediaFullFileId > 0
                    onClicked: bubbleItem.mediaPlayRequested(bubbleItem.mediaFullFileId)
                }
                Text {
                    Layout.fillWidth: true
                    text: bubbleItem.contentType === "voice" ? "Voice message"
                          : bubbleItem.contentType === "video_note" ? "Video message"
                          : "Audio"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }
            }

            Text {
                Layout.fillWidth: true
                visible: bubbleItem.displayText().length > 0
                text: bubbleItem.textHtml.length > 0
                      ? bubbleItem.textHtml
                      : bubbleItem.displayText()
                color: BeyGramTheme.text
                font.pixelSize: 15
                wrapMode: Text.Wrap
                lineHeight: 1.35
                textFormat: bubbleItem.textHtml.length > 0 ? Text.RichText
                                                           : Text.PlainText
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Item { Layout.fillWidth: true }

                Text {
                    visible: bubbleItem.pending
                    text: "sending..."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }
                Text {
                    visible: bubbleItem.failed
                    text: "Not sent"
                    color: BeyGramTheme.danger
                    font.pixelSize: 11
                }
                Text {
                    visible: bubbleItem.edited
                    text: "edited"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                }
                Text {
                    text: bubbleItem.timeText()
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
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

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: (mouse) => menu.popup(bubbleItem, mouse.x, mouse.y)
        }
    }

    Menu {
        id: menu
        width: 190
        padding: 4
        palette.window: BeyGramTheme.surface
        palette.windowText: BeyGramTheme.text
        palette.text: BeyGramTheme.text
        palette.buttonText: BeyGramTheme.text
        palette.highlight: BeyGramTheme.accentSoft
        palette.highlightedText: BeyGramTheme.accentInk

        background: Rectangle {
            implicitWidth: 190
            color: BeyGramTheme.surface
            border.width: 1
            border.color: BeyGramTheme.line
            radius: 8
        }

        MenuItem {
            text: "Reply"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.replyRequested(bubbleItem.messageId, bubbleItem.text)
        }
        MenuItem {
            text: "Forward"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.forwardRequested(bubbleItem.messageId)
        }
        MenuItem {
            text: "Copy text"
            enabled: bubbleItem.text.length > 0
            onClicked: BeyGramApp.copyText(bubbleItem.text)
        }
        MenuItem {
            text: "Edit"
            visible: bubbleItem.outgoing && !bubbleItem.pending && !bubbleItem.failed
            onClicked: bubbleItem.editRequested(bubbleItem.messageId, bubbleItem.text)
        }
        MenuSeparator {}
        MenuItem {
            text: "Delete"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.deleteRequested(bubbleItem.messageId)
        }
    }
}
