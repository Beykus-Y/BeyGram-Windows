// TranslationSheet - language picker and auto-translate toggle for the
// translation settings.
//
// Presentation only. The parent assigns `facade` (a TranslationFacade), which
// owns the language list, the selection, the auto-translate preference and the
// loading state. The sheet keeps no state of its own, never talks to TDLib and
// never logs; every interaction is a call back into the facade.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Dialog {
    id: root

    /// TranslationFacade from the C++ side. A null facade renders an empty,
    /// disabled sheet instead of failing.
    property var facade: null

    readonly property var names: root.facade ? root.facade.languageNames : []
    readonly property int selectedIndex: root.facade ? root.facade.languageIndex : -1
    readonly property bool autoOn: root.facade ? root.facade.autoTranslate : false
    readonly property bool busy: root.facade ? root.facade.loading : false

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: 380
    padding: 0
    modal: true
    focus: true
    title: "Translation"
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
        spacing: 4
        implicitWidth: 380

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 2
            text: "Messages are translated into the selected language."
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        ListView {
            id: languageList

            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: root.names.length > 0
                                    ? Math.min(300, root.names.length * 48)
                                    : 0
            visible: root.names.length > 0
            clip: true
            model: root.names
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: languageRow

                required property int index
                required property string modelData

                width: languageList.width
                height: 46
                highlighted: languageRow.index === root.selectedIndex
                Accessible.name: languageRow.modelData
                Accessible.role: Accessible.ListItem

                onClicked: {
                    if (root.facade)
                        root.facade.selectLanguage(languageRow.index)
                }

                background: Rectangle {
                    radius: 8
                    color: languageRow.highlighted ? BeyGramTheme.accentSoft
                                                   : (languageRow.hovered ? BeyGramTheme.surface2
                                                                          : "transparent")
                }

                contentItem: RowLayout {
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: languageRow.modelData
                        color: BeyGramTheme.text
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: languageRow.highlighted
                        text: "✓"
                        color: BeyGramTheme.accent
                        font.pixelSize: 14
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: 12
            visible: root.names.length === 0
            text: "No languages available."
            color: BeyGramTheme.muted
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }

        // Loading row: one line while at least one message waits for a result.
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.preferredHeight: root.busy ? 40 : 0
            visible: root.busy
            radius: 8
            color: BeyGramTheme.surface2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                BusyIndicator {
                    running: root.busy
                    implicitWidth: 18
                    implicitHeight: 18
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                }

                Text {
                    Layout.fillWidth: true
                    text: "Translating message…"
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        CheckBox {
            id: autoBox

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 4
            Layout.bottomMargin: 10
            text: "Translate incoming messages automatically"
            enabled: root.facade !== null
            checked: root.autoOn
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            Accessible.name: text

            onToggled: {
                if (root.facade)
                    root.facade.setAutoTranslate(checked)
            }
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

            PrimaryButton {
                text: "Done"
                onClicked: root.close()
            }
        }
    }

    // A toggle breaks the `checked` binding, so re-sync it whenever the facade
    // reports a change from the C++ side.
    Connections {
        target: root.facade

        function onChanged() {
            autoBox.checked = root.facade.autoTranslate
        }
    }
}
