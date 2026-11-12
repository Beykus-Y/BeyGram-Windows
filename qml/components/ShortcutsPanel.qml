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
//
// Layout comes from the design system: a DsSection header, a DsSearchField, one
// DsCard holding DsListRow entries and DsDialog for the reset.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
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
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Keyboard shortcuts")
            description: qsTr("Select a shortcut field and press the new combination. A duplicate is refused.")
        }

        DsSearchField {
            id: searchField

            Layout.fillWidth: true
            placeholderText: qsTr("Search shortcuts")
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
        }

        DsCard {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                anchors.fill: parent
                anchors.margins: Design.space4

                Flickable {
                    id: scroller

                    anchors.fill: parent
                    clip: true
                    contentWidth: width
                    contentHeight: rowColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    ColumnLayout {
                        id: rowColumn

                        width: scroller.width
                        spacing: 0

                        Repeater {
                            model: root.actions

                            delegate: ColumnLayout {
                                id: row

                                // `index` and `modelData` come from the
                                // Repeater's delegate context; copying them into
                                // properties lets the children below name them
                                // through `row`.
                                required property int index
                                required property string modelData

                                readonly property string actionText: row.modelData === undefined
                                                                      || row.modelData === null
                                                                      ? "" : String(row.modelData)

                                Layout.fillWidth: true
                                spacing: 0

                                DsListRow {
                                    Layout.fillWidth: true
                                    title: row.actionText
                                    hoverEnabled: false

                                    trailing: DsTextField {
                                        id: sequenceField

                                        width: 160
                                        horizontalAlignment: TextInput.AlignHCenter
                                        placeholderText: qsTr("Unbound")
                                        Accessible.name: qsTr("Shortcut for ") + row.actionText

                                        // Re-applied whenever the row is not
                                        // being edited, so a reset or an
                                        // external reload reaches a field the
                                        // user has typed in.
                                        Binding {
                                            target: sequenceField
                                            property: "text"
                                            value: root.sequenceAt(row.index)
                                            when: !sequenceField.activeFocus
                                            restoreMode: Binding.RestoreNone
                                        }

                                        onEditingFinished: {
                                            root.commitSequence(row.index, sequenceField.text)
                                            // Show what the facade stored:
                                            // trimmed, or the old binding when
                                            // it refused.
                                            sequenceField.text = root.sequenceAt(row.index)
                                        }

                                        Keys.onEscapePressed: (event) => {
                                            sequenceField.text = root.sequenceAt(row.index)
                                            event.accepted = true
                                        }
                                    }
                                }

                                // Hairline between rows, like a settings list.
                                DsDivider {
                                    Layout.fillWidth: true
                                    inset: Design.space3
                                    visible: row.index < root.rowCount - 1
                                }
                            }
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: Math.max(0, Math.min(260, parent.width - Design.space10))
                    visible: root.rowCount === 0
                    text: root.query.length > 0
                          ? qsTr("No shortcuts match your search")
                          : qsTr("No shortcuts to show")
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            DsBadge {
                visible: root.modified
                text: qsTr("Modified")
                tone: "accent"
                dot: true
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.modified
                      ? qsTr("Changes are not saved yet")
                      : qsTr("Shortcuts match the defaults")
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                elide: Text.ElideRight
            }

            DsButton {
                text: qsTr("Reset")
                tone: "danger"
                size: "sm"
                enabled: root.modified
                accessibleName: qsTr("Reset shortcuts to defaults")
                onClicked: resetDialog.open()
            }
        }
    }

    // The reset discards every custom binding, so it is confirmed first.
    DsDialog {
        id: resetDialog

        title: qsTr("Reset shortcuts?")
        description: qsTr("Every shortcut goes back to the combination that shipped with BeyGram.")
        primaryText: qsTr("Reset")
        secondaryText: qsTr("Cancel")
        tone: "danger"

        onAccepted: {
            if (root.facade)
                root.facade.resetToDefaults()
        }
    }
}
