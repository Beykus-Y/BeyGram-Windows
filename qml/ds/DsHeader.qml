import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Page header used at the top of a page or conversation pane.
Rectangle {
    id: header

    property string title: ""
    property string subtitle: ""
    property bool bordered: true
    property bool translucent: false

    property alias leading: leadingSlot.data
    property alias trailing: trailingSlot.data

    implicitHeight: Design.headerHeight
    height: Design.headerHeight
    color: translucent
           ? Qt.alpha(BeyGramTheme.surface,
                      BeyGramAppearance.designStyle === "LiquidGlass" ? 0.82 : 0.94)
           : "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: BeyGramTheme.line
        visible: header.bordered
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Design.space4
        anchors.rightMargin: Design.space4
        spacing: Design.space3

        Row {
            id: leadingSlot
            Layout.alignment: Qt.AlignVCenter
            spacing: Design.space1
            visible: children.length > 0
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: header.title
                color: BeyGramTheme.text
                font.pixelSize: Design.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                visible: text.length > 0
            }

            Text {
                Layout.fillWidth: true
                text: header.subtitle
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        Row {
            id: trailingSlot
            Layout.alignment: Qt.AlignVCenter
            spacing: Design.space1
            visible: children.length > 0
        }
    }
}
