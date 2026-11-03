import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: bubbleItem
    property int messageId: 0
    property string text: ""
    property bool outgoing: false
    property string senderName: ""
    property var date: 0
    property bool pending: false
    property bool failed: false
    property bool edited: false
    property string contentType: "text"
    signal editRequested(int messageId, string text)
    signal deleteRequested(int messageId)

    implicitHeight: bubble.height + 8
    width: ListView.view ? ListView.view.width : parent.width

    function timeText() {
        if (!date || date <= 0)
            return ""
        return Qt.formatTime(new Date(date * 1000), "hh:mm")
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

            Text {
                Layout.fillWidth: true
                text: bubbleItem.text
                color: BeyGramTheme.text
                font.pixelSize: 12
                wrapMode: Text.Wrap
                lineHeight: 1.3
                textFormat: Text.PlainText
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
