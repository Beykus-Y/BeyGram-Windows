import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: dialog
    title: "Create poll"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    width: 460

    property var optionFields: []

    signal createRequested(string question, var options, bool anonymous,
                           bool multiple, bool quiz, int correctIndex,
                           string explanation)

    function addOption() {
        if (optionFields.length >= 10)
            return
        optionFields.push("")
        optionRepeater.model = optionFields
    }

    function removeOption() {
        if (optionFields.length <= 2)
            return
        optionFields.pop()
        optionRepeater.model = optionFields
    }

    function submit() {
        const question = questionField.text.trim()
        if (question.length === 0)
            return
        const values = []
        for (let i = 0; i < optionRepeater.count; ++i) {
            const field = optionRepeater.itemAt(i)
            if (field && field.text.trim().length > 0)
                values.push(field.text.trim())
        }
        if (values.length < 2)
            return
        dialog.createRequested(question, values, anonymousBox.checked,
                               multipleBox.checked, quizBox.checked,
                               correctSpin.value, explanationField.text.trim())
        dialog.close()
    }

    onOpened: {
        optionFields = ["", ""]
        optionRepeater.model = optionFields
        questionField.text = ""
        explanationField.text = ""
        anonymousBox.checked = true
        multipleBox.checked = false
        quizBox.checked = false
        correctSpin.value = 0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        TextField {
            id: questionField
            Layout.fillWidth: true
            placeholderText: "Question"
            font.pixelSize: 13
        }

        Repeater {
            id: optionRepeater
            model: []
            delegate: TextField {
                Layout.fillWidth: true
                text: modelData
                placeholderText: "Option " + (index + 1)
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            SecondaryButton { text: "Add option"; onClicked: dialog.addOption() }
            SecondaryButton { text: "Remove"; onClicked: dialog.removeOption() }
            Item { Layout.fillWidth: true }
            Text { text: optionRepeater.count + "/10"; color: BeyGramTheme.muted; font.pixelSize: 10 }
        }

        CheckBox { id: anonymousBox; text: "Anonymous"; checked: true; palette.text: BeyGramTheme.text }
        CheckBox { id: multipleBox; text: "Multiple choice"; palette.text: BeyGramTheme.text }
        CheckBox { id: quizBox; text: "Quiz"; palette.text: BeyGramTheme.text }

        RowLayout {
            Layout.fillWidth: true
            visible: quizBox.checked
            Text { text: "Correct option"; color: BeyGramTheme.muted; font.pixelSize: 11 }
            SpinBox { id: correctSpin; from: 0; to: Math.max(0, optionRepeater.count - 1) }
        }
        TextField {
            id: explanationField
            Layout.fillWidth: true
            visible: quizBox.checked
            placeholderText: "Explanation (optional)"
            font.pixelSize: 12
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8
            SecondaryButton { text: "Cancel"; onClicked: dialog.close() }
            PrimaryButton { text: "Create"; onClicked: dialog.submit() }
        }
    }
}
