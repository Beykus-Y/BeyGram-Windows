import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Titled group for settings and detail pages: a header row (title, optional
// description, optional trailing action) followed by the section body.
ColumnLayout {
    id: section

    property string title
    property string description: ""
    // Right-aligned header item, for example a button or a chip.
    property alias trailing: trailingSlot.data
    // Section body; assign one item or a list of items.
    property alias content: body.data

    spacing: Design.space2

    RowLayout {
        id: headerRow
        Layout.fillWidth: true
        spacing: Design.space2
        visible: section.title.length > 0
                 || section.description.length > 0
                 || trailingSlot.children.length > 0

        ColumnLayout {
            id: heading
            Layout.fillWidth: true
            spacing: Design.space1

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: section.title
                color: BeyGramTheme.text
                font.pixelSize: Design.fontBodyLarge
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: text.length > 0
                text: section.description
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                lineHeight: Design.lineNormal
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: trailingSlot
            Layout.alignment: Qt.AlignTop
            spacing: Design.space2
        }
    }

    ColumnLayout {
        id: body
        Layout.fillWidth: true
        visible: body.children.length > 0
        spacing: Design.space2
    }
}
