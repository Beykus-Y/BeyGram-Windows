// BotKeyboard.qml — the bot surfaces of a chat: a wrap of command buttons and
// the inline-result list of the open inline query.
//
// Presentation only. The parent assigns `facade` (a BotsFacade), which owns the
// command menu, the inline session and the Web App session; this component
// keeps no state of its own and never talks to TDLib, never sends a message and
// never opens a URL. A command tap emits the facade's commandActivated() with
// the bare command name for the host to insert or send, an inline tap calls
// selectInlineResult(), and the query field calls setInlineQuery().
//
// The Web App properties (webAppActive / webAppTitle) are deliberately not
// rendered here: a Mini App takes over the chat surface and is owned by the
// page hosting it, not by the composer's keyboard strip.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    /// BotsFacade from the C++ side. A null facade renders an empty, inert
    /// strip instead of failing.
    property var facade: null

    readonly property var commandNames: root.facade ? root.facade.commands : []
    readonly property var inlineTitles: root.facade ? root.facade.inlineTitles : []
    readonly property bool inlineActive: root.facade ? root.facade.inlineActive : false

    implicitWidth: 360
    implicitHeight: column.implicitHeight

    // The field mirrors the facade, but not while the user is typing: pushing
    // every keystroke back into `text` would move the caret and undo the trim
    // mid-word. Losing focus re-syncs, so a trailing space the facade trimmed
    // does not stay on screen.
    function syncQuery() {
        if (queryField && !queryField.activeFocus)
            queryField.text = root.facade ? root.facade.inlineQuery : ""
    }

    onFacadeChanged: root.syncQuery()

    Component.onCompleted: root.syncQuery()

    Connections {
        target: root.facade

        function onChanged() {
            root.syncQuery()
        }
    }

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 8

        // One pill per advertised command, wrapped across rows. The label keeps
        // the slash the composer shows; the signal carries the bare name, as
        // BotsFacade stores it.
        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: root.commandNames.length > 0

            Repeater {
                model: root.commandNames

                delegate: ChoiceChip {
                    required property string modelData

                    text: "/" + modelData
                    onClicked: {
                        if (root.facade)
                            root.facade.commandActivated(modelData)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.inlineActive

            TextField {
                id: queryField

                Layout.fillWidth: true
                Layout.preferredHeight: 34
                implicitHeight: 34
                leftPadding: 13
                rightPadding: 13
                placeholderText: qsTr("Search inline results")
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                color: BeyGramTheme.text
                font.pixelSize: 12
                selectByMouse: true
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                Accessible.name: qsTr("Inline query")

                onTextEdited: {
                    if (root.facade)
                        root.facade.setInlineQuery(text)
                }
                onActiveFocusChanged: {
                    if (!activeFocus)
                        root.syncQuery()
                }
            }

            SecondaryButton {
                text: qsTr("Close")
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    if (root.facade)
                        root.facade.closeInline()
                }
            }
        }

        // The bot's answer, one row per result in the order the facade keeps.
        // Only the title is shown; the id stays in C++ and is reported by
        // selectInlineResult() when a row is tapped.
        ListView {
            id: resultList

            visible: root.inlineActive && root.inlineTitles.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, root.inlineTitles.length * 40)
            clip: true
            spacing: 2
            model: root.inlineTitles
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ItemDelegate {
                id: resultRow

                required property int index
                required property string modelData

                width: ListView.view ? ListView.view.width : 0
                height: 38
                Accessible.role: Accessible.ListItem
                Accessible.name: resultRow.modelData.length > 0
                                 ? resultRow.modelData
                                 : qsTr("Inline result %1").arg(resultRow.index + 1)

                onClicked: {
                    if (root.facade)
                        root.facade.selectInlineResult(resultRow.index)
                }

                background: Rectangle {
                    radius: 10
                    color: resultRow.hovered ? BeyGramTheme.surface2 : "transparent"
                }

                contentItem: Text {
                    text: resultRow.modelData.length > 0
                          ? resultRow.modelData
                          : qsTr("Untitled result")
                    color: BeyGramTheme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
