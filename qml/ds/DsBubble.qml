import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Presentation-only message bubble for the BeyGram design system.
// Respects BubbleStyle (Rounded | Soft | Square), message text scaling, and
// Android-aligned surface border treatment in AMOLED / LiquidGlass.
Item {
    id: root

    property bool outgoing: false
    property string text: ""
    property string timeText: ""
    property string senderName: ""
    /// Delivery state: "sent", "read" or "failed".
    property string status: "sent"
    property string replyPreview: ""
    property string reactions: ""

    readonly property int radiusMain: Design.bubbleRadiusFor(BeyGramAppearance.bubbleStyle)
    readonly property int radiusTail: Design.bubbleTailRadiusFor(BeyGramAppearance.bubbleStyle)

    readonly property real maxBubbleWidth: {
        const available = root.parent && root.parent.width > 0
                          ? root.parent.width : root.width
        return available * Design.bubbleMaxWidthRatio
    }

    readonly property string statusGlyph: status === "read" ? "✓✓"
                                            : status === "sent" ? "✓"
                                            : status === "failed" ? "!" : ""

    implicitWidth: bubble.width
    implicitHeight: bubble.height + Design.space2
    width: parent && parent.width > 0 ? parent.width : implicitWidth

    Accessible.role: Accessible.StaticText
    Accessible.name: {
        const parts = []
        if (senderName.length > 0)
            parts.push(senderName)
        if (text.length > 0)
            parts.push(text)
        if (timeText.length > 0)
            parts.push(timeText)
        return parts.join(", ")
    }

    Rectangle {
        id: bubble
        width: Math.min(root.maxBubbleWidth,
                        Math.max(82, content.implicitWidth + Design.space5))
        height: content.implicitHeight + Design.space3
        anchors.right: root.outgoing ? root.right : undefined
        anchors.left: root.outgoing ? undefined : root.left
        radius: root.radiusMain
        bottomRightRadius: root.outgoing ? root.radiusTail : root.radiusMain
        bottomLeftRadius: root.outgoing ? root.radiusMain : root.radiusTail
        color: {
            var base = root.outgoing ? BeyGramTheme.bubbleOut : BeyGramTheme.surface
            if (BeyGramAppearance.designStyle === "LiquidGlass")
                return Qt.alpha(base, BeyGramTheme.darkMode ? 0.88 : 0.92)
            return base
        }
        border.width: (!root.outgoing || BeyGramTheme.modeName === "Amoled"
                       || BeyGramAppearance.designStyle === "LiquidGlass") ? 1 : 0
        border.color: root.outgoing
                      ? Qt.alpha(BeyGramTheme.accent, 0.24)
                      : BeyGramTheme.line
        opacity: root.status === "failed" ? 0.85 : 1.0

        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 4

            Text {
                Layout.fillWidth: true
                visible: !root.outgoing && root.senderName.length > 0
                text: root.senderName
                color: BeyGramTheme.accentInk
                font.pixelSize: Design.fontSmall
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.replyPreview.length > 0
                implicitWidth: quoteText.implicitWidth + 18
                implicitHeight: quoteText.implicitHeight + 10
                radius: Design.radiusXs
                color: Qt.alpha(BeyGramTheme.accent, 0.10)
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: BeyGramTheme.accent
                }

                Text {
                    id: quoteText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 9
                    anchors.rightMargin: 8
                    text: root.replyPreview
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.text.length > 0
                text: root.text
                color: BeyGramTheme.text
                font.pixelSize: BeyGramAppearance.messageTextSize
                wrapMode: Text.Wrap
                lineHeight: Design.lineNormal
                textFormat: Text.PlainText
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space1
                visible: root.timeText.length > 0
                         || (root.outgoing && root.statusGlyph.length > 0)

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: root.timeText
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                }

                Text {
                    visible: root.outgoing && root.statusGlyph.length > 0
                    text: root.statusGlyph
                    color: root.status === "failed" ? BeyGramTheme.danger
                         : root.status === "read" ? BeyGramTheme.accent
                         : BeyGramTheme.muted
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignLeft
                Layout.maximumWidth: bubble.width - Design.space5
                visible: root.reactions.length > 0
                implicitWidth: Math.min(reactionsText.implicitWidth + 14,
                                        bubble.width - Design.space5)
                implicitHeight: reactionsText.implicitHeight + 6
                radius: height / 2
                color: Qt.alpha(BeyGramTheme.surface2, 0.9)
                clip: true

                Text {
                    id: reactionsText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    text: root.reactions
                    color: BeyGramTheme.text
                    font.pixelSize: Design.fontCaption
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }
        }
    }
}
