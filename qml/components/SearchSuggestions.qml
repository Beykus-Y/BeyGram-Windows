// SearchSuggestions.qml — recent global-search queries and the result summary.
//
// Presentation only. The parent passes the C++ SearchFacade
// (src/ui/SearchFacade.h) as `facade`; this component never talks to TDLib,
// the network or the history file itself. Selecting a row calls
// facade.begin(text), which records the query and emits searchRequested() for
// the owner to run. The clear button drops the history through
// facade.clearHistory().
//
// Every binding is guarded against a null facade, so the component can be
// instantiated before the backend exists and simply stays empty.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    property var facade: null

    readonly property var recentQueries: root.facade ? root.facade.history : []
    readonly property string summaryText: root.facade ? root.facade.summary : ""
    readonly property bool busy: root.facade ? root.facade.loading : false

    implicitWidth: 280
    implicitHeight: column.implicitHeight
    visible: root.recentQueries.length > 0 || root.summaryText.length > 0

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

                    // Repeater feeds each entry of the string list as modelData.
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
            visible: root.summaryText.length > 0
            text: root.summaryText
            color: root.busy ? BeyGramTheme.muted : BeyGramTheme.accentInk
            font.pixelSize: 11
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
