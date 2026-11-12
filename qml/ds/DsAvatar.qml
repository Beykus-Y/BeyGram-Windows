// DsAvatar.qml — circular avatar with a photo, an initials fallback and an
// optional presence dot.
//
// The six accentIndex colours mirror AvatarPalette in the Android design
// system (ui/theme/Color.kt) so a person keeps the same colour in both
// clients. Photos are masked to a real circle with MultiEffect: Qt Quick's
// `clip` follows an item's rectangular bounds, not its rounded corners.

import QtQuick
import QtQuick.Effects
import BeyGram.Theme 1.0
import "design.js" as Design

Item {
    id: root

    property string initials: "?"
    property string imageSource: ""
    property int size: Design.avatarMedium
    property int accentIndex: 0
    property bool online: false
    property bool muted: false

    readonly property bool hasImage: root.imageSource.length > 0
                                      && photo.status === Image.Ready

    // AvatarPalette from the Android design system.
    readonly property var avatarPalette: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    // Presence dot: 22% of the avatar, centred on the circle's edge at 45°
    // rather than floating in the bounding box corner.
    readonly property int _presenceSize: Math.max(8, Math.round(root.size * 0.22))
    readonly property int _presenceOffset: Math.round(root.size / 2
                                                      + (root.size / 2) * Math.SQRT1_2
                                                      - _presenceSize / 2)

    function paletteColor(index) {
        return root.avatarPalette[Math.abs(index) % root.avatarPalette.length]
    }

    implicitWidth: size
    implicitHeight: size
    opacity: muted ? Design.opacitySecondary : 1.0

    Behavior on opacity {
        NumberAnimation {
            duration: Design.motionFast
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: root.paletteColor(root.accentIndex)
        antialiasing: true
    }

    Text {
        anchors.centerIn: parent
        visible: !root.hasImage
        width: Math.round(root.size * 0.76)
        text: root.initials
        color: "white"
        font.pixelSize: Math.round(root.size * 0.36)
        font.weight: Font.Bold
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    Image {
        id: photo
        anchors.fill: parent
        source: root.imageSource
        sourceSize: Qt.size(root.size, root.size)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        mipmap: true
        layer.enabled: true
        visible: false
    }

    Item {
        id: photoMask
        anchors.fill: parent
        layer.enabled: true
        visible: false

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "white"
            antialiasing: true
        }
    }

    MultiEffect {
        anchors.fill: parent
        source: photo
        maskSource: photoMask
        maskEnabled: true
        visible: root.hasImage
    }

    Rectangle {
        width: root._presenceSize
        height: width
        radius: width / 2
        x: root._presenceOffset
        y: root._presenceOffset
        visible: root.online
        color: BeyGramTheme.okInk
        border.width: root.size >= 40 ? 2 : 1
        border.color: BeyGramTheme.bg
        antialiasing: true
    }
}
