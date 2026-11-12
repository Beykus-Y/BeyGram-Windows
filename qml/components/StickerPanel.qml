// Sticker panel for the message composer.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: root

    property var facade: null

    signal stickerActivated(string stickerId)

    readonly property var titles: root.facade ? root.facade.setTitles : []
    readonly property var recentIds: root.facade ? root.facade.recentStickers : []
    readonly property var stickerIds: root.facade ? root.facade.stickers : []
    readonly property int currentIndex: root.facade ? root.facade.currentSetIndex : -1
    readonly property bool loading: root.facade ? root.facade.loading : false
    readonly property string query: root.facade ? root.facade.query : ""

    readonly property int cellSize: 64
    readonly property int cellSpacing: 6

    implicitWidth: 360
    implicitHeight: 420
    radius: 14
    color: BeyGramTheme.surface
    border.width: 1
    border.color: BeyGramTheme.line

    function matchesQuery(stickerId) {
        if (root.query.length === 0)
            return true
        return String(stickerId).indexOf(root.query) !== -1
    }

    function activate(stickerId) {
        root.stickerActivated(String(stickerId))
    }

    onStickerActivated: function(stickerId) {
        if (root.facade)
            root.facade.recordSent(stickerId)
    }

    Connections {
        target: root.facade
        function onChanged() {
            if (searchField.text !== root.query && !searchField.activeFocus)
                searchField.text = root.query
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Flickable {
            id: tabStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            clip: true
            contentWidth: tabRow.width
            contentHeight: tabRow.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

            Row {
                id: tabRow
                spacing: 6

                Repeater {
                    model: root.titles

                    delegate: Rectangle {
                        id: tab

                        readonly property bool current: index === root.currentIndex

                        width: Math.max(56, tabLabel.implicitWidth + 22)
                        height: 28
                        radius: 14
                        color: tab.current ? BeyGramTheme.accent : BeyGramTheme.surface2
                        border.width: 1
                        border.color: tab.current ? BeyGramTheme.accent : BeyGramTheme.line

                        Accessible.role: Accessible.PageTab
                        Accessible.name: String(modelData)

                        Behavior on color { ColorAnimation { duration: 110 } }

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            width: Math.min(implicitWidth, tab.width - 16)
                            text: String(modelData)
                            color: tab.current ? BeyGramTheme.onAccent : BeyGramTheme.text
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.facade)
                                    root.facade.currentSetIndex = index
                            }
                        }
                    }
                }
            }
        }

        Flickable {
            id: recentStrip
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 44 : 0
            visible: root.recentIds.length > 0
            clip: true
            contentWidth: recentRow.width
            contentHeight: recentRow.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

            Row {
                id: recentRow
                spacing: 6

                Repeater {
                    model: root.recentIds

                    delegate: Rectangle {
                        id: recentCell

                        width: 40
                        height: 40
                        radius: 9
                        color: recentArea.containsMouse ? BeyGramTheme.surface2
                                                       : Qt.alpha(BeyGramTheme.surface2, 0.6)

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Recent sticker %1").arg(String(modelData))

                        Text {
                            anchors.centerIn: parent
                            width: parent.width - 6
                            text: String(modelData)
                            color: BeyGramTheme.muted
                            font.pixelSize: 9
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideMiddle
                        }

                        MouseArea {
                            id: recentArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activate(modelData)
                        }
                    }
                }
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            implicitHeight: 32
            leftPadding: 12
            rightPadding: 12
            placeholderText: qsTr("Search stickers")
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 12
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            text: root.query
            onTextEdited: {
                if (root.facade)
                    root.facade.query = searchField.text
            }
            Keys.onEscapePressed: (event) => {
                if (searchField.text.length > 0) {
                    searchField.text = ""
                    if (root.facade)
                        root.facade.query = ""
                }
                event.accepted = true
            }

            background: Rectangle {
                radius: 16
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
                    width: scroller.width
                    spacing: root.cellSpacing

                    Repeater {
                        model: root.stickerIds

                        delegate: Rectangle {
                            id: cell

                            readonly property string stickerId: String(modelData)

                            width: root.cellSize
                            height: root.cellSize
                            radius: 11
                            visible: root.matchesQuery(cell.stickerId)
                            color: cellArea.containsMouse ? BeyGramTheme.surface2
                                                          : Qt.alpha(BeyGramTheme.surface2, 0.55)
                            border.width: 1
                            border.color: BeyGramTheme.line

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Sticker %1").arg(cell.stickerId)

                            Behavior on color { ColorAnimation { duration: 100 } }

                            Text {
                                anchors.centerIn: parent
                                width: parent.width - 8
                                text: cell.stickerId
                                color: BeyGramTheme.text
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideMiddle
                            }

                            MouseArea {
                                id: cellArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.activate(cell.stickerId)
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
