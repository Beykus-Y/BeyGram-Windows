import QtQuick
import BeyGram.Theme 1.0

/// Square-cropped thumbnail grid for a multi-item media album in a message
/// bubble.
///
/// Two columns are used for 2-4 items and three columns for 5+ items, matching
/// Telegram's grouped-media layout. Each cell only renders the item thumbnail;
/// the full-size media is opened by the owner through `mediaClicked`.
///
/// `items` entries are plain objects shaped like:
///   { fileId, thumbnailFileId, ready, revision, width, height, messageId }
/// Assign a new array to `items` to refresh the grid; in-place mutation of the
/// array is not observed by the model.
Item {
    id: root

    property var items: []

    signal mediaClicked(int fileId)

    readonly property int maxWidth: 320
    readonly property int gap: 2
    readonly property int cornerRadius: 12
    readonly property int itemCount: items ? items.length : 0
    readonly property int columns: itemCount >= 5 ? 3 : 2
    readonly property int rows: itemCount > 0 ? Math.ceil(itemCount / columns) : 0

    // Natural cell size before the 320 px cap: 158 px for two columns and
    // 104 px for three, so the implicit width lands just under the maximum.
    readonly property real baseCellSize: columns === 3 ? 104 : 158
    readonly property real cellSize: columns > 0
                                     ? Math.max(0, (Math.min(width, maxWidth)
                                                    - gap * (columns - 1)) / columns)
                                     : 0

    implicitWidth: itemCount > 0
                   ? Math.min(maxWidth, columns * baseCellSize + gap * (columns - 1))
                   : 0
    implicitHeight: itemCount > 0 ? rows * cellSize + gap * (rows - 1) : 0

    function itemAt(index) {
        if (!items || index < 0 || index >= items.length)
            return null
        return items[index] || null
    }

    function thumbnailFor(index) {
        const item = itemAt(index)
        return item ? Number(item.thumbnailFileId || 0) : 0
    }

    function revisionFor(index) {
        const item = itemAt(index)
        return item ? Number(item.revision || 0) : 0
    }

    function sourceFor(index) {
        const item = itemAt(index)
        if (!item || item.ready !== true)
            return ""
        const thumbnailId = thumbnailFor(index)
        if (thumbnailId <= 0)
            return ""
        return "image://beygrammedia/" + thumbnailId + "?v=" + revisionFor(index)
    }

    Grid {
        anchors.left: parent.left
        anchors.top: parent.top
        columns: root.columns
        spacing: root.gap

        Repeater {
            model: root.items

            delegate: Rectangle {
                width: root.cellSize
                height: root.cellSize
                radius: root.cornerRadius
                color: BeyGramTheme.surface2
                clip: true

                Image {
                    anchors.fill: parent
                    source: root.sourceFor(index)
                    sourceSize: Qt.size(Math.max(1, Math.round(width * 2)),
                                        Math.max(1, Math.round(height * 2)))
                    cache: true
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const thumbnailId = root.thumbnailFor(index)
                        if (thumbnailId > 0)
                            root.mediaClicked(thumbnailId)
                    }
                }
            }
        }
    }
}
