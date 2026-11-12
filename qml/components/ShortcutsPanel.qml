// ShortcutsPanel - editor for the application's keyboard shortcuts.
//
// Presentation only. The owner assigns `facade` (beygram::ui::ShortcutsFacade),
// which owns the rows, the filter and the shipped defaults. This file reads
// facade.actions / facade.sequences / facade.query / facade.modified, writes
// facade.query, facade.rebind(index, sequence) and facade.resetToDefaults(), and
// does nothing else: it never touches TDLib, the settings store or logging.
//
// The two lists are parallel and already filtered by the facade, so a row's
// index in the repeater is also the index rebind() expects. A row is labelled
// with its action id because that is the stable name the facade exposes;
// searching by the human-readable description is done inside the facade, which
// is why the search field can find "Reply to message" by typing "reply".

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: root

    /// The ShortcutsFacade driving this panel. May be null while the page is
    /// still being constructed, so every binding below is null-safe.
    property var facade: null

    readonly property var actions: root.facade ? root.facade.actions : []
    readonly property var sequences: root.facade ? root.facade.sequences : []
    readonly property string query: root.facade ? root.facade.query : ""
    readonly property bool modified: root.facade ? root.facade.modified : false
    readonly property int rowCount: root.actions.length

    implicitWidth: 440
    implicitHeight: 460
    radius: 14
    color: BeyGramTheme.surface
    border.width: 1
    border.color: BeyGramTheme.line

    /// The sequence shown for a visible row, or "" when the index has no row.
    function sequenceAt(rowIndex) {
        const value = root.sequences[rowIndex]
        return value === undefined || value === null ? "" : String(value)
    }

    /// Hands one edited sequence to the facade and returns whether it was
    /// accepted. The facade trims the text and refuses a duplicate, an empty
    /// sequence or a stale index, so the caller must re-read the stored value
    /// afterwards instead of trusting the field's text.
    function commitSequence(rowIndex, text) {
        if (!root.facade)
            return false
        return root.facade.rebind(rowIndex, text)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: "Keyboard shortcuts"
                color: BeyGramTheme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            // The dot is the only hint that the table no longer matches what
            // shipped; the Reset button carries the same state through enabled.
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                visible: root.modified
                width: 6
                height: 6
                radius: 3
                color: BeyGramTheme.accent
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            implicitHeight: 34
            leftPadding: 13
            rightPadding: 13
            placeholderText: "Search shortcuts"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 12
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            Accessible.name: "Search shortcuts"
            // The facade stores the query as typed, so the binding never fights
            // the user mid-word.
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
                radius: 17
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
                contentHeight: rowColumn.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: rowColumn
                    width: scroller.width
                    spacing: 0

                    Repeater {
                        model: root.actions

                        delegate: Rectangle {
                            id: row

                            // `index` and `modelData` come from the Repeater's
                            // delegate context; copying them into properties
                            // lets the children below name them through `row`.
                            readonly property int rowIndex: index
                            readonly property string actionText: modelData === undefined
                                                                || modelData === null
                                                                ? "" : String(modelData)

                            width: rowColumn.width
                            height: 46
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 2
                                anchors.rightMargin: 2
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: row.actionText
                                    color: BeyGramTheme.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                TextField {
                                    id: sequenceField
                                    Layout.preferredWidth: 150
                                    Layout.alignment: Qt.AlignVCenter
                                    implicitHeight: 30
                                    leftPadding: 10
                                    rightPadding: 10
                                    horizontalAlignment: TextInput.AlignHCenter
                                    placeholderText: "Unbound"
                                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.7)
                                    color: BeyGramTheme.text
                                    font.pixelSize: 11
                                    selectByMouse: true
                                    selectionColor: BeyGramTheme.accent
                                    selectedTextColor: BeyGramTheme.onAccent
                                    Accessible.name: "Shortcut for " + row.actionText

                                    // Re-applied whenever the row is not being
                                    // edited, so a reset or an external reload
                                    // reaches a field the user has typed in.
                                    Binding {
                                        target: sequenceField
                                        property: "text"
                                        value: root.sequenceAt(row.rowIndex)
                                        when: !sequenceField.activeFocus
                                    }

                                    onEditingFinished: {
                                        root.commitSequence(row.rowIndex, sequenceField.text)
                                        // Show what the facade stored: trimmed,
                                        // or the old binding when it refused.
                                        sequenceField.text = root.sequenceAt(row.rowIndex)
                                    }

                                    Keys.onEscapePressed: (event) => {
                                        sequenceField.text = root.sequenceAt(row.rowIndex)
                                        event.accepted = true
                                    }

                                    background: Rectangle {
                                        radius: 9
                                        color: BeyGramTheme.surface2
                                        border.width: 1
                                        border.color: sequenceField.activeFocus
                                                      ? BeyGramTheme.accent
                                                      : Qt.alpha(BeyGramTheme.line, 0.9)
                                        Behavior on border.color { ColorAnimation { duration: 120 } }
                                    }
                                }
                            }

                            // Hairline between rows, like a settings list.
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: Qt.alpha(BeyGramTheme.line, 0.6)
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(260, parent.width - 40))
                visible: root.rowCount === 0
                text: root.query.length > 0 ? "No shortcuts match your search"
                                            : "No shortcuts to show"
                color: BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.modified ? "Changes are not saved yet"
                                    : "Shortcuts match the defaults"
                color: BeyGramTheme.muted
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            SecondaryButton {
                text: "Reset"
                enabled: root.modified
                Accessible.name: "Reset shortcuts to defaults"
                onClicked: {
                    if (root.facade)
                        root.facade.resetToDefaults()
                }
            }
        }
    }
}
