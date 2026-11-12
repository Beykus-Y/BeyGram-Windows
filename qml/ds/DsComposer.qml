import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "design.js" as Design

// Presentation-only composer bar for the BeyGram design system.
//
// The owner wires the four signals to its chat backend; the component never
// sends anything itself and never clears `text`. Enter emits sendRequested()
// when `canSend` is true, Shift+Enter inserts a newline. The root is
// transparent, so the page keeps ownership of the bar background/border.
Item {
    id: root

    property alias text: input.text
    property string placeholder: "Write a message"
    property string replyPreview: ""
    property bool canSend: text.trim().length > 0

    signal sendRequested()
    signal attachRequested()
    signal voiceRequested()
    signal emojiRequested()

    implicitWidth: 360
    implicitHeight: column.implicitHeight + Design.space4

    // --- Small vector icons (no icon font/asset dependency) -----------------
    component GlyphIcon: Canvas {
        id: glyph
        property string kind: "attach"
        property color color: BeyGramTheme.muted

        implicitWidth: Design.iconSize
        implicitHeight: Design.iconSize
        antialiasing: true

        onColorChanged: requestPaint()
        onKindChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = glyph.color
            ctx.fillStyle = glyph.color
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (glyph.kind === "attach") {
                ctx.beginPath()
                ctx.moveTo(11.8, 4.2)
                ctx.lineTo(5.8, 10.2)
                ctx.quadraticCurveTo(4.4, 11.6, 5.8, 13.0)
                ctx.quadraticCurveTo(7.2, 14.4, 8.6, 13.0)
                ctx.lineTo(13.4, 8.2)
                ctx.quadraticCurveTo(14.4, 7.2, 13.4, 6.2)
                ctx.quadraticCurveTo(12.4, 5.2, 11.4, 6.2)
                ctx.lineTo(6.6, 11.0)
                ctx.stroke()
            } else if (glyph.kind === "emoji") {
                ctx.beginPath()
                ctx.arc(9, 9, 6.2, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(6.9, 7.3, 0.9, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.arc(11.1, 7.3, 0.9, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(6.2, 10.3)
                ctx.quadraticCurveTo(9, 12.8, 11.8, 10.3)
                ctx.stroke()
            } else if (glyph.kind === "send") {
                ctx.beginPath()
                ctx.moveTo(5.2, 12.8)
                ctx.lineTo(12.8, 5.2)
                ctx.moveTo(6.6, 5.2)
                ctx.lineTo(12.8, 5.2)
                ctx.lineTo(12.8, 11.4)
                ctx.stroke()
            } else if (glyph.kind === "voice") {
                ctx.beginPath()
                ctx.moveTo(7.2, 5.0)
                ctx.quadraticCurveTo(7.2, 3.4, 9.0, 3.4)
                ctx.quadraticCurveTo(10.8, 3.4, 10.8, 5.0)
                ctx.lineTo(10.8, 9.0)
                ctx.quadraticCurveTo(10.8, 10.6, 9.0, 10.6)
                ctx.quadraticCurveTo(7.2, 10.6, 7.2, 9.0)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(5.4, 8.6)
                ctx.quadraticCurveTo(5.4, 12.2, 9.0, 12.2)
                ctx.quadraticCurveTo(12.6, 12.2, 12.6, 8.6)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(9.0, 12.2)
                ctx.lineTo(9.0, 14.6)
                ctx.moveTo(6.8, 14.6)
                ctx.lineTo(11.2, 14.6)
                ctx.stroke()
            }
        }
    }

    component IconButton: Rectangle {
        id: button
        property string accessibleName: ""
        signal clicked()

        implicitWidth: Design.controlHeightSmall
        implicitHeight: Design.controlHeightSmall
        radius: width / 2
        color: mouse.containsMouse
               ? Qt.alpha(BeyGramTheme.text, Design.opacityHover) : "transparent"

        Accessible.role: Accessible.Button
        Accessible.name: button.accessibleName
        Accessible.onPressAction: button.clicked()

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Design.space2
        spacing: Design.space2

        Rectangle {
            id: replyBar
            Layout.fillWidth: true
            visible: root.replyPreview.length > 0
            implicitHeight: replyText.implicitHeight + 10
            radius: Design.radiusSm
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
                id: replyText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 9
                anchors.rightMargin: 8
                text: root.replyPreview
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space1

            IconButton {
                Layout.alignment: Qt.AlignBottom
                accessibleName: "Attach file"
                onClicked: root.attachRequested()

                GlyphIcon {
                    anchors.centerIn: parent
                    kind: "attach"
                    color: BeyGramTheme.muted
                }
            }

            IconButton {
                Layout.alignment: Qt.AlignBottom
                accessibleName: "Insert emoji"
                onClicked: root.emojiRequested()

                GlyphIcon {
                    anchors.centerIn: parent
                    kind: "emoji"
                    color: BeyGramTheme.muted
                }
            }

            TextArea {
                id: input
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(120, Math.max(Design.controlHeight,
                                                              contentHeight + 18))
                leftPadding: 12
                rightPadding: 12
                topPadding: 9
                bottomPadding: 9
                placeholderText: root.placeholder
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                color: BeyGramTheme.text
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: Design.fontBody
                wrapMode: TextArea.Wrap
                background: Rectangle {
                    radius: Design.composerRadius
                    color: BeyGramTheme.surface2
                    border.width: input.activeFocus ? 1 : 0
                    border.color: BeyGramTheme.accent
                }
                Keys.onPressed: (event) => {
                    if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter)
                        return
                    if (event.modifiers & Qt.ShiftModifier)
                        return
                    if (root.canSend)
                        root.sendRequested()
                    event.accepted = true
                }
            }

            Rectangle {
                id: voiceButton
                Layout.alignment: Qt.AlignBottom
                visible: !root.canSend
                implicitWidth: Design.controlHeightSmall
                implicitHeight: Design.controlHeightSmall
                radius: width / 2
                color: voiceMouse.containsMouse
                       ? Qt.alpha(BeyGramTheme.text, Design.opacityHover)
                       : "transparent"

                Accessible.role: Accessible.Button
                Accessible.name: "Record voice message"
                Accessible.onPressAction: root.voiceRequested()

                MouseArea {
                    id: voiceMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.voiceRequested()
                }

                GlyphIcon {
                    anchors.centerIn: parent
                    kind: "voice"
                    color: BeyGramTheme.muted
                }
            }

            Rectangle {
                id: sendButton
                Layout.alignment: Qt.AlignBottom
                visible: root.canSend
                implicitWidth: Design.controlHeightSmall
                implicitHeight: Design.controlHeightSmall
                radius: width / 2
                color: sendMouse.pressed ? Qt.darker(BeyGramTheme.accent, 1.10)
                     : sendMouse.containsMouse ? Qt.lighter(BeyGramTheme.accent, 1.06)
                     : BeyGramTheme.accent

                Accessible.role: Accessible.Button
                Accessible.name: "Send message"
                Accessible.onPressAction: root.sendRequested()

                MouseArea {
                    id: sendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.sendRequested()
                }

                GlyphIcon {
                    anchors.centerIn: parent
                    kind: "send"
                    color: BeyGramTheme.onAccent
                }
            }
        }
    }
}
