// QuickSwitcher.qml — full-screen command palette for jumping between chats.
//
// Presentation only. The owner assigns `facade` (beygram::ui::QuickSwitcherFacade),
// which owns the catalog, the filter, the ranking, the selected row and the open
// state. This file renders facade.titles / facade.selectedIndex, writes
// facade.setQuery(), and calls hide(), moveSelection() and activateCurrent().
// It never searches, sorts or navigates on its own, and it never opens a chat:
// the facade's chatActivated() tells the host which one to open.
//
// The host calls facade.show() from its shortcut. show() clears the query, so
// the palette always opens on the full chat list; typing filters it. Up/Down
// wrap through the visible rows, Enter activates the highlighted one, and
// Escape (or a press on the scrim) closes the palette.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The QuickSwitcherFacade instance, or null while the shell is loading.
    property var facade: null

    readonly property bool ready: root.facade !== null && root.facade !== undefined
    readonly property bool open: root.ready && root.facade.open
    readonly property string query: root.ready ? root.facade.query : ""
    readonly property var titles: root.ready ? root.facade.titles : []
    readonly property int selectedIndex: root.ready ? root.facade.selectedIndex : -1
    readonly property int rowCount: root.titles.length
    readonly property bool filtering: root.query.trim().length > 0

    /// One result row. A compact palette is denser than a settings list.
    readonly property int rowHeight: Design.controlHeightLarge
    readonly property int listContentHeight: root.rowCount > 0
        ? root.rowCount * root.rowHeight + (root.rowCount - 1) * Design.space1
        : 0

    visible: root.open
    z: 950
    focus: root.open
    anchors.fill: parent

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Quick switcher")

    function close() {
        if (root.ready)
            root.facade.hide()
    }

    function move(delta) {
        if (root.ready)
            root.facade.moveSelection(delta)
    }

    function activateCurrent() {
        // The open check also makes a duplicated Enter delivery harmless: the
        // first handler closes the palette, the second one is ignored.
        if (root.ready && root.open)
            root.facade.activateCurrent()
    }

    /// Selects the clicked row and activates it. The facade only exposes
    /// relative movement, so a click walks from the current row to the clicked
    /// one; moveSelection() wraps and therefore always lands on the target.
    function activateRow(index) {
        if (!root.ready || index < 0 || index >= root.rowCount)
            return

        const delta = index - root.facade.selectedIndex
        if (delta !== 0)
            root.facade.moveSelection(delta)
        root.activateCurrent()
    }

    /// Mirrors the facade's query into the field. The field is not two-way
    /// bound: typing writes through setQuery(), while show() and the clear
    /// button arrive through changed() and land here.
    function syncQuery() {
        if (!root.ready || !searchField)
            return
        if (searchField.text !== root.facade.query)
            searchField.text = root.facade.query
    }

    function focusSearch() {
        if (root.open && searchField)
            searchField.forceActiveFocus()
    }

    function ensureSelectionVisible() {
        if (!root.open || !root.ready || !results)
            return
        const index = root.facade.selectedIndex
        if (index >= 0 && index < results.count)
            results.positionViewAtIndex(index, ListView.Contain)
    }

    onVisibleChanged: {
        if (root.open)
            Qt.callLater(root.focusSearch)
    }

    onFacadeChanged: {
        root.syncQuery()
        if (root.open) {
            Qt.callLater(root.focusSearch)
            Qt.callLater(root.ensureSelectionVisible)
        }
    }

    Component.onCompleted: {
        root.syncQuery()
        if (root.open) {
            Qt.callLater(root.focusSearch)
            Qt.callLater(root.ensureSelectionVisible)
        }
    }

    Connections {
        target: root.facade
        ignoreUnknownSignals: true

        function onChanged() {
            if (!root.ready)
                return
            root.syncQuery()
            // The list model may reset in the same signal delivery; defer the
            // scroll until the delegates have caught up.
            if (root.open)
                Qt.callLater(root.ensureSelectionVisible)
        }
    }

    // Fallback for when the palette itself holds focus. The search field
    // forwards the same keys while it is focused, which is the normal case.
    Keys.onUpPressed: (event) => {
        root.move(-1)
        event.accepted = true
    }
    Keys.onDownPressed: (event) => {
        root.move(1)
        event.accepted = true
    }
    Keys.onReturnPressed: (event) => {
        root.activateCurrent()
        event.accepted = true
    }
    Keys.onEnterPressed: (event) => {
        root.activateCurrent()
        event.accepted = true
    }
    Keys.onEscapePressed: (event) => {
        root.close()
        event.accepted = true
    }

    // Full-bleed scrim. Clicks on the card also reach this MouseArea because
    // the card has no click handler of its own, so only presses outside the
    // card's rectangle close the palette.
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", Design.opacityScrim)

        MouseArea {
            id: scrimArea

            anchors.fill: parent
            acceptedButtons: Qt.AllButtons

            onClicked: function (mouse) {
                if (!root.ready)
                    return
                const point = palette.mapFromItem(scrimArea, mouse.x, mouse.y)
                if (point.x < 0 || point.y < 0
                        || point.x > palette.width || point.y > palette.height)
                    root.close()
            }

            onWheel: function (wheel) {
                wheel.accepted = true
            }
        }
    }

    DsCard {
        id: palette

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(Design.space8, Math.round(root.height * 0.12))
        width: Math.min(Design.space10 * 14,
                        Math.max(0, root.width - Design.space8))
        padding: Design.space3
        spacing: Design.space2

        /// The list never grows past this, so the card stays inside a short
        /// window. 144 px is the card chrome: padding, search field, divider,
        /// spacing, hint and the bottom margin outside the card.
        readonly property int maxListHeight: Math.max(
            Design.controlHeightLarge,
            Math.min(Design.space10 * 9,
                     root.height - palette.y
                     - (Design.space10 * 3 + Design.space6)))

        content: ColumnLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            DsSearchField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTr("Search chats")
                Accessible.name: qsTr("Search chats")

                onTextEdited: {
                    if (root.ready)
                        root.facade.setQuery(searchField.text)
                }

                // The trailing clear button empties the field itself, so the
                // facade has to be told explicitly; textEdited is not emitted
                // for a programmatic clear.
                onCleared: {
                    if (root.ready)
                        root.facade.setQuery("")
                }

                onAccepted: root.activateCurrent()

                Keys.onUpPressed: (event) => {
                    root.move(-1)
                    event.accepted = true
                }
                Keys.onDownPressed: (event) => {
                    root.move(1)
                    event.accepted = true
                }
                Keys.onEscapePressed: (event) => {
                    root.close()
                    event.accepted = true
                }
            }

            DsDivider {
                Layout.fillWidth: true
            }

            Item {
                id: listSlot

                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                    palette.maxListHeight,
                    Math.max(root.rowHeight, root.listContentHeight))
                visible: root.rowCount > 0

                ListView {
                    id: results

                    anchors.fill: parent
                    clip: true
                    spacing: Design.space1
                    model: root.titles
                    boundsBehavior: Flickable.StopAtBounds
                    activeFocusOnTab: false
                    keyNavigationEnabled: false
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Chats")

                    delegate: DsListRow {
                        id: row

                        required property int index
                        required property string modelData

                        width: results.width
                        height: root.rowHeight
                        title: row.modelData.length > 0
                               ? row.modelData : qsTr("Untitled chat")
                        selected: row.index === root.selectedIndex
                        Accessible.selected: row.index === root.selectedIndex

                        onClicked: root.activateRow(row.index)
                    }
                }
            }

            Item {
                id: emptySlot

                Layout.fillWidth: true
                Layout.preferredHeight: Design.space10 * 3
                visible: root.rowCount === 0

                DsEmptyState {
                    anchors.centerIn: parent
                    width: Math.max(0, Math.min(parent.width - Design.space4,
                                                implicitWidth))
                    title: root.filtering ? qsTr("No matches") : qsTr("No chats yet")
                    description: root.filtering
                                 ? qsTr("Try a different name or clear the search.")
                                 : qsTr("Chats will appear here once they are loaded.")
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.rowCount > 0
                      ? qsTr("Up/Down to navigate · Enter to open · Esc to close")
                      : qsTr("Esc to close")
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }
}
