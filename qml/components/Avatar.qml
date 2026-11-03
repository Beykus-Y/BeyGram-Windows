import QtQuick
import BeyGram.Theme 1.0

/// Circular avatar.
///
/// The media image provider applies the circular alpha mask when the URL asks
/// for it, so the photo is a real circle rather than a square clipped by a
/// rounded Rectangle (Qt Quick's `clip` is always rectangular).
Item {
    id: root

    property int size: 46
    property int fileId: 0
    property bool ready: false
    property int revision: 0
    property string initials: "?"
    property color tint: BeyGramTheme.accentSoft
    property color ink: BeyGramTheme.accentInk

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.tint
    }

    Text {
        anchors.centerIn: parent
        visible: image.status !== Image.Ready
        text: root.initials
        color: root.ink
        font.pixelSize: Math.round(root.size * 0.40)
        font.weight: Font.DemiBold
    }

    Image {
        id: image
        anchors.fill: parent
        source: root.ready && root.fileId > 0
                ? "image://beygrammedia/" + root.fileId + "?v=" + root.revision
                  + "&round=1"
                : ""
        sourceSize: Qt.size(root.size, root.size)
        cache: true
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready
    }
}
