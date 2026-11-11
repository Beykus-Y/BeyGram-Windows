import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

/// Read-only poll / quiz card.
///
/// The component never invents results: percentages and the option bar are
/// derived only from `totalVoterCount` and each option's `voterCount`. When the
/// total is unknown (0 or less) no percentage or bar is drawn.
///
/// Options are `{ text, voterCount, chosen }`. Selecting an option emits
/// `optionSelected(index)` and is ignored once `closed` is true.
Item {
    id: root

    property string question: ""
    property var options: []
    property int totalVoterCount: 0
    property bool closed: false
    property bool quiz: false
    property bool multipleChoice: false

    signal optionSelected(int index)

    implicitWidth: 300
    implicitHeight: column.implicitHeight + 24

    readonly property int voteCount: totalVoterCount > 0 ? totalVoterCount : 0

    function optionAt(index) {
        if (!options || index < 0 || index >= options.length)
            return null
        return options[index]
    }

    function optionText(option) {
        return option && option.text !== undefined ? String(option.text) : ""
    }

    function countOf(option) {
        if (!option)
            return 0
        const value = Number(option.voterCount)
        return value > 0 ? Math.floor(value) : 0
    }

    function isChosen(option) {
        return !!(option && option.chosen === true)
    }

    // Share of the total, or 0 when the total is unknown so nothing is faked.
    function fractionFor(count) {
        if (voteCount <= 0)
            return 0
        return Math.max(0, Math.min(1, count / voteCount))
    }

    // -1 means "unknown": the label stays hidden instead of showing 0%.
    function percentFor(count) {
        if (voteCount <= 0)
            return -1
        return Math.round(100 * fractionFor(count))
    }

    function voteSummary() {
        return voteCount + (voteCount === 1 ? " vote" : " votes")
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.9 : 0.98)
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 0.9)
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 8

        Text {
            Layout.fillWidth: true
            visible: root.question.length > 0
            text: root.question
            color: BeyGramTheme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }

        Column {
            id: optionsColumn
            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: root.options ? root.options : []

                delegate: Rectangle {
                    id: optionRow

                    property var option: modelData
                    property int count: root.countOf(option)
                    property bool chosen: root.isChosen(option)
                    property real fraction: root.fractionFor(count)
                    property int percent: root.percentFor(count)

                    width: optionsColumn.width
                    height: Math.max(44, optionLabel.implicitHeight + 22)
                    radius: 10
                    clip: true
                    color: chosen
                           ? Qt.alpha(BeyGramTheme.accentSoft, 0.9)
                           : (hoverArea.containsMouse && !root.closed
                              ? Qt.alpha(BeyGramTheme.surface2, 0.9)
                              : Qt.alpha(BeyGramTheme.surface2, 0.5))
                    border.width: 1
                    border.color: chosen
                                  ? Qt.alpha(BeyGramTheme.accent, 0.38)
                                  : Qt.alpha(BeyGramTheme.line, 0.7)

                    Accessible.role: root.multipleChoice ? Accessible.CheckBox
                                                         : Accessible.RadioButton
                    Accessible.name: root.optionText(option)
                    Accessible.checked: chosen

                    // Result fill: width is count / total, never a decorative value.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: parent.width * optionRow.fraction
                        visible: optionRow.fraction > 0
                        radius: optionRow.radius
                        color: Qt.alpha(BeyGramTheme.accent, optionRow.chosen ? 0.24 : 0.12)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 12
                        spacing: 9

                        Item {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 16
                            implicitHeight: 16

                            Rectangle {
                                anchors.fill: parent
                                radius: root.multipleChoice ? 4 : 8
                                color: "transparent"
                                border.width: 1.5
                                border.color: optionRow.chosen
                                              ? BeyGramTheme.accent
                                              : Qt.alpha(BeyGramTheme.muted, 0.8)
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                visible: optionRow.chosen
                                width: 8
                                height: 8
                                radius: root.multipleChoice ? 2 : 4
                                color: BeyGramTheme.accent
                            }
                        }

                        Text {
                            id: optionLabel
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: root.optionText(optionRow.option)
                            color: BeyGramTheme.text
                            font.pixelSize: 13
                            font.weight: optionRow.chosen ? Font.DemiBold : Font.Normal
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            lineHeight: 1.25
                        }

                        ColumnLayout {
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 0
                            visible: optionRow.count > 0 || optionRow.percent >= 0

                            Text {
                                Layout.alignment: Qt.AlignRight
                                visible: optionRow.percent >= 0
                                text: optionRow.percent >= 0 ? optionRow.percent + "%" : ""
                                color: optionRow.chosen ? BeyGramTheme.accentInk
                                                        : BeyGramTheme.muted
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.alignment: Qt.AlignRight
                                visible: optionRow.count > 0
                                text: optionRow.count
                                color: BeyGramTheme.muted
                                font.pixelSize: 11
                            }
                        }
                    }

                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !root.closed
                        cursorShape: root.closed ? Qt.ArrowCursor
                                                 : Qt.PointingHandCursor
                        onClicked: root.optionSelected(index)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.voteSummary()
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            Pill {
                visible: root.closed
                text: "Closed"
                tone: "neutral"
            }

            Pill {
                visible: root.quiz
                text: "Quiz"
                tone: "accent"
            }

            Pill {
                visible: root.multipleChoice
                text: "Multiple choice"
                tone: "neutral"
            }
        }
    }
}
