// PinnedMessageBanner — slim strip shown above the message list for the chat's
// pinned message. This component is presentation only: the parent loads the
// pinned message (title/preview) and performs the unpin request when
// `unpinRequested` fires. It holds no C++/network/logging state of its own.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: banner

    property string title: "Pinned message"
    property string preview: ""
    signal activated()
    signal unpinRequested()

    // Height is fixed; `visible` is Item's own property (default overridden
    // below) so a parent can bind it to whether a pinned message exists.
    height: 42
    implicitHeight: 42
    visible: false
    clip: true

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 0
        color: BeyGramTheme.surface2
        border.width: 0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: BeyGramTheme.line
        }
    }

    // Left accent bar.
    Rectangle {
        id: accentBar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 0
        color: BeyGramTheme.accentInk
    }

    // Whole-banner activation target; declared before the content so the
    // unpin button (drawn above) keeps its own clicks.
    MouseArea {
        id: activateArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: banner.activated()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        anchors.topMargin: 0
        anchors.bottomMargin: 0
        spacing: 9

        Text {
            id: pinGlyph
            Layout.alignment: Qt.AlignVCenter
            text: "PIN"
            color: BeyGramTheme.accentInk
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: banner.title
                color: BeyGramTheme.muted
                font.pixelSize: 10
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                maximumLineCount: 1
                wrapMode: Text.NoWrap
            }

            Text {
                Layout.fillWidth: true
                text: banner.preview
                color: BeyGramTheme.text
                font.pixelSize: 12
                elide: Text.ElideRight
                maximumLineCount: 1
                wrapMode: Text.NoWrap
            }
        }

        ToolButton {
            id: unpinButton
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 24
            implicitHeight: 24
            padding: 0
            focusPolicy: Qt.NoFocus
            onClicked: banner.unpinRequested()

            background: Rectangle {
                radius: 12
                color: unpinButton.hovered ? Qt.alpha(BeyGramTheme.accentInk, 0.12)
                                           : "transparent"
            }

            contentItem: Text {
                text: "\u2715"
                color: unpinButton.hovered ? BeyGramTheme.text : BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Unpin message")
            ToolTip.delay: 600
        }
    }
}
