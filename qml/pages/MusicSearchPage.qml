import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

// MusicSearchPage — local library search, recent queries, provider links and
// waves. The page owns no state: `facade` is a MusicSearchFacade supplied by
// the music section, and every list below is read from it.
Item {
    id: page

    /// A MusicSearchFacade. The page stays usable (empty and disabled) while
    /// the owner has not connected one yet.
    property var facade: null

    /// Forwarded from the facade so the page can be wired like any other view.
    signal playRequested(string trackId)

    /// Emitted after the page submits a query (empty when the search was
    /// cleared), so an owner can run a provider request and drive `loading`.
    signal searchRequested(string query)

    /// Emitted with the original URL once the facade accepted a provider link,
    /// so an owner can resolve it without the page exposing internal state.
    signal providerLinkAdded(string url)

    readonly property bool hasFacade: page.facade !== null && page.facade !== undefined
    readonly property bool busy: page.hasFacade && page.facade.loading
    readonly property string activeQuery: page.hasFacade ? page.facade.query : ""
    readonly property var resultItems: page.hasFacade ? page.facade.results : []
    readonly property var historyItems: page.hasFacade ? page.facade.history : []

    // waves() and providerLinks() are invokables, so their results are kept in
    // local snapshots and refreshed whenever the facade announces a change.
    property var waveItems: []
    property var providerLinkItems: []
    property string linkStatus: ""
    property bool linkAccepted: false

    // The query last submitted through this page. It lets an external clear()
    // update the field without overwriting text the user has not submitted yet.
    property string appliedQuery: ""

    function focusSearch() {
        searchField.forceActiveFocus()
        searchField.selectAll()
    }

    function syncLists() {
        if (!page.hasFacade) {
            page.waveItems = []
            page.providerLinkItems = []
            return
        }
        page.waveItems = page.facade.waves()
        page.providerLinkItems = page.facade.providerLinks()
    }

    function submitSearch(text) {
        if (!page.hasFacade)
            return
        const value = String(text === undefined || text === null ? "" : text).trim()
        searchField.text = value
        page.appliedQuery = value
        page.facade.search(value)
        page.searchRequested(value)
    }

    function useHistory(text) {
        searchField.text = String(text)
        page.submitSearch(text)
        page.focusSearch()
    }

    function clearSearch() {
        if (page.hasFacade)
            page.facade.clear()
        page.appliedQuery = ""
        searchField.text = ""
        page.searchRequested("")
        page.focusSearch()
    }

    function adoptFacadeQuery() {
        page.appliedQuery = page.hasFacade ? page.facade.query : ""
        searchField.text = page.appliedQuery
    }

    function addProviderLink() {
        if (!page.hasFacade)
            return

        const url = String(linkField.text === undefined || linkField.text === null
                           ? "" : linkField.text).trim()
        if (url.length === 0) {
            page.linkAccepted = false
            page.linkStatus = qsTr("Paste a music-service link first.")
            linkStatusTimer.restart()
            return
        }

        const before = page.facade.providerLinks().length
        page.facade.addProviderLink(url)
        page.syncLists()

        if (page.providerLinkItems.length > before) {
            page.linkAccepted = true
            page.linkStatus = qsTr("Link added.")
            linkField.text = ""
            page.providerLinkAdded(url)
        } else {
            page.linkAccepted = false
            page.linkStatus = qsTr("That link is not supported, or it is already in the list.")
        }
        linkStatusTimer.restart()
    }

    Connections {
        target: page.facade
        ignoreUnknownSignals: true

        function onChanged() {
            // clear() also drops the facade query; mirror it back into the
            // field unless the user is the source of the change.
            if (page.facade && !searchField.activeFocus
                    && page.appliedQuery !== page.facade.query) {
                page.appliedQuery = page.facade.query
                searchField.text = page.appliedQuery
            }
            page.syncLists()
        }

        function onPlayRequested(trackId) {
            page.playRequested(trackId)
        }
    }

    onFacadeChanged: {
        page.adoptFacadeQuery()
        page.syncLists()
    }
    Component.onCompleted: {
        page.adoptFacadeQuery()
        page.syncLists()
    }

    Timer {
        id: linkStatusTimer
        interval: 5000
        onTriggered: page.linkStatus = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Music search")
            subtitle: {
                if (!page.hasFacade)
                    return qsTr("Not connected")
                if (page.busy)
                    return qsTr("Searching…")
                if (page.activeQuery.length > 0)
                    return qsTr("%n result(s)", "", page.resultItems.length)
                return qsTr("Search your library or keep a link from a music service")
            }

            trailing: DsIconButton {
                icon: "\uE894"
                tooltip: qsTr("Clear search")
                visible: page.activeQuery.length > 0 || searchField.text.length > 0
                onClicked: page.clearSearch()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.max(280, Math.min(880, page.width - Design.space8 * 2))
                x: Math.max(Design.space4, (page.width - width) / 2)
                y: Design.space6
                spacing: Design.space6

                // --- Search field -------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    DsSearchField {
                        id: searchField

                        Layout.fillWidth: true
                        Layout.preferredHeight: Design.controlHeightLarge + Design.space1
                        implicitHeight: Design.controlHeightLarge + Design.space1
                        leftPadding: Design.space8 + Design.space3
                        rightPadding: searchField.text.length > 0 ? 46 : Design.space5
                        font.pixelSize: Design.fontBodyLarge
                        enabled: page.hasFacade
                        placeholderText: qsTr("Search tracks, artists or albums — press Enter")
                        Accessible.name: qsTr("Search your music library")

                        onAccepted: page.submitSearch(searchField.text)
                        onCleared: page.clearSearch()
                        onTextEdited: {
                            // Backspacing the last character is a clear too, so
                            // stale results never sit under an empty field.
                            if (searchField.text.length === 0)
                                page.clearSearch()
                        }

                        Keys.onEscapePressed: (event) => {
                            if (searchField.text.length > 0) {
                                page.clearSearch()
                                event.accepted = true
                            }
                        }
                    }

                    DsButton {
                        text: qsTr("Search")
                        size: "lg"
                        tone: "primary"
                        Layout.alignment: Qt.AlignVCenter
                        enabled: page.hasFacade && searchField.text.trim().length > 0
                        onClicked: page.submitSearch(searchField.text)
                    }
                }

                // --- Recent searches ----------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    visible: page.historyItems.length > 0
                    title: qsTr("Recent searches")
                    description: qsTr("Tap a chip to run that search again.")

                    content: Flow {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Repeater {
                            model: page.historyItems

                            delegate: DsChip {
                                required property var modelData

                                text: String(modelData)
                                selected: page.activeQuery.toLowerCase()
                                          === String(modelData).toLowerCase()
                                onClicked: page.useHistory(modelData)
                            }
                        }
                    }
                }

                // --- Results -------------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    visible: page.hasFacade
                             && (page.activeQuery.length > 0 || page.busy)
                    title: qsTr("Results")
                    description: page.activeQuery.length > 0
                                 ? qsTr("Matches for %1").arg(page.activeQuery)
                                 : ""

                    trailing: DsBadge {
                        text: String(page.resultItems.length)
                        tone: "accent"
                    }

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsCard {
                            Layout.fillWidth: true
                            visible: page.resultItems.length > 0
                            padding: Design.space1
                            spacing: 0

                            content: ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Repeater {
                                    model: page.resultItems

                                    delegate: DsListRow {
                                        required property int index
                                        required property var modelData

                                        Layout.fillWidth: true
                                        title: String(modelData)
                                        Accessible.description: qsTr("Play track")

                                        leading: Rectangle {
                                            width: 36
                                            height: 36
                                            radius: width / 2
                                            color: BeyGramTheme.accentSoft
                                            border.width: 1
                                            border.color: Qt.alpha(BeyGramTheme.accent, 0.20)

                                            DsIcon {
                                                anchors.centerIn: parent
                                                name: "\uE8D6"
                                                size: 16
                                                color: BeyGramTheme.accentInk
                                                filled: true
                                            }
                                        }

                                        trailing: DsIconButton {
                                            icon: "\uE768"
                                            tone: "plain"
                                            tooltip: qsTr("Play")
                                            onClicked: {
                                                if (page.hasFacade)
                                                    page.facade.playResult(index)
                                            }
                                        }

                                        onClicked: {
                                            if (page.hasFacade)
                                                page.facade.playResult(index)
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 72
                            visible: page.busy && page.resultItems.length === 0

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: Design.space2

                                DsSpinner {
                                    running: page.busy && page.resultItems.length === 0
                                }

                                Text {
                                    text: qsTr("Searching…")
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontBody
                                }
                            }
                        }

                        DsEmptyState {
                            Layout.fillWidth: true
                            visible: !page.busy && page.resultItems.length === 0
                            glyph: "\u266B"
                            title: qsTr("No tracks found")
                            description: qsTr("Try a different title, artist or album. Searches run locally against your music library.")
                        }
                    }
                }

                // --- Empty welcome ------------------------------------------
                DsEmptyState {
                    Layout.fillWidth: true
                    visible: page.hasFacade && !page.busy
                             && page.activeQuery.length === 0
                             && page.historyItems.length === 0
                    glyph: "\u266B"
                    title: qsTr("Find something to play")
                    description: qsTr("Search your library by track title, artist or album. Recent searches stay here as chips.")
                }

                // --- Provider links -----------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: qsTr("Music service links")
                    description: qsTr("Keep a Yandex Music, Spotify, Apple Music, SoundCloud or YouTube Music link. It is validated locally and never opened here.")

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space4

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Design.space2

                                DsTextField {
                                    id: linkField

                                    Layout.fillWidth: true
                                    enabled: page.hasFacade
                                    placeholderText: qsTr("https://open.spotify.com/track/…")
                                    Accessible.name: qsTr("Music service link")
                                    onAccepted: page.addProviderLink()
                                }

                                DsButton {
                                    text: qsTr("Add link")
                                    tone: "secondary"
                                    enabled: page.hasFacade
                                             && linkField.text.trim().length > 0
                                    onClicked: page.addProviderLink()
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: page.linkStatus.length > 0
                                text: page.linkStatus
                                color: page.linkAccepted ? BeyGramTheme.okInk : BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: page.providerLinkItems.length === 0
                                         && page.linkStatus.length === 0
                                text: qsTr("No links added yet.")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                visible: page.providerLinkItems.length > 0

                                Repeater {
                                    model: page.providerLinkItems

                                    delegate: Item {
                                        required property var modelData

                                        Layout.fillWidth: true
                                        implicitHeight: 34

                                        RowLayout {
                                            anchors.fill: parent
                                            spacing: Design.space2

                                            DsIcon {
                                                Layout.alignment: Qt.AlignVCenter
                                                name: "\uE71B"
                                                size: 14
                                                color: BeyGramTheme.accent
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignVCenter
                                                text: String(modelData)
                                                color: BeyGramTheme.text
                                                font.pixelSize: Design.fontSmall
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- Waves ---------------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    visible: page.waveItems.length > 0
                    title: qsTr("Waves")
                    description: qsTr("Radio stations offered by the connected music services.")

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space1
                        spacing: 0

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Repeater {
                                model: page.waveItems

                                delegate: DsListRow {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    title: String(modelData)
                                    subtitle: qsTr("Radio station")
                                    hoverEnabled: false

                                    leading: Rectangle {
                                        width: 36
                                        height: 36
                                        radius: width / 2
                                        color: BeyGramTheme.surface2
                                        border.width: 1
                                        border.color: BeyGramTheme.line

                                        DsIcon {
                                            anchors.centerIn: parent
                                            name: "\uE8D6"
                                            size: 16
                                            color: BeyGramTheme.muted
                                        }
                                    }

                                    trailing: DsBadge {
                                        text: qsTr("Wave")
                                        tone: "muted"
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
