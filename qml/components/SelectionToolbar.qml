// SelectionToolbar — compact action strip shown while a message selection is
// active.
//
// Presentation only. All state lives in the injected `facade`
// (beygram::ui::SelectionFacade): the strip reads its count, asks it to open
// the forward picker and clears the selection through it. Nothing here touches
// TDLib, the network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: toolbar

    /// The SelectionFacade driving this toolbar. May be null while the page is
    /// still being constructed.
    property var facade

    readonly property int selectedCount: toolbar.facade ? toolbar.facade.count : 0
    readonly property bool hasSelection: toolbar.selectedCount > 0

    visible: toolbar.facade ? toolbar.facade.active : false
    implicitHeight: 48
    height: 48
    color: BeyGramTheme.surface2
    clip: true

    // Separator against the content above, matching the muted line used by the
    // pinned message banner.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: BeyGramTheme.line
    }

    // Compact flat action button shared by both actions.
    component ToolbarAction: Button {
        id: action

        implicitHeight: 28
        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        leftPadding: 10
        rightPadding: 10
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        opacity: enabled ? 1.0 : 0.45

        Accessible.role: Accessible.Button
        Accessible.name: action.text

        contentItem: Text {
            text: action.text
            color: BeyGramTheme.text
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 9
            color: !action.enabled ? "transparent"
                 : action.pressed ? Qt.alpha(BeyGramTheme.accentInk, 0.18)
                 : action.hovered ? Qt.alpha(BeyGramTheme.accentInk, 0.10)
                 : "transparent"
            border.width: 1
            border.color: action.enabled ? Qt.alpha(BeyGramTheme.line, 1.0)
                                         : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        spacing: 8

        Text {
            id: countLabel
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.maximumWidth: Math.max(0, toolbar.width * 0.5)
            text: qsTr("%1 selected").arg(toolbar.selectedCount)
            color: BeyGramTheme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }

        ToolbarAction {
            id: forwardButton
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Forward")
            enabled: toolbar.hasSelection
            onClicked: {
                if (toolbar.facade)
                    toolbar.facade.beginForward();
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Forward selected messages")
            ToolTip.delay: 600
        }

        ToolbarAction {
            id: cancelButton
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Cancel")
            enabled: toolbar.facade !== null
            onClicked: {
                if (toolbar.facade)
                    toolbar.facade.clear();
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Clear selection")
            ToolTip.delay: 600
        }
    }
}
