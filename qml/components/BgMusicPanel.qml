// BgMusicPanel - the background-music playlist manager.
//
// Presentation only. The owner assigns `facade` (beygram::ui::BgMusicFacade),
// which owns the playlist strip, the imported entries and the selection. This
// file reads facade.playlistIds / playlistTitles / entryCount / selectedId,
// writes facade.select(id), and calls facade.importFromText(fileName, text),
// facade.exportJson() and facade.exportM3u(). It never reads a file, touches
// TDLib or logs.
//
// Import pastes text rather than opening a file: the format segment chooses the
// extension the facade's parser sees, and the dialog stays open with the
// facade's rejection reason when the paste cannot be imported. Export shows the
// serialized playlist for copying; saving it is the owner's job.
//
// Layout comes from the design system: a DsSection header, a DsCard list of
// DsListRow entries, DsEmptyState when no playlist exists, DsDialog for import
// and export, and DsToast for copy feedback.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// A BgMusicFacade, or null before the controller is wired up.
    property var facade: null

    readonly property var playlistIds: root.facade ? root.facade.playlistIds : []
    readonly property var playlistTitles: root.facade ? root.facade.playlistTitles : []
    readonly property string selectedId: root.facade ? root.facade.selectedId : ""
    readonly property int entryCount: root.facade ? root.facade.entryCount : 0
    readonly property int playlistCount: root.playlistIds.length

    /// The facade's last import rejection, already turned into display text.
    property string importError: ""
    /// The serialized playlist shown by the export dialog.
    property string exportPayload: ""
    property string exportLabel: "JSON"
    property string exportExtension: "json"

    implicitWidth: 480
    implicitHeight: 560

    /// The row label, falling back to the id and then to a placeholder so a
    /// published playlist with no title is still clickable and readable.
    function titleAt(index) {
        if (index < 0 || index >= root.playlistIds.length)
            return ""

        const titleValue = index < root.playlistTitles.length
                           ? root.playlistTitles[index] : ""
        const title = titleValue === undefined || titleValue === null
                      ? "" : String(titleValue)
        if (title.trim().length > 0)
            return title

        const idValue = root.playlistIds[index]
        const id = idValue === undefined || idValue === null ? "" : String(idValue)
        return id.length > 0 ? id : qsTr("Untitled playlist")
    }

    function trackCountText(count) {
        return qsTr("%n track(s)", "", count)
    }

    readonly property string selectionSummary: {
        if (!root.facade || root.selectedId.length === 0)
            return qsTr("Select a playlist to see its track count")

        const index = root.playlistIds.indexOf(root.selectedId)
        const title = index >= 0 ? root.titleAt(index) : root.selectedId
        return qsTr("%1 · %2").arg(title).arg(root.trackCountText(root.entryCount))
    }

    /// The facade's reason tags are machine-readable, so the panel owns the
    /// user-facing wording instead of showing "no-entries" in the dialog.
    function reasonText(reason) {
        if (reason === "empty")
            return qsTr("The pasted content is empty.")
        if (reason === "too-large")
            return qsTr("The playlist is 2 MiB or larger.")
        if (reason === "unsupported-extension")
            return qsTr("Choose JSON or M3U for the pasted content.")
        if (reason === "no-entries")
            return qsTr("No usable tracks were found in the pasted content.")
        return qsTr("The playlist could not be imported.")
    }

    function openImport() {
        if (!root.facade)
            return
        importDialog.open()
    }

    function attemptImport() {
        if (!root.facade)
            return

        root.importError = ""
        const name = importNameField.text.trim()
        const stem = name.length > 0 ? name : "Pasted playlist"
        const fileName = stem + (formatPicker.currentIndex === 0 ? ".json" : ".m3u")
        if (root.facade.importFromText(fileName, importArea.text)) {
            importDialog.close()
            toast.show(qsTr("Playlist imported"), "ok")
        }
    }

    function openExport(formatIndex) {
        if (!root.facade || root.selectedId.length === 0)
            return

        if (formatIndex === 0) {
            root.exportPayload = root.facade.exportJson()
            root.exportLabel = "JSON"
            root.exportExtension = "json"
        } else {
            root.exportPayload = root.facade.exportM3u()
            root.exportLabel = "M3U"
            root.exportExtension = "m3u"
        }
        exportDialog.open()
    }

    function copyExport() {
        if (root.exportPayload.length === 0)
            return

        exportArea.selectAll()
        exportArea.copy()
        toast.show(qsTr("Copied to clipboard"), "ok")
    }

    Connections {
        target: root.facade
        ignoreUnknownSignals: true

        function onImportFailed(reason) {
            if (importDialog.visible)
                root.importError = root.reasonText(reason)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Background music")
            description: qsTr("Paste a playlist to add its tracks, then export it as JSON or M3U.")
        }

        DsCard {
            id: listCard

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220

            Item {
                anchors.fill: parent
                anchors.margins: Design.space2

                ListView {
                    id: playlistView

                    anchors.fill: parent
                    visible: root.playlistCount > 0
                    clip: true
                    model: root.playlistIds
                    spacing: Design.space1
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: DsListRow {
                        id: playlistRow

                        required property int index
                        required property string modelData

                        width: playlistView.width
                        title: root.titleAt(playlistRow.index)
                        selected: root.selectedId === playlistRow.modelData
                        onClicked: {
                            if (root.facade)
                                root.facade.select(playlistRow.modelData)
                        }

                        trailing: DsBadge {
                            visible: playlistRow.selected && root.entryCount > 0
                            text: root.entryCount.toString()
                            tone: "accent"
                        }
                    }
                }

                DsEmptyState {
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, parent.width)
                    visible: root.playlistCount === 0
                    glyph: "\u266B"
                    title: qsTr("No playlists yet")
                    description: qsTr("Paste a JSON or M3U playlist to add background music to your chats.")
                    actionText: qsTr("Import playlist")
                    onActionRequested: root.openImport()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.selectionSummary
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            DsButton {
                text: qsTr("Import playlist")
                tone: "primary"
                size: "sm"
                enabled: root.facade !== null
                onClicked: root.openImport()
            }

            Item { Layout.fillWidth: true }

            DsButton {
                text: qsTr("Export JSON")
                tone: "secondary"
                size: "sm"
                enabled: root.selectedId.length > 0
                onClicked: root.openExport(0)
            }

            DsButton {
                text: qsTr("Export M3U")
                tone: "secondary"
                size: "sm"
                enabled: root.selectedId.length > 0
                onClicked: root.openExport(1)
            }
        }
    }

    DsDialog {
        id: importDialog

        title: qsTr("Import a playlist")
        description: qsTr("Paste a playlist below. JSON uses the .json format; M3U uses .m3u or .m3u8. Under 2 MiB.")
        primaryText: ""
        secondaryText: qsTr("Close")
        width: 520

        onOpened: {
            root.importError = ""
            importNameField.text = ""
            importArea.text = ""
            formatPicker.currentIndex = 0
            importArea.forceActiveFocus()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space3

            Text {
                text: qsTr("Format")
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
            }

            DsSegmented {
                id: formatPicker
                Layout.fillWidth: true
                model: [qsTr("JSON"), qsTr("M3U")]
                currentIndex: 0
            }
        }

        DsTextField {
            id: importNameField

            Layout.fillWidth: true
            label: qsTr("Playlist name")
            placeholderText: qsTr("Optional; used when the file has no title")
            maximumLength: 80
        }

        ScrollView {
            id: importScroll

            Layout.fillWidth: true
            Layout.preferredHeight: 170
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            TextArea {
                id: importArea

                width: importScroll.availableWidth
                height: Math.max(implicitHeight, importScroll.availableHeight)
                leftPadding: Design.space3
                rightPadding: Design.space3
                topPadding: Design.space2
                bottomPadding: Design.space2
                color: BeyGramTheme.text
                placeholderText: formatPicker.currentIndex === 0
                                 ? qsTr("Paste playlist JSON here")
                                 : qsTr("Paste M3U lines here")
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: Design.fontSmall
                wrapMode: TextArea.Wrap
                selectByMouse: true

                background: Rectangle {
                    radius: Design.radiusSm
                    color: BeyGramTheme.surface2
                    border.width: 1
                    border.color: importArea.activeFocus
                                  ? BeyGramTheme.accent
                                  : BeyGramTheme.line

                    Behavior on border.color {
                        ColorAnimation { duration: Design.motionFast }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.importError.length > 0
            text: root.importError
            color: BeyGramTheme.danger
            font.pixelSize: Design.fontSmall
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            Text {
                Layout.fillWidth: true
                text: qsTr("The name labels an M3U or an untitled JSON; nothing is saved.")
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontCaption
                wrapMode: Text.WordWrap
            }

            DsButton {
                text: qsTr("Import")
                tone: "primary"
                size: "sm"
                enabled: root.facade !== null && importArea.text.trim().length > 0
                onClicked: root.attemptImport()
            }
        }
    }

    DsDialog {
        id: exportDialog

        title: qsTr("Export %1").arg(root.exportLabel)
        description: qsTr("Copy the playlist text and save it as a .%1 file. Nothing is written by BeyGram.").arg(root.exportExtension)
        primaryText: root.exportPayload.length > 0 ? qsTr("Copy") : ""
        secondaryText: qsTr("Close")
        width: 560

        onAccepted: root.copyExport()

        ScrollView {
            id: exportScroll

            Layout.fillWidth: true
            Layout.preferredHeight: 240
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            TextArea {
                id: exportArea

                width: exportScroll.availableWidth
                height: Math.max(implicitHeight, exportScroll.availableHeight)
                text: root.exportPayload
                readOnly: true
                selectByMouse: true
                wrapMode: TextArea.Wrap
                leftPadding: Design.space3
                rightPadding: Design.space3
                topPadding: Design.space2
                bottomPadding: Design.space2
                color: BeyGramTheme.text
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: Design.fontSmall

                background: Rectangle {
                    radius: Design.radiusSm
                    color: BeyGramTheme.surface2
                    border.width: 1
                    border.color: BeyGramTheme.line
                }
            }
        }
    }

    DsToast {
        id: toast
        anchors.fill: parent
        z: 100
    }
}
