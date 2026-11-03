import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: bubbleItem
    property int messageId: 0
    property string text: ""
    property string textHtml: ""
    property string replyPreview: ""
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
    signal editRequested(int messageId, string text)
    signal deleteRequested(int messageId)
    signal mediaOpenRequested(int fileId)
    signal replyRequested(int messageId, string preview)

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
        return text
    }

    function mediaHeightForWidth(width) {
        if (mediaWidth <= 0 || mediaHeight <= 0)
            return Math.round(width * 0.66)
        return Math.round(width * mediaHeight / mediaWidth)
    }

    Rectangle {
        id: bubble
        width: Math.min(bubbleItem.width * 0.72, content.implicitWidth + 26)
        height: content.implicitHeight + 18
        anchors.right: bubbleItem.outgoing ? bubbleItem.right : undefined
        anchors.left: bubbleItem.outgoing ? undefined : bubbleItem.left
        anchors.rightMargin: 14
        anchors.leftMargin: 14
        radius: 14
        color: bubbleItem.outgoing ? BeyGramTheme.bubbleOut : BeyGramTheme.surface2
        border.width: 1
        border.color: BeyGramTheme.line
        opacity: bubbleItem.failed ? 0.8 : 1.0

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 11
            spacing: 3

            Text {
                Layout.fillWidth: true
                visible: !bubbleItem.outgoing && bubbleItem.senderName !== ""
                text: bubbleItem.senderName
                color: BeyGramTheme.accentInk
                font.pixelSize: 10
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
                    font.pixelSize: 10
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
                radius: 10
                clip: true
                color: Qt.alpha(BeyGramTheme.surface2, 0.6)

                Image {
                    anchors.fill: parent
                    source: bubbleItem.mediaReady && bubbleItem.mediaFileId > 0
                            ? "image://beygrammedia/" + bubbleItem.mediaFileId
                              + "?v=" + bubbleItem.mediaRevision
                            : ""
                    cache: false
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
                    onClicked: bubbleItem.mediaOpenRequested(
                                   bubbleItem.mediaFullFileId > 0
                                   ? bubbleItem.mediaFullFileId
                                   : bubbleItem.mediaFileId)
                }
            }

            Text {
                Layout.fillWidth: true
                visible: bubbleItem.displayText().length > 0
                text: bubbleItem.textHtml.length > 0
                      ? bubbleItem.textHtml
                      : bubbleItem.displayText()
                color: BeyGramTheme.text
                font.pixelSize: 12
                wrapMode: Text.Wrap
                lineHeight: 1.3
                textFormat: bubbleItem.textHtml.length > 0 ? Text.RichText
                                                           : Text.PlainText
                onLinkActivated: (link) => BeyGramApp.openLink(link)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                Text {
                    Layout.fillWidth: true
                    text: bubbleItem.contentType !== "text" && bubbleItem.contentType !== "service"
                          && !bubbleItem.text.startsWith("[")
                          ? "[" + bubbleItem.contentType + "]"
                          : ""
                    color: BeyGramTheme.muted
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
                Text {
                    visible: bubbleItem.pending
                    text: "sending…"
                    color: BeyGramTheme.muted
                    font.pixelSize: 9
                }
                Text {
                    visible: bubbleItem.failed
                    text: "failed"
                    color: BeyGramTheme.danger
                    font.pixelSize: 9
                }
                Text {
                    visible: bubbleItem.edited
                    text: "edited"
                    color: BeyGramTheme.muted
                    font.pixelSize: 9
                }
                Text {
                    text: bubbleItem.timeText()
                    color: BeyGramTheme.muted
                    font.pixelSize: 9
                }
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

        MenuItem {
            text: "Reply"
            enabled: bubbleItem.messageId > 0
            onClicked: bubbleItem.replyRequested(bubbleItem.messageId, bubbleItem.text)
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
