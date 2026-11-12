pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "ds/design.js" as Design

// Shell — the signed-in desktop shell.
//
// A slim, collapsible left navigation rail (72 px collapsed by default,
// 216 px expanded) houses the user avatar, the 4 primary destinations
// (Chats, Search, Profile, Settings), and a quiet footer for connection state,
// Light/Dark toggle, and rail expand/collapse. Each section page owns its own
// contextual header; there is no redundant top bar here.
Item {
    id: shell

    // --- Public API (kept for tst_qml_ui and callers) ----------------------
    property int currentSection: 0 // 0=Chats, 1=Search, 2=Profile, 3=Settings
    property bool railExpanded: false
    readonly property int railWidth: shell.railExpanded ? 216 : 72

    signal openDiagnostics()

    property int computedUnread: 0
    readonly property int unreadTotal: Math.max(
                                           shell.computedUnread,
                                           BeyGramApp.chatListControls
                                           ? Number(BeyGramApp.chatListControls.totalUnread || 0)
                                           : 0)

    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
    readonly property int _animNormal: Design.motion(Design.motionNormal, BeyGramAppearance.reduceMotion)

    readonly property string accountDisplayName: BeyGramApp.accountDisplayName.length > 0
                                                 ? BeyGramApp.accountDisplayName
                                                 : qsTr("Telegram account")
    readonly property string accountHandle: BeyGramApp.accountUsername.length > 0
                                            ? "@" + BeyGramApp.accountUsername
                                            : BeyGramApp.connectionTitle
    readonly property string accountInitials: {
        var parts = shell.accountDisplayName.trim().split(/\s+/)
        var letters = ""
        for (var i = 0; i < parts.length && letters.length < 2; ++i) {
            if (parts[i].length > 0)
                letters += parts[i].charAt(0)
        }
        return letters.length > 0 ? letters.toUpperCase() : "B"
    }

    readonly property bool isConnected: BeyGramApp.connectionTitle === "Connected"
    readonly property bool isConnecting: BeyGramApp.connectionTitle.toLowerCase().indexOf("connect") >= 0
                                         || BeyGramApp.connectionTitle.toLowerCase().indexOf("updat") >= 0
    readonly property bool accountOnline: (BeyGramApp.network && BeyGramApp.network.online)
                                          || shell.isConnected

    readonly property var sections: [
        { key: "chats", title: qsTr("Chats"), glyph: "\uE8BD", shortcut: "Ctrl+1" },
        { key: "search", title: qsTr("Search"), glyph: "\uE721", shortcut: "Ctrl+K" },
        { key: "profile", title: qsTr("Profile"), glyph: "\uE77B", shortcut: "Ctrl+3" },
        { key: "settings", title: qsTr("Settings"), glyph: "\uE713", shortcut: "Ctrl+," }
    ]

    function selectSection(index) {
        if (index >= 0 && index < shell.sections.length) {
            shell.currentSection = index
            if (index === 1)
                Qt.callLater(function() { searchPage.focusInput() })
        }
    }

    function openSearch() {
        shell.currentSection = 1
        Qt.callLater(function() { searchPage.focusInput() })
    }

    function openDiagnosticsPage() {
        settingsPage.openSection("diagnostics")
        shell.currentSection = 3
        shell.openDiagnostics()
    }

    function recomputeUnread() {
        var total = 0
        for (var i = 0; i < unreadInstantiator.count; ++i) {
            var obj = unreadInstantiator.objectAt(i) as UnreadEntry
            if (obj) {
                var value = Number(obj.unreadCount)
                if (value > 0)
                    total += value
            }
        }
        shell.computedUnread = total
    }

    component UnreadEntry: QtObject {
        required property var unreadCount
        onUnreadCountChanged: Qt.callLater(shell.recomputeUnread)
    }

    onWidthChanged: {
        if (shell.width > 0 && shell.width < 920 && shell.railExpanded)
            shell.railExpanded = false
    }

    // --- Real reactive unread counter over BeyGramApp.chatList --------------
    Instantiator {
        id: unreadInstantiator
        model: BeyGramApp.chatList
        delegate: UnreadEntry {}
        onObjectAdded: Qt.callLater(shell.recomputeUnread)
        onObjectRemoved: Qt.callLater(shell.recomputeUnread)
    }

    Connections {
        target: BeyGramApp.chatList
        ignoreUnknownSignals: true

        function onRowsInserted() { Qt.callLater(shell.recomputeUnread) }
        function onRowsRemoved() { Qt.callLater(shell.recomputeUnread) }
        function onModelReset() { Qt.callLater(shell.recomputeUnread) }
        function onLayoutChanged() { Qt.callLater(shell.recomputeUnread) }
        function onDataChanged() { Qt.callLater(shell.recomputeUnread) }
    }

    Component.onCompleted: Qt.callLater(shell.recomputeUnread)

    // --- Global desktop shortcuts -------------------------------------------
    Shortcut { sequence: "Ctrl+K"; onActivated: shell.openSearch() }
    Shortcut { sequence: "Ctrl+1"; onActivated: shell.selectSection(0) }
    Shortcut { sequence: "Ctrl+2"; onActivated: shell.selectSection(1) }
    Shortcut { sequence: "Ctrl+3"; onActivated: shell.selectSection(2) }
    Shortcut { sequence: "Ctrl+4"; onActivated: shell.selectSection(3) }
    Shortcut { sequence: "Ctrl+,"; onActivated: shell.selectSection(3) }
    Shortcut { sequence: "Ctrl+D"; onActivated: BeyGramAppearance.setThemeMode(BeyGramTheme.darkMode ? "Light" : "Dark") }

    // ----------------------------------------------------------------------
    // Slim adaptive left navigation rail
    // ----------------------------------------------------------------------
    Rectangle {
        id: rail

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: shell.railWidth
        color: BeyGramAppearance.designStyle === "Signature"
               ? BeyGramTheme.bg
               : BeyGramTheme.surface
        clip: true

        Behavior on width {
            NumberAnimation {
                duration: shell._animNormal
                easing.type: Easing.OutCubic
            }
        }

        // Hairline separating the rail from the section stack.
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: BeyGramTheme.line
            z: 2
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: shell.railExpanded ? Design.space3 : Design.space2
            anchors.rightMargin: shell.railExpanded ? Design.space3 : Design.space2
            anchors.topMargin: Design.space3
            anchors.bottomMargin: Design.space3
            spacing: Design.space2

            // Top: user avatar with online indicator -> opens Profile (section 2).
            AbstractButton {
                id: profileButton

                Layout.fillWidth: true
                Layout.preferredHeight: shell.railExpanded ? 54 : 50
                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Profile — %1").arg(shell.accountDisplayName)

                onClicked: shell.selectSection(2)

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }

                DsTooltip {
                    text: shell.accountDisplayName + " (" + shell.accountHandle + ")"
                    visible: !shell.railExpanded && profileButton.hovered
                }

                background: Rectangle {
                    radius: Design.rowRadius(BeyGramAppearance.designStyle)
                    color: shell.currentSection === 2
                           ? Qt.alpha(BeyGramTheme.accentSoft, 0.7)
                           : (profileButton.hovered
                              ? Qt.alpha(BeyGramTheme.surface2, 0.85)
                              : "transparent")
                    border.width: shell.railExpanded ? 1 : 0
                    border.color: shell.currentSection === 2
                                  ? Qt.alpha(BeyGramTheme.accent, 0.35)
                                  : Qt.alpha(BeyGramTheme.line, 0.85)
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: shell._animFast; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: Item {
                    implicitHeight: shell.railExpanded ? 54 : 50

                    DsAvatar {
                        id: railAvatar
                        size: shell.railExpanded ? 36 : 40
                        initials: shell.accountInitials
                        online: shell.accountOnline
                        anchors.verticalCenter: parent.verticalCenter
                        x: shell.railExpanded
                           ? Design.space2
                           : Math.round((parent.width - width) / 2)
                    }

                    ColumnLayout {
                        visible: shell.railExpanded
                        anchors.verticalCenter: parent.verticalCenter
                        x: railAvatar.x + railAvatar.width + Design.space2 + 2
                        width: Math.max(0, parent.width - x - Design.space2)
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: shell.accountDisplayName
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: shell.accountHandle
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: Design.rowRadius(BeyGramAppearance.designStyle) + 2
                    color: "transparent"
                    border.width: 2
                    border.color: BeyGramTheme.accent
                    visible: profileButton.visualFocus
                    antialiasing: true
                    z: 3
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: Design.space1
                Layout.rightMargin: Design.space1
                color: Qt.alpha(BeyGramTheme.line, 0.7)
            }

            // Everyday primary destinations (Chats, Search, Profile, Settings)
            Repeater {
                model: shell.sections

                delegate: Item {
                    id: navItem
                    required property var modelData
                    required property int index

                    readonly property bool selected: shell.currentSection === navItem.index
                    readonly property int badgeCount: navItem.modelData.key === "chats"
                                                      ? shell.unreadTotal : 0

                    Layout.fillWidth: true
                    Layout.preferredHeight: shell.railExpanded ? 42 : 54

                    // Expanded: labelled desktop NavButton
                    NavButton {
                        anchors.fill: parent
                        visible: shell.railExpanded
                        text: navItem.modelData.title
                        icon: navItem.modelData.glyph
                        selected: navItem.selected
                        hint: navItem.modelData.shortcut
                        badge: navItem.badgeCount
                        onClicked: shell.selectSection(navItem.index)
                    }

                    // Collapsed: Android AdaptiveNavigationRail-inspired pill + micro label
                    AbstractButton {
                        id: railBtn
                        anchors.fill: parent
                        visible: !shell.railExpanded
                        padding: 0
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus

                        Accessible.role: Accessible.Button
                        Accessible.name: navItem.modelData.title
                        Accessible.checked: navItem.selected

                        onClicked: shell.selectSection(navItem.index)

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        DsTooltip {
                            text: navItem.modelData.title + " (" + navItem.modelData.shortcut + ")"
                            visible: railBtn.hovered
                        }

                        background: Item {}

                        contentItem: ColumnLayout {
                            spacing: 3

                            Item {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 32

                                Rectangle {
                                    anchors.fill: parent
                                    radius: BeyGramAppearance.designStyle === "Material2"
                                            ? Design.radiusSm : Design.radiusPill
                                    color: navItem.selected
                                           ? BeyGramTheme.accentSoft
                                           : (railBtn.hovered
                                              ? Qt.alpha(BeyGramTheme.surface2, 0.9)
                                              : "transparent")
                                    border.width: (navItem.selected && BeyGramTheme.modeName === "Amoled")
                                                  || railBtn.visualFocus ? 1 : 0
                                    border.color: railBtn.visualFocus
                                                  ? BeyGramTheme.accent
                                                  : Qt.alpha(BeyGramTheme.accent, 0.35)
                                    antialiasing: true

                                    Behavior on color {
                                        ColorAnimation { duration: shell._animFast; easing.type: Easing.OutCubic }
                                    }
                                }

                                DsIcon {
                                    anchors.centerIn: parent
                                    name: navItem.modelData.glyph
                                    size: Design.iconSize
                                    color: navItem.selected
                                           ? BeyGramTheme.accentInk
                                           : (railBtn.hovered ? BeyGramTheme.text : BeyGramTheme.muted)
                                    filled: navItem.selected
                                }

                                // Real unread badge pill on Chats
                                Rectangle {
                                    visible: navItem.badgeCount > 0
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.topMargin: -3
                                    anchors.rightMargin: -4
                                    width: Math.max(18, railBadgeText.implicitWidth + 8)
                                    height: 16
                                    radius: 8
                                    color: BeyGramTheme.accent
                                    border.width: 1.5
                                    border.color: rail.color
                                    antialiasing: true

                                    Text {
                                        id: railBadgeText
                                        anchors.centerIn: parent
                                        text: navItem.badgeCount > 99 ? "99+" : String(navItem.badgeCount)
                                        color: BeyGramTheme.onAccent
                                        font.pixelSize: 9
                                        font.weight: Font.Bold
                                        Accessible.ignored: true
                                    }
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.maximumWidth: 64
                                text: navItem.modelData.title
                                color: navItem.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
                                font.pixelSize: 10
                                font.weight: navItem.selected ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                Accessible.ignored: true
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true; Layout.minimumHeight: Design.space2 }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: Design.space1
                Layout.rightMargin: Design.space1
                color: Qt.alpha(BeyGramTheme.line, 0.7)
            }

            // Footer: subtle connection status indicator (opens Settings on click)
            AbstractButton {
                id: statusButton
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Connection status: %1").arg(BeyGramApp.connectionTitle)

                onClicked: {
                    settingsPage.openSection("connection")
                    shell.selectSection(3)
                }

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                DsTooltip {
                    text: qsTr("Connection: %1").arg(BeyGramApp.connectionTitle)
                    visible: !shell.railExpanded && statusButton.hovered
                }

                background: Rectangle {
                    radius: Design.radiusSm
                    color: statusButton.hovered
                           ? Qt.alpha(BeyGramTheme.surface2, 0.85)
                           : "transparent"
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: shell.railExpanded ? Design.space2 : 0
                    anchors.rightMargin: shell.railExpanded ? Design.space2 : 0
                    spacing: Design.space2

                    Item {
                        Layout.fillWidth: !shell.railExpanded
                        Layout.preferredWidth: shell.railExpanded ? 20 : 20
                        Layout.preferredHeight: 20
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            anchors.centerIn: parent
                            width: 9
                            height: 9
                            radius: width / 2
                            color: shell.isConnected ? BeyGramTheme.okInk
                                 : shell.isConnecting ? BeyGramTheme.warnInk
                                 : BeyGramTheme.muted
                            antialiasing: true
                        }
                    }

                    Text {
                        visible: shell.railExpanded
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: BeyGramApp.connectionTitle
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        elide: Text.ElideRight
                    }
                }
            }

            // Footer: quick Light/Dark toggle & rail expand/collapse toggle
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: shell.railExpanded ? Qt.AlignLeft : Qt.AlignHCenter
                spacing: Design.space1

                DsIconButton {
                    Layout.alignment: Qt.AlignVCenter
                    icon: BeyGramTheme.darkMode ? "\uE706" : "\uE708"
                    size: "md"
                    tooltip: BeyGramTheme.darkMode
                             ? qsTr("Switch to light theme (Ctrl+D)")
                             : qsTr("Switch to dark theme (Ctrl+D)")
                    onClicked: BeyGramAppearance.setThemeMode(
                                   BeyGramTheme.darkMode ? "Light" : "Dark")
                }

                Item {
                    Layout.fillWidth: true
                    visible: shell.railExpanded
                }

                DsIconButton {
                    visible: shell.railExpanded
                    Layout.alignment: Qt.AlignVCenter
                    icon: "\uE76B" // ChevronLeft
                    size: "md"
                    tooltip: qsTr("Collapse sidebar")
                    onClicked: shell.railExpanded = false
                }
            }

            // Expand button centered at very bottom when rail is collapsed
            DsIconButton {
                visible: !shell.railExpanded
                Layout.alignment: Qt.AlignHCenter
                icon: "\uE76C" // ChevronRight
                size: "md"
                tooltip: qsTr("Expand sidebar")
                onClicked: shell.railExpanded = true
            }
        }
    }

    // ----------------------------------------------------------------------
    // Main content: section stack (each page owns its own header)
    // ----------------------------------------------------------------------
    Item {
        id: content

        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        StackLayout {
            anchors.fill: parent
            currentIndex: shell.currentSection

            ChatsPage {
                onGlobalSearchRequested: shell.openSearch()
            }

            GlobalSearchPage {
                id: searchPage
                model: BeyGramApp.globalSearchResults
                loading: BeyGramApp.globalSearchLoading
                hasMore: BeyGramApp.globalSearchResults
                         ? Boolean(BeyGramApp.globalSearchResults.hasMore) : false
                onSearchRequested: (query) => {
                    if (query.length > 0)
                        BeyGramApp.searchGlobal(query)
                    else
                        BeyGramApp.clearGlobalSearch()
                }
                onLoadMoreRequested: BeyGramApp.loadMoreGlobalSearch()
                onResultActivated: (chatId, messageId) => {
                    BeyGramApp.openChat(chatId)
                    shell.currentSection = 0
                }
            }

            OverviewPage {
                onOpenSavedMessagesRequested: {
                    shell.openSearch()
                    BeyGramApp.searchGlobal(qsTr("Saved Messages"))
                }
                onOpenAppearanceRequested: {
                    settingsPage.openSection("appearance")
                    shell.selectSection(3)
                }
                onOpenSettingsRequested: {
                    settingsPage.openSection("appearance")
                    shell.selectSection(3)
                }
                onOpenDiagnosticsRequested: shell.openDiagnosticsPage()
            }

            SettingsPage {
                id: settingsPage
            }
        }
    }

    // Global overlays
    CallOverlay {
        anchors.fill: parent
        facade: BeyGramApp.calls
    }
    MediaGallery {
        anchors.fill: parent
        facade: BeyGramApp.gallery
    }
    StoryViewer {
        anchors.fill: parent
        facade: BeyGramApp.stories
    }
    PlaybackBar {
        anchors.left: content.left
        anchors.right: content.right
        anchors.bottom: content.bottom
        visible: (BeyGramApp.playbackBar ? BeyGramApp.playbackBar.active : false)
                 && shell.currentSection !== 0
        facade: BeyGramApp.playbackBar
    }
    DeepLinkPrompt {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 24
        facade: BeyGramApp.deepLinks
    }
}
