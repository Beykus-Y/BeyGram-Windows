// AttachmentSheet - the composer's attachment picker.
//
// Presentation only. The parent assigns `facade` (an AttachmentFacade), which
// owns the tile order and turns a chosen tile into one request signal. The
// sheet keeps no state of its own, never opens a file dialog and never talks to
// TDLib: the owner reacts to photoRequested()/videoRequested()/... on the facade
// and uses acceptFilter(kind) to build the dialog it needs.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds"
import "../ds/design.js" as Design

DsSheet {
    id: root

    /// AttachmentFacade from the C++ side. A null facade renders an empty sheet
    /// instead of failing.
    property var facade: null

    readonly property var titles: root.facade ? root.facade.titles : []
    readonly property var kinds: root.facade ? root.facade.kinds : []

    // Four tiles per row on a normal window, fewer when the sheet is narrow.
    // The floor of two keeps the labels readable on a phone-sized window.
    readonly property int gridColumns: Math.max(2, Math.min(4, Math.floor(grid.width / 96)))

    // The decorative glyphs mirror the defaults in AttachmentOptions.cpp. They
    // are plain Unicode symbols, so they are drawn with the Windows symbol font
    // rather than DsIcon's Segoe MDL2 Assets. A kind without a glyph still
    // shows its label.
    function glyphFor(kind) {
        switch (kind) {
        case "photo":
            return "\u25A3"
        case "video":
            return "\u25B6"
        case "document":
            return "\u25A4"
        case "audio":
            return "\u266A"
        case "location":
            return "\u2316"
        case "contact":
            return "\u260E"
        case "poll":
            return "\u2263"
        case "sticker":
            return "\u263A"
        }
        return ""
    }

    title: qsTr("Attach")

    function focusFirstTile() {
        if (root.visible && tileRepeater.count > 0)
            tileRepeater.itemAt(0).forceActiveFocus()
    }

    onVisibleChanged: {
        if (visible)
            Qt.callLater(root.focusFirstTile)
    }

    GridLayout {
        id: grid

        Layout.fillWidth: true
        // Keep the tiles compact on a maximized window: the sheet still spans
        // the window, but the grid is centered instead of stretching.
        Layout.maximumWidth: 560
        Layout.alignment: Qt.AlignHCenter
        visible: root.kinds.length > 0
        columns: root.gridColumns
        columnSpacing: Design.space2
        rowSpacing: Design.space3

        Repeater {
            id: tileRepeater

            model: root.kinds

            delegate: ItemDelegate {
                id: tile

                required property int index
                required property string modelData

                readonly property string kindName: String(tile.modelData)
                readonly property string label: tile.index < root.titles.length
                                                ? String(root.titles[tile.index])
                                                : tile.kindName

                Layout.fillWidth: true
                implicitHeight: 84
                padding: Design.space2
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter

                Accessible.role: Accessible.Button
                Accessible.name: qsTr(tile.label)

                onClicked: {
                    // Capture before closing: a parent may clear `facade` from
                    // its onClosed handler. Close first so a picker opened by
                    // the owner is not stacked behind a still-open sheet.
                    const target = root.facade
                    const chosenKind = tile.kindName
                    root.close()
                    if (target)
                        target.choose(chosenKind)
                }

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                background: Rectangle {
                    radius: Design.rowRadius(BeyGramAppearance.designStyle)
                    color: tile.down
                           ? Qt.alpha(BeyGramTheme.accent, 0.18)
                           : (tile.hovered
                              ? Qt.alpha(BeyGramTheme.surface2, 0.9)
                              : Qt.alpha(BeyGramTheme.surface2, 0.55))
                    border.width: 1
                    border.color: tile.hovered
                                  ? Qt.alpha(BeyGramTheme.accent, 0.45)
                                  : Qt.alpha(BeyGramTheme.line, 0.75)
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation {
                            duration: Design.motion(Design.motionFast,
                                                    BeyGramAppearance.reduceMotion)
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                contentItem: Item {
                    // A zero implicit width keeps a long label from forcing the
                    // grid column wider than its share; the inner column is
                    // sized to the tile instead and elides the label.
                    implicitWidth: 0
                    implicitHeight: tileColumn.implicitHeight

                    ColumnLayout {
                        id: tileColumn

                        anchors.centerIn: parent
                        width: tile.availableWidth
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: root.glyphFor(tile.kindName)
                            color: BeyGramTheme.accent
                            font.family: "Segoe UI Symbol"
                            font.pixelSize: Design.iconSizeLarge
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            textFormat: Text.PlainText
                            Accessible.ignored: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr(tile.label)
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            wrapMode: Text.NoWrap
                        }
                    }
                }

                // A 2 px halo just outside the tile, matching DsButton and
                // DsIconButton. Keyboard focus must stay visible on the grid.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -3
                    radius: Design.rowRadius(BeyGramAppearance.designStyle) + 3
                    color: "transparent"
                    border.width: 2
                    border.color: BeyGramTheme.accent
                    visible: tile.visualFocus
                    antialiasing: true
                    z: 2
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: root.kinds.length === 0
        text: qsTr("No attachment options available.")
        color: BeyGramTheme.muted
        font.pixelSize: Design.fontSmall
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }
}
