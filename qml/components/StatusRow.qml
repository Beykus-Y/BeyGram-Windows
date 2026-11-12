import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

/// One labelled status line: a tone dot, the label, the value on the right and
/// an optional wrapped detail underneath.
///
/// The line itself is a DsListRow so it matches every other list row; the
/// detail is a plain Text below it because DsListRow's subtitle is a single
/// elided line and these details are full sentences.
Item {
    id: row

    property string label: ""
    property string value: ""
    property string detail: ""
    property string tone: "neutral" // neutral | ok | warn | danger | accent

    implicitWidth: 320
    implicitHeight: layout.implicitHeight

    Accessible.role: Accessible.StaticText
    Accessible.name: row.value.length > 0 ? row.label + ": " + row.value : row.label
    Accessible.description: row.detail

    /// The tone colours both the dot and the value.
    readonly property color toneColor: {
        if (row.tone === "ok")
            return BeyGramTheme.okInk
        if (row.tone === "warn")
            return BeyGramTheme.warnInk
        if (row.tone === "danger")
            return BeyGramTheme.danger
        if (row.tone === "accent")
            return BeyGramTheme.accent
        return BeyGramTheme.muted
    }

    ColumnLayout {
        id: layout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Design.space1

        DsListRow {
            Layout.fillWidth: true
            // Status lines are read-only, so no hover tint or pointing hand.
            hoverEnabled: false
            title: row.label

            leading: Rectangle {
                width: 8
                height: 8
                radius: 4
                color: row.toneColor
                antialiasing: true

                Behavior on color {
                    ColorAnimation {
                        duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                        easing.type: Easing.OutCubic
                    }
                }
            }

            trailing: Text {
                // Keep a long value from squeezing the label out of the row.
                width: row.width > 0 ? Math.min(implicitWidth, row.width * 0.5) : implicitWidth
                text: row.value
                color: row.toneColor
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight

                Behavior on color {
                    ColorAnimation {
                        duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            // Align with the title: 12 px padding + 8 px dot + 12 px spacing.
            Layout.leftMargin: Design.space8
            Layout.rightMargin: Design.space3
            visible: row.detail.length > 0
            text: row.detail
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            lineHeight: Design.lineNormal
            wrapMode: Text.WordWrap
        }
    }
}
