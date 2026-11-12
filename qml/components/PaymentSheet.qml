// PaymentSheet - checkout sheet for one invoice.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: root

    property var facade: null

    readonly property bool payable: root.facade ? root.facade.canPay : false

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 420
    padding: 0
    modal: true
    focus: true
    title: qsTr("Payment")
    closePolicy: Popup.CloseOnEscape

    function syncFields() {
        if (!root.facade)
            return
        if (!nameField.activeFocus)
            nameField.text = root.facade.name
        if (!emailField.activeFocus)
            emailField.text = root.facade.email
        if (!phoneField.activeFocus)
            phoneField.text = root.facade.phone
    }

    onOpened: {
        syncFields()
        nameField.forceInputFocus()
    }

    Connections {
        target: root.facade

        function onChanged() {
            root.syncFields()
        }
    }

    Overlay.modal: Rectangle {
        color: Qt.alpha("#000000", 0.35)
    }

    background: Rectangle {
        radius: 16
        color: BeyGramTheme.surface
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 1.0)
    }

    header: Item {
        implicitHeight: 52

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        implicitWidth: 420

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 4
            spacing: 4

            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: root.facade ? root.facade.title : ""
                color: BeyGramTheme.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("Total")
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: root.facade ? root.facade.totalText : ""
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            implicitHeight: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        AuthField {
            id: nameField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: qsTr("Name")
            placeholderText: qsTr("Full name")
            maximumLength: 128
            onEdited: if (root.facade) root.facade.name = text
            onAccepted: emailField.forceInputFocus()
        }

        AuthField {
            id: emailField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: qsTr("Email")
            placeholderText: "name@example.com"
            maximumLength: 128
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                                | Qt.ImhNoPredictiveText
            onEdited: if (root.facade) root.facade.email = text
            onAccepted: phoneField.forceInputFocus()
        }

        AuthField {
            id: phoneField
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            label: qsTr("Phone")
            placeholderText: "+1 555 0100"
            hint: qsTr("An email or a phone number is enough.")
            maximumLength: 32
            inputMethodHints: Qt.ImhDialableCharactersOnly
            onEdited: if (root.facade) root.facade.phone = text
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            spacing: 6
            visible: methodRepeater.count > 0

            Text {
                text: qsTr("Payment method")
                color: BeyGramTheme.muted
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }

            Repeater {
                id: methodRepeater
                model: root.facade ? root.facade.methodTitles : []

                delegate: Rectangle {
                    id: methodRow

                    property var methodTitle: modelData
                    property int rowIndex: index
                    property bool chosen: root.facade !== null
                                          && root.facade.methodIndex === methodRow.rowIndex

                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: 10
                    color: methodRow.chosen
                           ? Qt.alpha(BeyGramTheme.accentSoft, 0.9)
                           : (hoverArea.containsMouse
                              ? Qt.alpha(BeyGramTheme.surface2, 0.9)
                              : Qt.alpha(BeyGramTheme.surface2, 0.5))
                    border.width: 1
                    border.color: methodRow.chosen
                                  ? Qt.alpha(BeyGramTheme.accent, 0.38)
                                  : Qt.alpha(BeyGramTheme.line, 0.7)

                    Accessible.role: Accessible.RadioButton
                    Accessible.name: methodRow.methodTitle
                    Accessible.checked: methodRow.chosen

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Item {
                            Layout.alignment: Qt.AlignVCenter
                            implicitWidth: 16
                            implicitHeight: 16

                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: "transparent"
                                border.width: 1.5
                                border.color: methodRow.chosen
                                              ? BeyGramTheme.accent
                                              : Qt.alpha(BeyGramTheme.muted, 0.8)
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                visible: methodRow.chosen
                                width: 8
                                height: 8
                                radius: 4
                                color: BeyGramTheme.accent
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: methodRow.methodTitle
                            color: BeyGramTheme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: hoverArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.facade)
                                       root.facade.selectMethod(methodRow.rowIndex)
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
        }
    }

    footer: Item {
        implicitHeight: 58

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SecondaryButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            PrimaryButton {
                text: qsTr("Pay")
                enabled: root.payable
                onClicked: if (root.facade) root.facade.pay()
            }
        }
    }
}
