import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Surface container for the design system. Groups related content behind a
// one-pixel hairline, optionally raised one surface step above the page.
Rectangle {
    id: card

    // Assign one item or a list of items; they are stacked vertically.
    property alias content: inner.data
    property int padding: Design.space4
    property bool elevated: false
    property bool interactive: false
    property alias spacing: inner.spacing

    signal clicked()

    radius: Design.radiusLg
    color: card.elevated ? BeyGramTheme.surface2 : BeyGramTheme.surface
    border.width: 1
    border.color: BeyGramTheme.line
    implicitWidth: inner.implicitWidth + card.padding * 2
    implicitHeight: inner.implicitHeight + card.padding * 2

    Behavior on color {
        ColorAnimation { duration: Design.motionNormal }
    }

    // Hover wash, painted beneath the content so text keeps its contrast.
    Rectangle {
        id: hoverWash
        anchors.fill: parent
        radius: card.radius
        color: Qt.alpha(BeyGramTheme.text, Design.opacityHover)
        opacity: card.interactive && hoverArea.containsMouse ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: Design.motionFast
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

    // Only swallows input while the card is interactive, so buttons and
    // fields inside a plain card keep working normally.
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: card.interactive
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
    }
}
