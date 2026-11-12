import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Surface container for the design system. Adapts smoothly to the active
// DesignStyle (LiquidGlass specular sheen, Signature warm/accent rim,
// Material3 softer corners) and maintains clear depth even on AMOLED.
Rectangle {
    id: card

    property alias content: inner.data
    property int padding: Design.space4
    property bool elevated: false
    property bool interactive: false
    property alias spacing: inner.spacing

    signal clicked()

    readonly property bool _glass: BeyGramAppearance.designStyle === "LiquidGlass"
    readonly property bool _signature: BeyGramAppearance.designStyle === "Signature"
    readonly property bool _amoled: BeyGramTheme.modeName === "Amoled"

    radius: Design.cardRadius(BeyGramAppearance.designStyle)
    color: {
        var base = card.elevated ? BeyGramTheme.surface2 : BeyGramTheme.surface
        if (_glass)
            return Qt.alpha(base, BeyGramTheme.darkMode ? 0.86 : 0.90)
        return base
    }
    border.width: 1
    border.color: {
        if (_glass)
            return Qt.alpha(BeyGramTheme.darkMode ? "#FFFFFF" : BeyGramTheme.line,
                            BeyGramTheme.darkMode ? 0.14 : 0.85)
        if (_signature)
            return Qt.alpha(BeyGramTheme.accent, BeyGramTheme.darkMode ? 0.24 : 0.20)
        if (_amoled)
            return Qt.lighter(BeyGramTheme.line, 1.35)
        return BeyGramTheme.line
    }
    implicitWidth: inner.implicitWidth + card.padding * 2
    implicitHeight: inner.implicitHeight + card.padding * 2

    Behavior on color {
        ColorAnimation {
            duration: Design.motion(Design.motionNormal, BeyGramAppearance.reduceMotion)
        }
    }

    // Subtle specular top edge matching Android's GlassSurface / SignatureChrome.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: card.radius * 0.65
        anchors.rightMargin: card.radius * 0.65
        anchors.topMargin: 1
        height: 1
        visible: card._glass || card._signature
        color: card._signature
               ? Qt.alpha(BeyGramTheme.accent, BeyGramTheme.darkMode ? 0.34 : 0.22)
               : Qt.alpha("#FFFFFF", BeyGramTheme.darkMode ? 0.16 : 0.55)
    }

    Rectangle {
        id: hoverWash
        anchors.fill: parent
        radius: card.radius
        color: Qt.alpha(BeyGramTheme.text, Design.opacityHover)
        opacity: card.interactive && hoverArea.containsMouse ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                easing.type: Easing.OutCubic
            }
        }
    }

    ColumnLayout {
        id: inner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: card.padding
        spacing: Design.space3
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: card.interactive
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
    }
}
