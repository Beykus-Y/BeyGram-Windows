// Sticker picker popup for the message composer.
//
// Presentation only. The parent owns the StickerSetModel, the download state
// and the send request: it assigns `model`, flips `loading` while the set is
// fetched, and turns the emitted stickerSelected(stickerId, fullFileId) into a
// TDLib sendMessage with a messageSticker content. Thumbnails come from the
// existing image://beygrammedia provider, so this file never touches TDLib,
// the network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Popup {
    id: root

    /// StickerSetModel with the roles stickerId, emoji, thumbnailFileId,
    /// fullFileId, isAnimated and isVideo. Only the first four are used here;
    /// animated/video playback stays with the message bubble.
    property var model: null
    /// True while the parent is still loading the sticker set.
    property bool loading: false

    /// Emitted with the tapped sticker. `fullFileId` is the file the caller
    /// should download and send; `stickerId` identifies the sticker itself.
    signal stickerSelected(var stickerId, var fullFileId)

    /// Four 80px cells per row, as laid out by the Flow below.
    readonly property int columns: 4
    readonly property int cellSize: 80
    readonly property int cellSpacing: 5
    readonly property int gridWidth: root.columns * root.cellSize
                                          + (root.columns - 1) * root.cellSpacing

    /// Lowercased search text; empty means "show everything".
    readonly property string query: searchField.text.trim().toLowerCase()

    implicitWidth: 360
    implicitHeight: 420
    width: implicitWidth
    height: implicitHeight

    padding: 0
    modal: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // An empty emoji always matches an empty query, so a sticker without an
    // emoji is only hidden while the user is actually searching.
    function matchesQuery(emoji) {
        if (root.query.length === 0)
            return true
        if (!emoji)
            return false
        return String(emoji).toLowerCase().indexOf(root.query) !== -1
    }

    // Emits the choice and dismisses the picker; the caller decides what to do.
    function select(stickerId, fullFileId) {
        root.stickerSelected(stickerId, fullFileId)
        root.close()
    }

    onOpened: {
        searchField.text = ""
        searchField.forceActiveFocus()
    }

    background: Rectangle {
        radius: 14
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
    }

    contentItem: ColumnLayout {
        spacing: 8

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.preferredHeight: 34
            implicitHeight: 34
            leftPadding: 13
            rightPadding: 13
            placeholderText: qsTr("Search stickers...")
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 12
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            Keys.onEscapePressed: (event) => {
                if (searchField.text.length > 0) {
                    searchField.text = ""
                } else {
                    root.close()
                }
                event.accepted = true
            }

            background: Rectangle {
                radius: 17
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: searchField.activeFocus ? BeyGramTheme.accent
                                                      : Qt.alpha(BeyGramTheme.line, 0.9)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12

            Flickable {
                id: scroller
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: stickerFlow.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Flow {
                    id: stickerFlow
                    width: Math.min(scroller.width, root.gridWidth)
                    spacing: root.cellSpacing

                    Repeater {
                        model: root.model

                        delegate: Rectangle {
                            id: cell

                            // Roles of StickerSetModel. stickerId is a qint64,
                            // so every id stays `var` to avoid truncation.
                            required property var stickerId
                            required property var emoji
                            required property var thumbnailFileId
                            required property var fullFileId

                            readonly property int thumbId: Number(cell.thumbnailFileId) || 0
                            readonly property string emojiText: cell.emoji ? String(cell.emoji) : ""
                            readonly property bool matches: root.matchesQuery(cell.emojiText)

                            width: root.cellSize
                            height: root.cellSize
                            radius: 12
                            // The Flow is a positioner, so it skips invisible
                            // children and closes the gap for filtered stickers.
                            visible: cell.matches
                            color: cellArea.containsMouse ? BeyGramTheme.surface2 : "transparent"

                            Accessible.role: Accessible.Button
                            Accessible.name: cell.emojiText.length > 0 ? cell.emojiText : qsTr("Sticker")

                            Behavior on color { ColorAnimation { duration: 100 } }

                            Image {
                                id: preview
                                anchors.fill: parent
                                anchors.margins: 6
                                source: cell.thumbId > 0
                                        ? "image://beygrammedia/" + cell.thumbId + "?v=0"
                                        : ""
                                sourceSize: Qt.size(root.cellSize * 2, root.cellSize * 2)
                                cache: true
                                asynchronous: true
                                smooth: true
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }

                            // Emoji fallback while the thumbnail is missing or
                            // still downloading.
                            Text {
                                anchors.centerIn: parent
                                width: parent.width - 8
                                visible: !preview.visible
                                text: cell.emojiText.length > 0 ? cell.emojiText : qsTr("[Sticker]")
                                color: cell.emojiText.length > 0 ? BeyGramTheme.text
                                                                 : BeyGramTheme.muted
                                font.pixelSize: cell.emojiText.length > 0 ? 34 : 10
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: cellArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.select(cell.stickerId, cell.fullFileId)
                            }
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.loading
                visible: running
                implicitWidth: 32
                implicitHeight: 32
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(240, parent.width - 40))
                // A Flow skips invisible children, so its height stays 0 while
                // the set is empty or the search hides every sticker.
                visible: !root.loading && stickerFlow.height === 0
                text: root.query.length > 0 ? qsTr("No stickers match your search")
                                            : qsTr("No stickers to show")
                color: BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
