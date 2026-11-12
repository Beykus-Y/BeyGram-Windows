// PollComposerDialog - modal composer for a Telegram poll.
//
// The parent owns the action: `createRequested` carries the trimmed question,
// the non-empty options in display order, the three flags, the index of the
// correct option within `options` (quiz only), and the optional explanation.
// The dialog only validates locally and never calls the backend.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

DsDialog {
    id: root

    property var optionFields: []

    property int correctIndex: 0
    property int filledCount: 0

    readonly property bool canCreate: questionField.text.trim().length > 0
                                      && root.filledCount >= 2

    signal createRequested(string question, var options, bool anonymous,
                           bool multiple, bool quiz, int correctIndex,
                           string explanation)

    function recountOptions() {
        let count = 0
        for (let i = 0; i < optionRepeater.count; ++i) {
            const row = optionRepeater.itemAt(i)
            if (row && row.text && row.text.trim().length > 0)
                ++count
        }
        root.filledCount = count
    }

    function addOption() {
        if (root.optionFields.length >= 10)
            return
        root.optionFields = root.optionFields.concat([""])
        Qt.callLater(function() { root.recountOptions() })
    }

    function removeOptionAt(index) {
        if (root.optionFields.length <= 2)
            return
        if (index < 0 || index >= root.optionFields.length)
            return
        const next = root.optionFields.slice()
        next.splice(index, 1)
        root.optionFields = next
        if (index < root.correctIndex)
            --root.correctIndex
        else if (root.correctIndex >= next.length)
            root.correctIndex = Math.max(0, next.length - 1)
        Qt.callLater(function() { root.recountOptions() })
    }

    function removeOption() {
        root.removeOptionAt(root.optionFields.length - 1)
    }

    function submit() {
        if (!root.canCreate)
            return
        const question = questionField.text.trim()
        const values = []
        let correct = 0
        for (let i = 0; i < optionRepeater.count; ++i) {
            const row = optionRepeater.itemAt(i)
            const value = row && row.text ? row.text.trim() : ""
            if (value.length === 0)
                continue
            if (i === root.correctIndex)
                correct = values.length
            values.push(value)
        }
        if (values.length < 2)
            return
        root.createRequested(question, values, anonymousSwitch.checked,
                             multipleSwitch.checked, quizSwitch.checked,
                             quizSwitch.checked ? correct : 0,
                             quizSwitch.checked ? explanationField.text.trim() : "")
        root.close()
    }

    title: "Create poll"
    description: "Ask a question and add 2–10 answer options."
    width: 520

    onOpened: {
        root.optionFields = ["", ""]
        root.correctIndex = 0
        root.filledCount = 0
        questionField.text = ""
        explanationField.text = ""
        anonymousSwitch.checked = true
        multipleSwitch.checked = false
        quizSwitch.checked = false
        questionField.forceActiveFocus()
    }

    DsTextField {
        id: questionField
        Layout.fillWidth: true
        label: "Question"
        placeholderText: "Ask a question"
        clearable: true
        onAccepted: root.submit()
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: Design.space1
        spacing: Design.space2

        Text {
            text: "Options"
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
        }

        Item { Layout.fillWidth: true }

        Text {
            text: optionRepeater.count + "/10"
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
        }
    }

    ScrollView {
        id: optionScroll
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(optionColumn.implicitHeight, 180)
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: optionColumn
            width: optionScroll.availableWidth
            spacing: Design.space2

            Repeater {
                id: optionRepeater
                model: root.optionFields

                delegate: RowLayout {
                    id: optionRow
                    required property string modelData
                    required property int index
                    property alias text: optionField.text

                    Layout.fillWidth: true
                    spacing: Design.space2

                    DsTextField {
                        id: optionField
                        Layout.fillWidth: true
                        text: optionRow.modelData
                        placeholderText: "Option " + (optionRow.index + 1)
                        clearable: true
                        onTextChanged: {
                            if (root.optionFields && optionRow.index >= 0
                                    && optionRow.index < root.optionFields.length)
                                root.optionFields[optionRow.index] = text
                            root.recountOptions()
                        }
                    }

                    DsIconButton {
                        icon: "\uE74D"
                        tooltip: "Remove option"
                        size: "sm"
                        Layout.alignment: Qt.AlignVCenter
                        enabled: optionRepeater.count > 2
                        onClicked: root.removeOptionAt(optionRow.index)
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Design.space2

        DsButton {
            text: "Add option"
            tone: "secondary"
            size: "sm"
            enabled: optionRepeater.count < 10
            onClicked: root.addOption()
        }

        Item { Layout.fillWidth: true }
    }

    DsDivider {
        Layout.fillWidth: true
        Layout.topMargin: Design.space1
        Layout.bottomMargin: Design.space1
    }

    DsSwitch {
        id: anonymousSwitch
        Layout.fillWidth: true
        text: "Anonymous"
        checked: true
    }

    DsSwitch {
        id: multipleSwitch
        Layout.fillWidth: true
        text: "Allow multiple answers"
    }

    DsSwitch {
        id: quizSwitch
        Layout.fillWidth: true
        text: "Quiz mode"
        onCheckedChanged: {
            if (quizSwitch.checked && root.correctIndex >= optionRepeater.count)
                root.correctIndex = Math.max(0, optionRepeater.count - 1)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: quizSwitch.checked
        spacing: Design.space2

        Text {
            text: "Correct answer"
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontCaption
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: correctFlow.height

            Flow {
                id: correctFlow
                width: parent.width
                height: childrenRect.height
                spacing: Design.space2

                Repeater {
                    model: optionRepeater.count

                    DsChip {
                        required property int index
                        text: "Option " + (index + 1)
                        selected: root.correctIndex === index
                        onClicked: root.correctIndex = index
                    }
                }
            }
        }
    }

    DsTextField {
        id: explanationField
        Layout.fillWidth: true
        visible: quizSwitch.checked
        label: "Explanation"
        placeholderText: "Optional note shown after answering"
    }

    // A custom footer keeps Create disabled until the question and at least
    // two options are present, which the shared DsDialog footer cannot express.
    footer: Item {
        implicitHeight: 36 + Design.space5

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Design.space5
            anchors.rightMargin: Design.space5
            anchors.bottomMargin: Design.space5
            spacing: Design.space2

            Item { Layout.fillWidth: true }

            DsButton {
                text: "Cancel"
                tone: "secondary"
                onClicked: root.close()
            }

            DsButton {
                text: "Create"
                enabled: root.canCreate
                onClicked: root.submit()
            }
        }
    }
}
