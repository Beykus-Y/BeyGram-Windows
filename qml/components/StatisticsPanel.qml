// StatisticsPanel.qml — chat statistics, per-message counters and boost state.
//
// The parent owns the StatisticsFacade and assigns it to `facade`; this panel
// only reads it. Refresh asks for new chat statistics through
// facade.requestChatStats(), and the page answers by calling
// facade.setChatStats() (plus facade.setBoosts()) once TDLib replies. Opening a
// message's statistics is the page's job through facade.openMessageStats().
// Nothing here talks to Telegram.
//
// Every list the facade returns is already formatted for display ("1 234",
// "12.5%"), so the chart strips the grouping spaces before it turns a value
// into a bar height. The panel never invents a number: a missing value, an
// unknown maximum or a null facade simply draws nothing.

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

    // A facade list entry, or an empty string when the two sides disagree in
    // length. Guards every positional read so a partial answer cannot throw.
    function textAt(list, index) {
        if (!list || index < 0 || index >= list.length)
            return ""
        return String(list[index])
    }

    // The number behind a formatted entry: "1 234" -> 1234, "12.5%" -> 12.5.
    // Anything unreadable or negative counts as 0, so no bar is faked.
    function numberAt(list, index) {
        const text = textAt(list, index).replace(/\s/g, "")
        const value = Number(text)
        return isFinite(value) && value > 0 ? value : 0
    }

    // The tallest chart point, used as the 100% of the bar heights.
    readonly property double chartMaximum: {
        let maximum = 0
        for (let i = 0; i < root.chartValues.length; ++i)
            maximum = Math.max(maximum, root.numberAt(root.chartValues, i))
        return maximum
    }

    // Share of the tallest bar, in [0, 1]. 0 while no point has a value.
    function barFraction(index) {
        if (root.chartMaximum <= 0)
            return 0
        return Math.max(0, Math.min(1, root.numberAt(root.chartValues, index) / root.chartMaximum))
    }

    // "1500" -> "1 500" for the per-message counters, which the facade keeps as
    // plain ints.
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
                    text: "Statistics"
                    color: BeyGramTheme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.chatStatsAvailable
                          ? "Chat activity, messages and boosts"
                          : (root.messageStatsActive ? "This message's statistics"
                                                     : "No chat statistics loaded")
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            SecondaryButton {
                text: "Refresh"
                enabled: root.facade !== null
                onClicked: {
                    if (root.facade)
                        root.facade.requestChatStats()
                }
            }
        }

        // Overview: one "label — value" row per statistics entry.
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

        // Chart: one bar per label, scaled against the tallest point.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.chartLabels.length > 0

            Text {
                Layout.fillWidth: true
                text: "Activity"
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

        // Per-message counters: only meaningful while a message is selected.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.messageStatsActive

            Text {
                Layout.fillWidth: true
                text: "This message"
                color: BeyGramTheme.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: ["Views", "Forwards", "Reactions"]

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

        // Boost state: the level, the boosts the chat has and the fill towards
        // the next level. The facade already clamps the progress to [0, 1].
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.boostsVisible

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "Boost level " + root.boostLevel
                    color: BeyGramTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    text: root.groupedCount(root.boostCount)
                          + (root.boostCount === 1 ? " boost" : " boosts")
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
            text: "Statistics appear here once a chat or a message is opened."
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
