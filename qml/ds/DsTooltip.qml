import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

// Styled tooltip. Declare it inside the item it describes:
//
//   DsTooltip {
//       text: qsTr("Delete message")
//       visible: deleteButton.hovered
//   }
ToolTip {
    id: tooltip

    delay: 500
    padding: Design.space2
    font.pixelSize: Design.fontCaption

    contentItem: Text {
        text: tooltip.text
        font: tooltip.font
        color: BeyGramTheme.text
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: BeyGramTheme.surface2
        radius: Design.radiusXs
        border.width: 1
        border.color: BeyGramTheme.line
    }
}
