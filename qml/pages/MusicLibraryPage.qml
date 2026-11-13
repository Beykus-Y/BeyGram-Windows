pragma ComponentBehavior: Bound

// MusicLibraryPage — the music library screen.
//
// The owner supplies a MusicLibraryFacade through `facade` and loads it before
// showing the page; the page only reads it and calls its edit methods. Playing
// is not a facade concern, so a row or a playlist card announces the intent
// through playRequested()/playlistPlayRequested() for the owner to handle.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: page

    property var facade: null

    /// 0 = Library, 1 = Playlists, 2 = Favorites.
    property int currentTab: 0

    /// Row/card data rebuilt from the facade. The lists are plain JS arrays so
    /// a change to the facade replaces the model in one assignment.
    property var rows: []
    property var playlistCards: []
    property int playlistSerial: 0

    readonly property string query: searchField.text.trim()
    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"
    readonly property int trackCount: facade ? facade.trackCount : 0
    readonly property int playlistCount: facade ? facade.playlistCount : 0
    readonly property bool playlistsTab: currentTab === 1
    readonly property bool favoritesTab: currentTab === 2
    readonly property bool searching: query.length > 0
    readonly property int visibleCount: playlistsTab ? playlistCards.length
                                                     : rows.length
    readonly property bool empty: visibleCount === 0

    signal playRequested(string trackId)
    signal playlistPlayRequested(string playlistId)

    // --- Model building -----------------------------------------------------

    function rebuild() {
        if (!facade) {
            rows = []
            playlistCards = []
            return
        }

        // Read the tab once: a change handler may run before every derived
        // binding above has been re-evaluated.
        const tab = currentTab
        const needle = query.toLowerCase()
        const sourcePlaylists = facade.playlists || []
        const cards = []
        for (let p = 0; p < sourcePlaylists.length; ++p) {
            const playlist = sourcePlaylists[p]
            if (needle.length === 0
                    || (playlist.title || "").toLowerCase().indexOf(needle) >= 0)
                cards.push(playlist)
        }
        playlistCards = cards

        if (tab === 1) {
            rows = []
            return
        }

        // The facade's engine owns the matching rules; a favorites view filters
        // its result further instead of duplicating the text match in QML.
        const source = needle.length > 0 ? (facade.searchTracks(query) || [])
                                         : (facade.tracks || [])
        const visible = []
        for (let i = 0; i < source.length; ++i) {
            const track = source[i]
            if (tab === 2 && !track.favorite)
                continue
            visible.push(track)
        }
        rows = visible
    }

    function trackSubtitle(track) {
        const parts = []
        if (track.artistId)
            parts.push(track.artistId)
        if (track.albumId)
            parts.push(track.albumId)
        return parts.length > 0 ? parts.join(" · ") : qsTr("Unknown artist")
    }

    function createPlaylistFromField() {
        if (!facade)
            return
        const title = playlistNameField.text.trim()
        if (title.length === 0)
            return
        playlistSerial += 1
        facade.createPlaylist("local-" + Date.now().toString(36) + "-"
                              + playlistSerial, title)
        playlistNameField.text = ""
    }

    function emptyGlyph() {
        if (searching)
            return "\uD83D\uDD0D"
        if (playlistsTab)
            return "\u2630"
        if (favoritesTab)
            return "\u2665"
        return "\u266B"
    }

    function emptyTitle() {
        if (searching)
            return qsTr("No matches")
        if (playlistsTab)
            return qsTr("No playlists yet")
        if (favoritesTab)
            return qsTr("No favorites yet")
        return qsTr("Your library is empty")
    }

    function emptyDescription() {
        if (searching) {
            return playlistsTab
                ? qsTr("No playlist matches \"%1\".").arg(query)
                : qsTr("No track matches \"%1\".").arg(query)
        }
        if (playlistsTab)
            return qsTr("Create a playlist to group tracks from your library.")
        if (favoritesTab)
            return qsTr("Tap the heart on a track to keep it here.")
        return qsTr("Tracks you add to the library will appear here.")
    }

    function emptyActionText() {
        if (searching)
            return qsTr("Clear search")
        if (playlistsTab)
            return qsTr("New playlist")
        return ""
    }

    function focusPlaylistName() {
        currentTab = 1
        Qt.callLater(function() { playlistNameField.forceActiveFocus() })
    }

    onFacadeChanged: rebuild()
    onQueryChanged: rebuild()
    onCurrentTabChanged: {
        tabs.currentIndex = currentTab
        rebuild()
    }
    Component.onCompleted: rebuild()

    Connections {
        target: page.facade
        ignoreUnknownSignals: true

        function onChanged() {
            // clear() also drops the facade query; mirror it back into the field
            // unless the user is the source of the change.
            if (page.facade && searchField.text !== page.facade.query)
                searchField.text = page.facade.query
            page.rebuild()
        }
    }

    // --- Layout -------------------------------------------------------------

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Music")
            subtitle: page.searching
                      ? qsTr("%n result(s)", "", page.visibleCount)
                      : qsTr("%1 tracks · %2 playlists")
                        .arg(page.trackCount)
                        .arg(page.playlistCount)

            trailing: DsIconButton {
                icon: "\uE710"
                tooltip: qsTr("New playlist")
                onClicked: page.focusPlaylistName()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Design.space4
            spacing: Design.space3

            DsSearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: page.playlistsTab ? qsTr("Search playlists")
                                                   : qsTr("Search tracks")
                Accessible.name: placeholderText
                text: page.facade ? page.facade.query : ""
                onTextChanged: {
                    if (page.facade && page.facade.query !== text)
                        page.facade.setQuery(text)
                }
            }

            DsTabs {
                id: tabs
                Layout.fillWidth: true
                model: [qsTr("Library"), qsTr("Playlists"), qsTr("Favorites")]
                currentIndex: page.currentTab
                onActivated: (index) => page.currentTab = index
            }

            RowLayout {
                Layout.fillWidth: true
                visible: page.playlistsTab
                spacing: Design.space2

                DsTextField {
                    id: playlistNameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("New playlist name")
                    Accessible.name: qsTr("New playlist name")
                    onAccepted: page.createPlaylistFromField()
                }

                DsButton {
                    text: qsTr("Create")
                    enabled: playlistNameField.text.trim().length > 0
                    onClicked: page.createPlaylistFromField()
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: trackList
                    anchors.fill: parent
                    visible: !page.playlistsTab
                    clip: true
                    spacing: 2
                    model: page.rows
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: trackDelegate

                        required property var modelData
                        required property int index

                        width: trackList.width
                        height: page.compact ? Design.rowHeightCompact
                                             : Design.rowHeight

                        DsListRow {
                            id: trackRow
                            anchors.fill: parent
                            title: trackDelegate.modelData.title || qsTr("Untitled")
                            subtitle: page.trackSubtitle(trackDelegate.modelData)
                            onClicked: page.playRequested(trackDelegate.modelData.id)

                            leading: DsIconButton {
                                icon: "\uE768"
                                tone: "accent"
                                tooltip: qsTr("Play")
                                onClicked: page.playRequested(trackDelegate.modelData.id)
                            }

                            trailing: RowLayout {
                                spacing: Design.space1

                                Text {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: trackDelegate.modelData.duration || "0:00"
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                }

                                DsIconButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    icon: trackDelegate.modelData.favorite
                                          ? "\uEB51" : "\uEB52"
                                    tone: "plain"
                                    tooltip: trackDelegate.modelData.favorite
                                             ? qsTr("Remove from favorites")
                                             : qsTr("Add to favorites")
                                    onClicked: {
                                        if (page.facade)
                                            page.facade.toggleFavorite(
                                                trackDelegate.modelData.id)
                                    }
                                }

                                DsIconButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    icon: "\uE712"
                                    tone: "plain"
                                    tooltip: qsTr("Track actions")
                                    onClicked: {
                                        trackMenu.trackId = trackDelegate.modelData.id
                                        trackMenu.openAtCursor()
                                    }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: {
                                trackMenu.trackId = trackDelegate.modelData.id
                                trackMenu.openAtCursor()
                            }
                        }
                    }
                }

                GridView {
                    id: playlistGrid
                    anchors.fill: parent
                    visible: page.playlistsTab
                    clip: true
                    model: page.playlistCards
                    cellWidth: Math.max(220, width / Math.max(1, Math.floor(width / 260)))
                    cellHeight: 140
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Item {
                        id: playlistDelegate

                        required property var modelData
                        required property int index

                        width: playlistGrid.cellWidth
                        height: playlistGrid.cellHeight

                        DsCard {
                            anchors.fill: parent
                            anchors.margins: Design.space1
                            padding: Design.space4

                            content: ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Design.space2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Design.space2

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: playlistDelegate.modelData.title
                                            color: BeyGramTheme.text
                                            font.pixelSize: Design.fontBodyLarge
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("%n track(s)", "",
                                                       playlistDelegate.modelData.trackCount)
                                            color: BeyGramTheme.muted
                                            font.pixelSize: Design.fontSmall
                                        }
                                    }

                                    DsIconButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        icon: "\uE768"
                                        tone: "accent"
                                        tooltip: qsTr("Play playlist")
                                        onClicked: page.playlistPlayRequested(
                                                       playlistDelegate.modelData.id)
                                    }

                                    DsIconButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        icon: "\uE74D"
                                        tone: "plain"
                                        tooltip: qsTr("Delete playlist")
                                        onClicked: {
                                            if (page.facade)
                                                page.facade.deletePlaylist(
                                                    playlistDelegate.modelData.id)
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Playlist")
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }
                        }
                    }
                }

                DsEmptyState {
                    anchors.centerIn: parent
                    visible: page.empty
                    glyph: page.emptyGlyph()
                    title: page.emptyTitle()
                    description: page.emptyDescription()
                    actionText: page.emptyActionText()
                    onActionRequested: {
                        if (page.searching)
                            searchField.clearText()
                        else if (page.playlistsTab)
                            page.focusPlaylistName()
                    }
                }
            }
        }
    }

    // --- Track actions ------------------------------------------------------

    DsMenu {
        id: trackMenu
        property string trackId: ""

        DsMenuItem {
            text: qsTr("Play")
            glyph: "\uE768"
            enabled: trackMenu.trackId.length > 0
            onTriggered: page.playRequested(trackMenu.trackId)
        }

        MenuSeparator {}

        DsMenuItem {
            text: qsTr("Add to playlist…")
            glyph: "\uE710"
            enabled: trackMenu.trackId.length > 0 && page.playlistCount > 0
            onTriggered: {
                // The track menu closes as this row triggers; open the sheet
                // once the close has settled so the two popups never overlap.
                Qt.callLater(function() { playlistPickerSheet.open() })
            }
        }

        DsMenuItem {
            text: qsTr("New playlist…")
            glyph: "\uE710"
            onTriggered: page.focusPlaylistName()
        }

        MenuSeparator {}

        DsMenuItem {
            text: qsTr("Remove from library")
            glyph: "\uE74D"
            tone: "danger"
            enabled: trackMenu.trackId.length > 0
            onTriggered: {
                if (page.facade)
                    page.facade.removeTrack(trackMenu.trackId)
            }
        }
    }

    // A sheet keeps the playlist rows dynamic without mixing generated and
    // static items: the Repeater rebuilds them whenever the facade changes.
    DsSheet {
        id: playlistPickerSheet
        title: qsTr("Add to playlist")

        Repeater {
            model: page.facade ? page.facade.playlists : []

            delegate: DsListRow {
                required property var modelData

                Layout.fillWidth: true
                title: modelData.title
                subtitle: qsTr("%n track(s)", "", modelData.trackCount)
                onClicked: {
                    if (page.facade)
                        page.facade.addTrackToPlaylist(modelData.id,
                                                       trackMenu.trackId)
                    playlistPickerSheet.close()
                }
            }
        }
    }
}
