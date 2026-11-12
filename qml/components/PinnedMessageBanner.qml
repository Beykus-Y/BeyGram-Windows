// PinnedMessageBanner — slim strip shown above the message list for the chat's
// pinned message.
//
// Presentation only: the parent loads the pinned message (title/preview) and
// performs the unpin request when `unpinRequested` fires. It holds no
// C++/network/logging state of its own.
//
// The strip is 40 px tall on `surface2`, marked by a 3 px accent rail and a
// single 12 px label. `preview` is what the user actually reads; `title` is the
// fallback for messages without text (media, service messages) and the spoken
// name of the strip.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: banner

    property string title: "Pinned message"
    property string preview: ""
    signal activated()
    signal unpinRequested()

    // Height is fixed; `visible` is Item's own property (default overridden
    // below) so a parent can bind it to whether a pinned message exists.
    implicitHeight: 40
    height: 40
    visible: false
    clip: true

    readonly property bool _hasPreview: banner.preview.length > 0
    readonly property string _label: _hasPreview ? banner.preview : banner.title

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 0
        color: BeyGramTheme.surface2

        // Hover wash, painted beneath the content so the label keeps contrast.
        Rectangle {
            anchors.fill: parent
            color: Qt.alpha(BeyGramTheme.text, Design.opacityHover)
            opacity: activateArea.containsMouse ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation {
                    duration: Design.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }

        // Hairline against the message list below.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: BeyGramTheme.line
        }
    }

    // Accent rail marking the strip as the pinned-message affordance.
    Rectangle {
        id: accentBar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: BeyGramTheme.accent
    }

    // Whole-strip activation target; declared before the content so the unpin
    // button (drawn above) keeps its own clicks.
    MouseArea {
        id: activateArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: banner.activated()

        Accessible.role: Accessible.Button
        Accessible.name: banner._hasPreview
                         ? banner.title + ": " + banner.preview
                         : banner.title
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Design.space3
        anchors.rightMargin: Design.space1
        spacing: Design.space2

        DsIcon {
            Layout.alignment: Qt.AlignVCenter
            name: "\uE718" // Segoe MDL2 Assets: Pin
            size: 14
            color: BeyGramTheme.accentInk
            filled: true
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: banner._label
            color: BeyGramTheme.text
            font.pixelSize: Design.fontSmall
            font.weight: Font.Medium
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
            Accessible.ignored: true
        }

        DsIconButton {
            Layout.alignment: Qt.AlignVCenter
            icon: "\uE711" // Segoe MDL2 Assets: Cancel
            size: "sm"
            tooltip: qsTr("Unpin message")
            onClicked: banner.unpinRequested()
        }
    }
}
