// SearchSuggestions.qml — recent global-search queries and localized result summary.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    property var facade: null

    readonly property var recentQueries: root.facade ? root.facade.history : []
    readonly property string rawSummaryText: root.facade ? root.facade.summary : ""
    readonly property bool busy: root.facade ? root.facade.loading : false

    readonly property string displaySummary: {
        const s = String(root.rawSummaryText || "").trim()
        if (s.length === 0 || s === "0 chats, 0 messages, 0 files")
            return ""
        const m = s.match(/^(\d+)\s+chats?,\s*(\d+)\s+messages?,\s*(\d+)\s+files?$/)
        if (m)
            return qsTr("%1 chats · %2 messages · %3 files").arg(m[1]).arg(m[2]).arg(m[3])
        return s
    }

    implicitWidth: 280
    implicitHeight: column.implicitHeight
    visible: root.recentQueries.length > 0 || root.displaySummary.length > 0

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.recentQueries.length > 0

            Text {
                Layout.fillWidth: true
                text: qsTr("Recent searches")
                color: BeyGramTheme.muted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            ToolButton {
                id: clearButton

                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 56
                implicitHeight: 24
                padding: 0
                focusPolicy: Qt.NoFocus

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Clear recent searches")

                onClicked: {
                    if (root.facade)
                        root.facade.clearHistory()
                }

                background: Rectangle {
                    radius: 12
                    color: clearButton.hovered ? BeyGramTheme.surface2 : "transparent"
                }

                contentItem: Text {
                    text: qsTr("Clear")
                    color: clearButton.hovered ? BeyGramTheme.accent : BeyGramTheme.muted
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Clear recent searches")
                ToolTip.delay: 600
            }
        }

        Column {
            id: rows

            Layout.fillWidth: true
            spacing: 1

            Repeater {
                model: root.recentQueries

                delegate: Item {
                    id: row

                    readonly property string label: modelData !== undefined
                                                     ? String(modelData) : ""

                    width: rows.width
                    height: 30

                    Accessible.role: Accessible.ListItem
                    Accessible.name: row.label

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: rowArea.containsMouse ? BeyGramTheme.surface2 : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: Text.AlignVCenter
                        text: row.label
                        color: BeyGramTheme.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    MouseArea {
                        id: rowArea

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.facade)
                                root.facade.begin(row.label)
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.displaySummary.length > 0
            text: root.displaySummary
            color: root.busy ? BeyGramTheme.muted : BeyGramTheme.accentInk
            font.pixelSize: 11
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
