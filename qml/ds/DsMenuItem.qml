import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// One row inside a DsMenu. A glyph and an optional keyboard hint flank the
// label; the "danger" tone tints the row for destructive actions.
//
// The glyph is exposed as "glyph" because MenuItem.icon is a final grouped
// property (QQuickIcon) that cannot be redeclared on a MenuItem.
MenuItem {
    id: control

    property string glyph: ""
    property string tone: "normal" // normal | danger
    property string shortcutText: ""

    readonly property bool danger: control.tone === "danger"

    implicitHeight: 34
    leftPadding: Design.space3
    rightPadding: Design.space3
    topPadding: 0
    bottomPadding: 0
    spacing: Design.space2
    hoverEnabled: true
    opacity: enabled ? 1.0 : Design.opacityDisabled

    Accessible.role: Accessible.MenuItem
    Accessible.name: control.text
    Accessible.description: control.shortcutText

    contentItem: RowLayout {
        spacing: control.spacing

        Text {
            visible: control.glyph.length > 0
            Layout.alignment: Qt.AlignVCenter
            text: control.glyph
            color: control.danger ? BeyGramTheme.danger : BeyGramTheme.muted
            font.pixelSize: 16
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.text
            textFormat: Text.PlainText
            color: control.danger ? BeyGramTheme.danger : BeyGramTheme.text
            font.pixelSize: 13
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            visible: control.shortcutText.length > 0
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: Design.space3
            text: control.shortcutText
            textFormat: Text.PlainText
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        radius: Design.radiusSm
        color: !control.enabled ? "transparent"
               : control.down ? Qt.darker(BeyGramTheme.surface2, 1.06)
               : (control.hovered || control.highlighted) ? BeyGramTheme.surface2
               : "transparent"

        Behavior on color { ColorAnimation { duration: Design.motionFast } }
    }
}
