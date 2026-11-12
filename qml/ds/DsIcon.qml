import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

// DsIcon — a single glyph rendered as text.
//
// Glyphs come from the Windows symbol font, so no image assets or icon files
// are needed. `name` takes the code point, for example "\uE710".
Text {
    id: icon

    property string name: ""
    property real size: Design.iconSize
    property bool filled: false

    // `color` is inherited from Text and defaults to the theme's primary ink.
    text: icon.name
    color: BeyGramTheme.text
    font.family: "Segoe MDL2 Assets"
    font.pixelSize: Math.max(1, Math.round(icon.size))
    opacity: icon.filled ? 1.0 : Design.opacitySecondary

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    textFormat: Text.PlainText
    wrapMode: Text.NoWrap

    // The glyph is decorative; the surrounding control carries the accessible
    // name. Callers can opt back in with `Accessible.ignored: false`.
    Accessible.ignored: true

    Behavior on color {
        ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
    }
    Behavior on opacity {
        NumberAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
    }
}
