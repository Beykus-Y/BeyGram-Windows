import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

// Rounded context menu surface shared by every BeyGram menu.
//
// Elevation is painted with translucent rounded outlines instead of
// Qt5Compat's DropShadow, so the component stays plain Qt 6. The menu is
// rendered in-scene (Popup.Item) so the shadow can extend past the surface;
// the default Popup.Window would clip it at the window edge.
Menu {
    id: menu

    default property alias items: menu.contentData

    readonly property int minimumWidth: 220

    // Menu's ListView content item does not report an implicit width, so the
    // widest row is measured here instead of relying on contentWidth.
    readonly property real itemsWidth: {
        let widest = 0
        for (let i = 0; i < menu.count; ++i) {
            const item = menu.itemAt(i)
            if (item && item.visible)
                widest = Math.max(widest, item.implicitWidth)
        }
        return widest
    }

    implicitWidth: Math.max(minimumWidth,
                            implicitBackgroundWidth + leftInset + rightInset,
                            itemsWidth + leftPadding + rightPadding)
    padding: Design.space1
    popupType: Popup.Item

    // popup() opens at the mouse cursor on desktop platforms; this helper makes
    // that intent explicit at call sites.
    function openAtCursor() {
        popup()
    }

    background: Item {
        // Three rounded outlines, each a little wider and fainter than the one
        // inside it, read as a soft drop shadow below and around the surface.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -6
            anchors.bottomMargin: -10
            radius: Design.radiusMd + 6
            color: "transparent"
            border.width: 6
            border.color: Qt.rgba(0, 0, 0, BeyGramTheme.darkMode ? 0.18 : 0.05)
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            anchors.bottomMargin: -6
            radius: Design.radiusMd + 3
            color: "transparent"
            border.width: 4
            border.color: Qt.rgba(0, 0, 0, BeyGramTheme.darkMode ? 0.26 : 0.09)
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            anchors.bottomMargin: -3
            radius: Design.radiusMd + 1
            color: "transparent"
            border.width: 3
            border.color: Qt.rgba(0, 0, 0, BeyGramTheme.darkMode ? 0.34 : 0.13)
        }

        Rectangle {
            anchors.fill: parent
            radius: Design.radiusMd
            color: BeyGramTheme.surface
            border.width: 1
            border.color: BeyGramTheme.line

            Behavior on color { ColorAnimation { duration: Design.motionFast } }
        }
    }
}
