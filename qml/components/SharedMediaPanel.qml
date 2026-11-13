// SharedMediaPanel.qml — the shared-media browser of a chat profile.
//
// Presentation only. Every row comes from `facade` (a SharedMediaFacade): the
// tab strip reads facade.tabs and drives facade.selectTab(), the page reads
// facade.ids and facade.titles, and a tile or row calls facade.activate(index),
// which reports the chosen message through itemActivated(). The panel never
// loads a file, sorts a page or invents a tab.
//
// Photos, Videos and GIFs render as a tile grid; Files, Links, Voice and Music
// render as list rows. An empty page shows DsEmptyState with copy that names
// the current tab. The facade is nullable so the panel can be created before
// the controller is wired up; until it is set, the panel renders the empty
// state.
//
// The tab indices are the order of SharedMediaTab: Photos, Videos, Files,
// Links, Voice, Music, GIFs.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// A SharedMediaFacade, or null before the controller is available.
    property var facade: null

    readonly property var tabTitles: root.facade ? root.facade.tabs : []
    readonly property var itemIds: root.facade ? root.facade.ids : []
    readonly property var itemTitles: root.facade ? root.facade.titles : []
    readonly property int currentTab: root.facade ? root.facade.currentTab : 0
    readonly property int itemCount: root.itemIds ? root.itemIds.length : 0
    readonly property bool gridMode: root.isGridTab(root.currentTab)

    /// Preferred tile width; the grid fits as many columns as the panel allows.
    readonly property int preferredCellWidth: 128

    implicitWidth: 420
    implicitHeight: 480

    function isGridTab(tab) {
        switch (tab) {
        case 0: // Photos
        case 1: // Videos
        case 6: // GIFs
            return true
        default:
            return false
        }
    }

    function tabGlyph(tab) {
        switch (tab) {
        case 0:
            return "\uE91B" // Photos
        case 1:
            return "\uE714" // Videos
        case 2:
            return "\uE8A5" // Files
        case 3:
            return "\uE71B" // Links
        case 4:
            return "\uE720" // Voice
        case 5:
            return "\uE8D6" // Music
        default:
            return "\uE786" // GIFs
        }
    }

    function idAt(index) {
        if (index < 0 || index >= root.itemIds.length)
            return ""
        return String(root.itemIds[index] || "")
    }

    function labelAt(index) {
        const title = index >= 0 && index < root.itemTitles.length
                      ? String(root.itemTitles[index] || "")
                      : ""
        if (title.length > 0)
            return title

        const id = root.idAt(index)
        return id.length > 0 ? qsTr("Message %1").arg(id) : qsTr("Attachment")
    }

    function emptyTitleFor(tab) {
        switch (tab) {
        case 0:
            return qsTr("No photos yet")
        case 1:
            return qsTr("No videos yet")
        case 2:
            return qsTr("No files yet")
        case 3:
            return qsTr("No links yet")
        case 4:
            return qsTr("No voice messages yet")
        case 5:
            return qsTr("No music yet")
        default:
            return qsTr("No GIFs yet")
        }
    }

    function emptyDescriptionFor(tab) {
        switch (tab) {
        case 0:
            return qsTr("Photos shared in this chat will appear here.")
        case 1:
            return qsTr("Videos shared in this chat will appear here.")
        case 2:
            return qsTr("Files shared in this chat will appear here.")
        case 3:
            return qsTr("Links shared in this chat will appear here.")
        case 4:
            return qsTr("Voice messages shared in this chat will appear here.")
        case 5:
            return qsTr("Music shared in this chat will appear here.")
        default:
            return qsTr("GIFs shared in this chat will appear here.")
        }
    }

    function activate(index) {
        if (root.facade)
            root.facade.activate(index)
    }

    // DsTabs assigns its own currentIndex on a click, which replaces the
    // binding above, so the strip is re-synced whenever the facade reports a
    // change (including a tab switch requested from C++).
    onFacadeChanged: tabs.currentIndex = root.currentTab

    Connections {
        target: root.facade

        function onChanged() {
            tabs.currentIndex = root.currentTab
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Design.space2

        DsTabs {
            id: tabs

            Layout.fillWidth: true
            model: root.tabTitles
            currentIndex: root.currentTab

            onActivated: function (index) {
                if (root.facade)
                    root.facade.selectTab(index)
            }
        }

        DsDivider {
            Layout.fillWidth: true
        }

        Item {
            id: page

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Tile grid for the visual tabs. The facade carries no thumbnail
            // URL, so a tile shows the tab glyph and the prepared title.
            Flickable {
                id: gridScroll

                anchors.fill: parent
                visible: root.gridMode && root.itemCount > 0
                clip: true
                contentWidth: width
                contentHeight: grid.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Grid {
                    id: grid

                    width: gridScroll.width
                    spacing: Design.space2
                    columns: Math.max(1, Math.floor((width + spacing)
                                                    / (root.preferredCellWidth + spacing)))

                    Repeater {
                        model: root.itemIds

                        delegate: Rectangle {
                            id: tile

                            required property int index

                            readonly property string label: root.labelAt(tile.index)

                            width: Math.floor((grid.width - grid.spacing * (grid.columns - 1))
                                              / grid.columns)
                            height: Math.round(width * 0.86)
                            radius: Design.rowRadius(BeyGramAppearance.designStyle)
                            color: tileArea.containsMouse ? BeyGramTheme.surface2
                                                          : Qt.alpha(BeyGramTheme.surface2, 0.55)
                            border.width: tile.activeFocus ? 2 : 1
                            border.color: tile.activeFocus
                                          ? BeyGramTheme.accent
                                          : (tileArea.containsMouse
                                             ? Qt.alpha(BeyGramTheme.accent, 0.45)
                                             : BeyGramTheme.line)
                            clip: true
                            activeFocusOnTab: true

                            Accessible.role: Accessible.Button
                            Accessible.name: tile.label

                            Behavior on color {
                                ColorAnimation {
                                    duration: Design.motion(Design.motionFast,
                                                            BeyGramAppearance.reduceMotion)
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Behavior on border.color {
                                ColorAnimation {
                                    duration: Design.motion(Design.motionFast,
                                                            BeyGramAppearance.reduceMotion)
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Keys.onReturnPressed: root.activate(tile.index)
                            Keys.onEnterPressed: root.activate(tile.index)
                            Keys.onSpacePressed: root.activate(tile.index)

                            DsIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: -Design.space3
                                name: root.tabGlyph(root.currentTab)
                                size: Design.iconSizeLarge
                                color: BeyGramTheme.muted
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: Math.min(28, tile.height)
                                color: Qt.alpha(BeyGramTheme.surface, 0.90)

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: Design.space2
                                    anchors.rightMargin: Design.space2
                                    text: tile.label
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontSmall
                                    font.weight: Font.Medium
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: tileArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor

                                onClicked: {
                                    tile.forceActiveFocus()
                                    root.activate(tile.index)
                                }
                            }
                        }
                    }
                }
            }

            // Compact rows for tabs whose titles carry the useful detail.
            Flickable {
                id: listScroll

                anchors.fill: parent
                visible: !root.gridMode && root.itemCount > 0
                clip: true
                contentWidth: width
                contentHeight: listColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Column {
                    id: listColumn

                    width: listScroll.width
                    spacing: 0

                    Repeater {
                        model: root.itemIds

                        delegate: DsListRow {
                            id: row

                            required property int index

                            width: listColumn.width
                            title: root.labelAt(row.index)

                            leading: DsIcon {
                                name: root.tabGlyph(root.currentTab)
                                size: Design.iconSize
                                color: BeyGramTheme.muted
                            }

                            trailing: DsIcon {
                                name: "\uE76C" // Chevron
                                size: 14
                                color: BeyGramTheme.muted
                            }

                            onClicked: root.activate(row.index)
                        }
                    }
                }
            }

            DsEmptyState {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(parent.width - Design.space6 * 2, implicitWidth))
                visible: root.itemCount === 0
                title: root.emptyTitleFor(root.currentTab)
                description: root.emptyDescriptionFor(root.currentTab)
            }
        }
    }
}
