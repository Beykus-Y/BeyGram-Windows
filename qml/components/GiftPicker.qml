// Gift picker panel.
//
// Presentation only. The parent owns the GiftsFacade and turns the facade's
// sendRequested signal into a TDLib sendGift request. This file only calls the
// facade's invokables and reads its properties; it never touches TDLib, the
// network, the message store, or logging.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: root

    /// GiftsFacade from src/ui/GiftsFacade.h. Every action is guarded, so the
    /// panel renders empty and disables Send while it is null.
    property var facade: null

    readonly property var giftTitles: root.facade ? root.facade.titles : []
    readonly property var giftPrices: root.facade ? root.facade.prices : []
    readonly property int selectedIndex: root.facade ? root.facade.selectedIndex : -1
    readonly property bool canSend: root.facade ? root.facade.canSend : false

    implicitWidth: 440
    implicitHeight: content.implicitHeight + 24
    radius: 14
    color: BeyGramTheme.surface
    border.width: 1
    border.color: BeyGramTheme.line

    // The message field is filled from the facade rather than bound to it, so
    // the controller's trimming does not rewrite the text and move the cursor
    // while the user is typing a trailing space. The anonymous switch is
    // synced the same way, because a user toggle replaces its binding.
    function syncFromFacade() {
        if (!root.facade)
            return
        if (messageField && !messageField.activeFocus)
            messageField.text = root.facade.message
        if (anonymousBox)
            anonymousBox.checked = root.facade.anonymous
    }

    onFacadeChanged: syncFromFacade()
    Component.onCompleted: syncFromFacade()

    Connections {
        target: root.facade
        enabled: root.facade !== null
        function onChanged() { root.syncFromFacade() }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: "Send a gift"
            color: BeyGramTheme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 104

            ListView {
                id: strip
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: 8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.giftTitles
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: card

                    required property int index
                    required property var modelData

                    readonly property bool current: card.index === root.selectedIndex
                    readonly property string label: card.modelData
                                                    && String(card.modelData).length > 0
                        ? String(card.modelData)
                        : "Gift " + (card.index + 1)
                    readonly property string price: card.index < root.giftPrices.length
                        ? String(root.giftPrices[card.index])
                        : ""

                    width: 96
                    height: 96
                    radius: 12
                    color: card.current
                        ? Qt.alpha(BeyGramTheme.accent, 0.18)
                        : (cardArea.containsMouse ? BeyGramTheme.surface2 : "transparent")
                    border.width: 1
                    border.color: card.current
                        ? BeyGramTheme.accent
                        : Qt.alpha(BeyGramTheme.line, 0.9)

                    Accessible.role: Accessible.Button
                    Accessible.name: card.label + (card.price.length > 0
                                                   ? ", " + card.price : "")
                    Accessible.selected: card.current

                    Behavior on color { ColorAnimation { duration: 100 } }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "\uD83C\uDF81" // wrapped gift
                            font.pixelSize: 24
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.label
                            color: BeyGramTheme.text
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: card.price
                            color: card.current ? BeyGramTheme.accent : BeyGramTheme.muted
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: cardArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.facade)
                                root.facade.select(card.index)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(280, parent.width - 40))
                visible: root.giftTitles.length === 0
                text: "No gifts available"
                color: BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        TextField {
            id: messageField
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            implicitHeight: 34
            leftPadding: 12
            rightPadding: 12
            placeholderText: "Add a message (optional)"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 12
            maximumLength: 255
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            enabled: root.facade !== null
            Accessible.name: "Gift message"
            onTextEdited: {
                if (root.facade)
                    root.facade.message = text
            }
            onActiveFocusChanged: {
                if (!activeFocus)
                    root.syncFromFacade()
            }

            background: Rectangle {
                radius: 17
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: messageField.activeFocus
                    ? BeyGramTheme.accent
                    : Qt.alpha(BeyGramTheme.line, 0.9)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CheckBox {
                id: anonymousBox
                text: "Send anonymously"
                checked: root.facade ? root.facade.anonymous : false
                enabled: root.facade !== null
                palette.text: BeyGramTheme.text
                palette.windowText: BeyGramTheme.text
                Accessible.name: "Send anonymously"
                onToggled: {
                    if (root.facade)
                        root.facade.anonymous = checked
                }
            }

            Item { Layout.fillWidth: true }

            PrimaryButton {
                Layout.preferredWidth: 112
                text: "Send"
                enabled: root.canSend
                Accessible.name: "Send gift"
                onClicked: {
                    if (root.facade)
                        root.facade.send()
                }
            }
        }
    }
}
