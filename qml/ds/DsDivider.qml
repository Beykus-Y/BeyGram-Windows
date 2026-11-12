import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

// One-pixel separator.
//
// Horizontal dividers fill the available width and are 1 px tall; vertical
// dividers fill the available height and are 1 px wide. `inset` trims the
// start edge (left for horizontal, top for vertical).
Item {
    id: divider

    property int inset: 0
    property bool vertical: false

    implicitWidth: vertical ? 1 : 0
    implicitHeight: vertical ? 0 : 1

    Layout.fillWidth: !vertical
    Layout.fillHeight: vertical

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: divider.vertical ? 0 : divider.inset
        anchors.topMargin: divider.vertical ? divider.inset : 0
        color: BeyGramTheme.line
    }
}
