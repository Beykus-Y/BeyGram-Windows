// StatisticsPanel.qml — chat statistics, per-message counters and boost state.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: root

    property var facade: null

    readonly property int padding: 20
    readonly property int chartHeight: 132

    readonly property bool chatStatsAvailable: root.facade ? Boolean(root.facade.chatStatsAvailable) : false
    readonly property var overviewLabels: root.facade ? root.facade.overviewLabels : []
    readonly property var overviewValues: root.facade ? root.facade.overviewValues : []
    readonly property var chartLabels: root.facade ? root.facade.chartLabels : []
    readonly property var chartValues: root.facade ? root.facade.chartValues : []

    readonly property bool messageStatsActive: root.facade ? Boolean(root.facade.messageStatsActive) : false
    readonly property int messageViews: root.facade ? Number(root.facade.views) : 0
    readonly property int messageForwards: root.facade ? Number(root.facade.forwards) : 0
    readonly property int messageReactions: root.facade ? Number(root.facade.reactions) : 0

    readonly property int boostLevel: root.facade ? Number(root.facade.boostLevel) : 0
    readonly property int boostCount: root.facade ? Number(root.facade.boostCount) : 0
    readonly property double boostProgress: root.facade ? Number(root.facade.boostProgress) : 0

    readonly property bool boostsVisible: root.boostLevel > 0 || root.boostCount > 0
    readonly property bool empty: !root.chatStatsAvailable && !root.messageStatsActive && !root.boostsVisible

    implicitWidth: 420
    implicitHeight: content.implicitHeight + root.padding * 2

    radius: 18
    color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.78 : 0.92)
    border.width: 1
    border.color: Qt.alpha(BeyGramTheme.line, 0.95)

    Behavior on color { ColorAnimation { duration: 180 } }

    function textAt(list, index) {
        if (!list || index < 0 || index >= list.length)
            return ""
        return String(list[index])
    }

    function numberAt(list, index) {
        const text = textAt(list, index).replace(/\s/g, "")
        const value = Number(text)
        return isFinite(value) && value > 0 ? value : 0
    }

    readonly property double chartMaximum: {
        let maximum = 0
        for (let i = 0; i < root.chartValues.length; ++i)
            maximum = Math.max(maximum, root.numberAt(root.chartValues, i))
        return maximum
    }

    function barFraction(index) {
        if (root.chartMaximum <= 0)
            return 0
        return Math.max(0, Math.min(1, root.numberAt(root.chartValues, index) / root.chartMaximum))
    }

    function groupedCount(value) {
        const count = Number(value)
        if (!isFinite(count) || count <= 0)
            return "0"
        return String(Math.floor(count)).replace(/\B(?=(\d{3})+(?!\d))/g, " ")
    }

    function counterAt(index) {
        if (index === 0)
            return root.messageViews
        if (index === 1)
            return root.messageForwards
        return root.messageReactions
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.padding
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Statistics")
                    color: BeyGramTheme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.chatStatsAvailable
                          ? qsTr("Chat activity, messages and boosts")
                          : (root.messageStatsActive ? qsTr("This message's statistics")
                                                     : qsTr("No chat statistics loaded"))
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            SecondaryButton {
                text: qsTr("Refresh")
                enabled: root.facade !== null
                onClicked: {
                    if (root.facade)
                        root.facade.requestChatStats()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.overviewLabels.length > 0

            Repeater {
                model: root.overviewLabels.length

                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: root.textAt(root.overviewLabels, index)
                        color: BeyGramTheme.muted
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    Text {
                        text: root.textAt(root.overviewValues, index)
                        color: BeyGramTheme.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.chartLabels.length > 0

            Text {
                Layout.fillWidth: true
                text: qsTr("Activity")
                color: BeyGramTheme.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Item {
                id: chartArea
                Layout.fillWidth: true
                Layout.preferredHeight: root.chartHeight

                Row {
                    id: bars
                    anchors.fill: parent
                    spacing: 8

                    Repeater {
                        model: root.chartLabels.length

                        delegate: Column {
                            id: barColumn
                            width: Math.max(0, (bars.width - bars.spacing * Math.max(0, root.chartLabels.length - 1))
                                                / Math.max(1, root.chartLabels.length))
                            height: bars.height
                            spacing: 4

                            Text {
                                width: parent.width
                                height: 16
                                text: root.textAt(root.chartValues, index)
                                color: BeyGramTheme.muted
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            Item {
                                id: barTrack
                                width: parent.width
                                height: Math.max(2, barColumn.height - 40)

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 5
                                    color: Qt.alpha(BeyGramTheme.muted, 0.12)
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: Math.max(2, parent.height * root.barFraction(index))
                                    radius: 5
                                    color: BeyGramTheme.accent
                                }
                            }

                            Text {
                                width: parent.width
                                height: 16
                                text: root.textAt(root.chartLabels, index)
                                color: BeyGramTheme.muted
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.messageStatsActive

            Text {
                Layout.fillWidth: true
                text: qsTr("This message")
                color: BeyGramTheme.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: [qsTr("Views"), qsTr("Forwards"), qsTr("Reactions")]

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: root.groupedCount(root.counterAt(index))
                            color: BeyGramTheme.text
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: BeyGramTheme.muted
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.boostsVisible

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Boost level %1").arg(root.boostLevel)
                    color: BeyGramTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    text: qsTr("%n boost(s)", "", root.boostCount)
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
                radius: 4
                color: Qt.alpha(BeyGramTheme.muted, 0.16)

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, root.boostProgress))
                    height: parent.height
                    radius: 4
                    color: BeyGramTheme.accent
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.empty
            text: qsTr("Statistics appear here once a chat or a message is opened.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
