pragma ComponentBehavior: Bound

// SavedMessagesPage — the Saved Messages list.
//
// Presentation only: SavedMessagesFacade owns the rows and their order, this
// page turns the parallel previews/ids/dates lists into row objects, asks the
// facade to open or delete a row by index, and keeps the delete gesture and
// the context menu in sync with the same index.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

Item {
    id: page

    property var facade: null
    property bool showBack: false

    signal backRequested()

    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"
    readonly property int rowCount: page.facade ? page.facade.count : 0

    // The facade publishes parallel lists; the list view wants one object per
    // row. Rebuilt whenever the facade emits changed().
    readonly property var rows: {
        if (!page.facade)
            return []

        const ids = page.facade.ids || []
        const previews = page.facade.previews || []
        const dates = page.facade.dates || []
        const out = []
        const total = Math.min(ids.length, previews.length)
        for (let i = 0; i < total; ++i) {
            out.push({
                key: ids[i],
                preview: previews[i],
                date: i < dates.length ? dates[i] : ""
            })
        }
        return out
    }

    function dateLabel(value) {
        const ts = Number(value)
        if (!isFinite(ts) || ts <= 0)
            return ""
        const _rev = BeyGramLang.revision
        return BeyGramLang.formatChatTimestamp(ts)
    }

    function openRow(index) {
        if (page.facade)
            page.facade.open(index)
    }

    function removeRow(index) {
        if (page.facade)
            page.facade.removeAt(index)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Saved messages")
            subtitle: page.rowCount === 0
                      ? qsTr("Messages you save for later")
                      : qsTr("%n saved message(s)", "", page.rowCount)

            leading: DsIconButton {
                visible: page.showBack
                icon: "\uE72B"
                tooltip: qsTr("Back")
                onClicked: page.backRequested()
            }

            trailing: DsIconButton {
                icon: "\uE74D"
                tooltip: qsTr("Clear saved messages")
                enabled: page.rowCount > 0
                onClicked: clearDialog.open()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list

                anchors.fill: parent
                anchors.margins: Design.space2
                clip: true
                spacing: Design.space1
                model: page.rows
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Accessible.role: Accessible.List
                Accessible.name: qsTr("Saved messages")

                delegate: Item {
                    id: row

                    required property int index
                    required property var modelData

                    readonly property int deleteWidth: 88

                    width: list.width
                    height: page.compact ? Design.rowHeightCompact : Design.rowHeight
                    clip: true

                    // Destructive action revealed by dragging the row left.
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: row.deleteWidth
                        radius: Design.rowRadius(BeyGramAppearance.designStyle)
                        color: Qt.alpha(BeyGramTheme.danger, 0.14)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2

                            DsIcon {
                                Layout.alignment: Qt.AlignHCenter
                                name: "\uE74D"
                                size: Design.iconSize
                                color: BeyGramTheme.danger
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("Delete")
                                color: BeyGramTheme.danger
                                font.pixelSize: Design.fontCaption
                                font.weight: Font.Medium
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.removeRow(row.index)
                        }
                    }

                    // The row itself. It slides left over the delete action and
                    // snaps open or closed when the drag ends.
                    Item {
                        id: foreground

                        width: row.width
                        height: row.height
                        x: 0

                        Behavior on x {
                            enabled: !dragArea.drag.active
                            NumberAnimation {
                                duration: Design.motion(Design.motionFast,
                                                        BeyGramAppearance.reduceMotion)
                                easing.type: Easing.OutCubic
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: Design.rowRadius(BeyGramAppearance.designStyle)
                            color: dragArea.containsMouse || dragArea.drag.active
                                   ? Qt.alpha(BeyGramTheme.surface2, 0.70)
                                   : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: Design.motion(Design.motionFast,
                                                            BeyGramAppearance.reduceMotion)
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        DsListRow {
                            anchors.fill: parent
                            hoverEnabled: false
                            title: row.modelData.preview.length > 0
                                   ? row.modelData.preview
                                   : qsTr("Saved message")
                            subtitle: page.dateLabel(row.modelData.date)

                            leading: Rectangle {
                                width: 40
                                height: 40
                                radius: Design.radiusSm
                                color: BeyGramTheme.accentSoft
                                border.width: 1
                                border.color: Qt.alpha(BeyGramTheme.accent, 0.22)

                                DsIcon {
                                    anchors.centerIn: parent
                                    name: "\uE74E"
                                    size: Design.iconSize
                                    color: BeyGramTheme.accentInk
                                    filled: true
                                }
                            }
                        }

                        // One pointer area owns both gestures: dragging left
                        // reveals the delete action, a right click opens the
                        // row menu. It lives inside the foreground so it
                        // slides away with it and leaves the revealed delete
                        // button clickable.
                        MouseArea {
                            id: dragArea

                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            drag.target: foreground
                            drag.axis: Drag.XAxis
                            drag.minimumX: -row.deleteWidth
                            drag.maximumX: 0
                            drag.threshold: 8

                            onPressed: (mouse) => {
                                // Only a left press starts the swipe.
                                drag.target = mouse.button === Qt.LeftButton ? foreground : null
                            }

                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    rowMenu.rowIndex = row.index
                                    rowMenu.openAtCursor()
                                    return
                                }

                                // A revealed row closes before it opens a message.
                                if (foreground.x < 0) {
                                    foreground.x = 0
                                    return
                                }
                                page.openRow(row.index)
                            }

                            onReleased: {
                                foreground.x = foreground.x <= -row.deleteWidth / 2
                                                 ? -row.deleteWidth
                                                 : 0
                                drag.target = foreground
                            }
                        }
                    }
                }
            }

            DsEmptyState {
                anchors.centerIn: parent
                visible: page.rows.length === 0
                glyph: "\uE74E"
                title: qsTr("No saved messages")
                description: qsTr("Messages you save from any chat will appear here.")
            }
        }
    }

    DsMenu {
        id: rowMenu

        property int rowIndex: -1

        DsMenuItem {
            text: qsTr("Open message")
            glyph: "\uE8A7"
            enabled: rowMenu.rowIndex >= 0
            onTriggered: page.openRow(rowMenu.rowIndex)
        }

        DsMenuItem {
            text: qsTr("Delete")
            glyph: "\uE74D"
            tone: "danger"
            enabled: rowMenu.rowIndex >= 0
            onTriggered: page.removeRow(rowMenu.rowIndex)
        }
    }

    DsDialog {
        id: clearDialog

        title: qsTr("Clear saved messages?")
        description: qsTr("This removes all %n saved message(s) from the list. The messages themselves stay in their chats.",
                          "", page.rowCount)
        tone: "danger"
        primaryText: qsTr("Clear all")
        secondaryText: qsTr("Cancel")

        onAccepted: {
            if (page.facade)
                page.facade.clear()
        }
    }
}
