import QtQuick
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

/// Static sticker thumbnail for a message bubble.
///
/// Animated playback is not implemented yet; only the static full-size image
/// or its thumbnail is shown. The `animated` flag only drives a small badge and
/// does not start any animation. The component keeps its original API
/// (fileId / thumbnailFileId / ready / revision / animated / emojiSize and the
/// clicked signal); only the surface and typography now come from the DS.
Item {
    id: root

    property int fileId: 0
    property int thumbnailFileId: 0
    property bool ready: false
    property int revision: 0
    property bool animated: false
    property int emojiSize: 128
    /// Emoji fallback shown while the sticker file is not available. Telegram
    /// always sends one with the sticker, so the bubble is never blank.
    property string emoji: ""

    signal clicked()

    readonly property int activeFileId: ready && fileId > 0
                                          ? fileId
                                          : (thumbnailFileId > 0 ? thumbnailFileId : 0)
    readonly property string mediaSource: activeFileId > 0
                                          ? "image://beygrammedia/" + activeFileId
                                            + "?v=" + revision
                                          : ""

    implicitWidth: Math.min(160, Math.max(48, emojiSize))
    implicitHeight: implicitWidth

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Sticker")
    Accessible.onPressAction: root.clicked()

    Rectangle {
        id: placeholder
        anchors.fill: parent
        radius: Design.radiusMd
        color: Qt.alpha(BeyGramTheme.surface2, 0.55)
        visible: image.status !== Image.Ready

        Text {
            anchors.centerIn: parent
            width: Math.max(0, parent.width - Design.space3)
            text: root.emoji.length > 0 ? root.emoji : qsTr("[Sticker]")
            color: root.emoji.length > 0 ? BeyGramTheme.text : BeyGramTheme.muted
            font.pixelSize: root.emoji.length > 0
                            ? Math.max(24, Math.min(root.width, root.height) * 0.62)
                            : Design.fontCaption
            horizontalAlignment: Text.AlignHCenter
            elide: root.emoji.length > 0 ? Text.ElideNone : Text.ElideRight
        }
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.mediaSource
        sourceSize: Qt.size(256, 256)
        cache: true
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        visible: status === Image.Ready
    }

    DsBadge {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Design.space1
        text: qsTr("ANIM")
        tone: "muted"
        visible: root.animated
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
