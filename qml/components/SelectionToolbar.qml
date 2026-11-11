// SelectionToolbar — action strip shown while a message selection is active.
//
// Presentation only. The parent owns multi-select mode and the set of selected
// message ids; it binds `selectedCount` and turns the emitted signals into
// TDLib requests (forward / copy / delete) or leaves selection mode. Nothing in
// this file touches TDLib, the network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: toolbar

    /// Number of selected messages. Non-positive values keep the actions
    /// disabled; the parent normally hides the toolbar in that case.
    property int selectedCount: 0

    signal forwardRequested()
    signal copyRequested()
    signal deleteRequested()
    signal clearRequested()

    readonly property int count: Math.max(0, toolbar.selectedCount)
    readonly property bool hasSelection: toolbar.count > 0

    // Height is fixed. `visible` stays Item's own property (default false) so
    // the parent can bind it to whether selection mode is active.
    implicitHeight: 48
    height: 48
    visible: false
    clip: true

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 0
        color: BeyGramTheme.surface2

        // Separator against the content above (this strip sits over the
        // composer), matching the muted line used by the pinned banner.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: BeyGramTheme.line
        }
    }

    // Compact flat action button. Declared as an inline component so the three
    // actions share one look; instances pass their own enabled state.
    component ToolbarAction: Button {
        id: action

        property bool destructive: false

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
            color: action.destructive ? BeyGramTheme.danger : BeyGramTheme.text
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
        anchors.topMargin: 0
        anchors.bottomMargin: 0
        spacing: 8

        Text {
            id: countLabel
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: Math.max(0, toolbar.width * 0.4)
            text: qsTr("%1 selected").arg(toolbar.count)
            color: BeyGramTheme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }

        Item {
            Layout.fillWidth: true
        }

        ToolbarAction {
            id: forwardButton
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Forward")
            enabled: toolbar.hasSelection
            onClicked: toolbar.forwardRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Forward selected messages")
            ToolTip.delay: 600
        }

        ToolbarAction {
            id: copyButton
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Copy")
            enabled: toolbar.hasSelection
            onClicked: toolbar.copyRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Copy selected messages")
            ToolTip.delay: 600
        }

        ToolbarAction {
            id: deleteButton
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Delete")
            destructive: true
            enabled: toolbar.hasSelection
            onClicked: toolbar.deleteRequested()

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Delete selected messages")
            ToolTip.delay: 600
        }

        // The close affordance stays enabled even with an empty count so the
        // parent can always leave selection mode through it.
        ToolButton {
            id: closeButton
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 24
            implicitHeight: 24
            padding: 0
            focusPolicy: Qt.NoFocus
            onClicked: toolbar.clearRequested()

            background: Rectangle {
                radius: 12
                color: closeButton.hovered ? Qt.alpha(BeyGramTheme.accentInk, 0.12)
                                           : "transparent"
            }

            contentItem: Text {
                text: "\u2715"
                color: closeButton.hovered ? BeyGramTheme.text : BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Clear selection")
            ToolTip.delay: 600
        }
    }
}
