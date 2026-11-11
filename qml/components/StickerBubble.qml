import QtQuick
import BeyGram.Theme 1.0

/// Static sticker thumbnail for a message bubble.
///
/// Animated playback is not implemented yet; only the static full-size image
/// or its thumbnail is shown. The `animated` flag only drives a small badge and
/// does not start any animation.
Item {
    id: root

    property int fileId: 0
    property int thumbnailFileId: 0
    property bool ready: false
    property int revision: 0
    property bool animated: false
    property int emojiSize: 128

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

    Rectangle {
        id: placeholder
        anchors.fill: parent
        radius: 8
        color: Qt.alpha(BeyGramTheme.surface2, 0.55)
        visible: image.status !== Image.Ready

        Text {
            anchors.centerIn: parent
            width: parent.width - 12
            text: "[Sticker]"
            color: BeyGramTheme.muted
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
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

    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 3
        width: badge.implicitWidth + 8
        height: badge.implicitHeight + 2
        radius: height / 2
        color: Qt.alpha(BeyGramTheme.bg, 0.72)
        visible: root.animated

        Text {
            id: badge
            anchors.centerIn: parent
            text: "ANIM"
            color: BeyGramTheme.text
            font.pixelSize: 8
            font.weight: Font.DemiBold
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
