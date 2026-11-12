pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "ds/design.js" as Design

// Shell — the signed-in window.
//
// A collapsible navigation rail sits left of the page stack: 76 px of glyph
// buttons with tooltips, or 268 px with labels when railExpanded. A 1 px line
// separates the rail from the content, and a slim DsHeader above the stack
// names the current section. The brand mark doubles as the rail toggle so the
// header needs no extra chrome.
Item {
    id: shell

    property int currentSection: 0
    property bool railExpanded: true

    // Section order is also the Alt+1..8 order and the StackLayout order.
    readonly property var sections: [
        { key: "chats", title: "Chats", glyph: "\uE8BD" },
        { key: "profile", title: "Profile", glyph: "\uE77B" },
        { key: "settings", title: "Settings", glyph: "\uE713" },
        { key: "network", title: "Network", glyph: "\uE774" },
        { key: "logs", title: "Logs", glyph: "\uE8A5" },
        { key: "search", title: "Search", glyph: "\uE721" },
        { key: "tools", title: "Tools", glyph: "\uE90F" },
        { key: "design", title: "Design", glyph: "\uE790" }
    ]

    readonly property int railWidth: shell.railExpanded ? 268 : 76
    readonly property string sectionTitle: shell.currentSection >= 0
                                          && shell.currentSection < shell.sections.length
                                          ? shell.sections[shell.currentSection].title : ""

    readonly property int unreadTotal: BeyGramApp.chatListControls.totalUnread

    function selectSection(index) {
        if (index >= 0 && index < shell.sections.length)
            shell.currentSection = index
    }

    // Used by the top-bar icon: show the search page and put the caret in its
    // field. The page is always instantiated, so callLater waits for the
    // StackLayout to make it visible before focus is requested.
    function openSearch() {
        shell.currentSection = 5
        Qt.callLater(function() { searchPage.focusInput() })
    }

    Shortcut { sequence: "Alt+1"; onActivated: shell.selectSection(0) }
    Shortcut { sequence: "Alt+2"; onActivated: shell.selectSection(1) }
    Shortcut { sequence: "Alt+3"; onActivated: shell.selectSection(2) }
    Shortcut { sequence: "Alt+4"; onActivated: shell.selectSection(3) }
    Shortcut { sequence: "Alt+5"; onActivated: shell.selectSection(4) }
    Shortcut { sequence: "Alt+6"; onActivated: shell.selectSection(5) }
    Shortcut { sequence: "Alt+7"; onActivated: shell.selectSection(6) }
    Shortcut { sequence: "Alt+8"; onActivated: shell.selectSection(7) }

    // ----------------------------------------------------------------------
    // Navigation rail
    // ----------------------------------------------------------------------
    Rectangle {
        id: rail

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: shell.railWidth
        color: BeyGramTheme.surface
        clip: true

        Behavior on width {
            NumberAnimation { duration: Design.motionNormal; easing.type: Easing.OutCubic }
        }

        // The hairline that separates the rail from the page stack.
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
            anchors.leftMargin: shell.railExpanded ? Design.space4 : Design.space3
            anchors.rightMargin: shell.railExpanded ? Design.space4 : Design.space3
            anchors.topMargin: Design.space3
            anchors.bottomMargin: Design.space3
            spacing: Design.space1

            // Brand mark and wordmark. Clicking anywhere on the header folds
            // the rail; the chevron only hints at it while labels are visible.
            AbstractButton {
                id: brandButton

                Layout.fillWidth: true
                Layout.preferredHeight: 48
                padding: 0
                hoverEnabled: true

                readonly property string actionText: shell.railExpanded
                                                     ? "Collapse navigation"
                                                     : "Expand navigation"

                Accessible.role: Accessible.Button
                Accessible.name: brandButton.actionText

                onClicked: shell.railExpanded = !shell.railExpanded

                DsTooltip {
                    text: brandButton.actionText
                    visible: brandButton.hovered
                }

                background: Rectangle {
                    radius: Design.radiusMd
                    color: brandButton.hovered
                           ? Qt.alpha(BeyGramTheme.surface2, 0.85)
                           : "transparent"
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: Item {
                    implicitHeight: 48

                    Rectangle {
                        id: brandMark
                        width: 40
                        height: 40
                        radius: Design.radiusMd
                        color: BeyGramTheme.accent
                        anchors.verticalCenter: parent.verticalCenter
                        x: shell.railExpanded ? 0 : Math.round((parent.width - width) / 2)
                        antialiasing: true

                        Text {
                            anchors.centerIn: parent
                            text: "B"
                            color: BeyGramTheme.onAccent
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                        }
                    }

                    Column {
                        visible: shell.railExpanded
                        anchors.verticalCenter: parent.verticalCenter
                        x: brandMark.x + brandMark.width + Design.space3
                        spacing: 1

                        Text {
                            text: "BeyGram"
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontTitle
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: "Windows"
                            color: BeyGramTheme.muted
                            font.pixelSize: 10
                            font.letterSpacing: 0.6
                        }
                    }

                    DsIcon {
                        visible: shell.railExpanded
                        name: "\uE76B"
                        size: 12
                        color: BeyGramTheme.muted
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                // Same 2 px halo as DsIconButton, so keyboard focus is visible.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: Design.radiusMd + 2
                    color: "transparent"
                    border.width: 2
                    border.color: BeyGramTheme.accent
                    visible: brandButton.visualFocus
                    antialiasing: true
                    z: 3
                }
            }

            Item { Layout.preferredHeight: Design.space2 }

            Repeater {
                model: shell.sections

                delegate: Item {
                    id: navItem
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    // Expanded: the shared NavButton keeps the labelled API
                    // (text / selected / clicked) while that component is
                    // migrated to the design system.
                    NavButton {
                        anchors.fill: parent
                        visible: shell.railExpanded
                        text: navItem.modelData.title
                        selected: shell.currentSection === navItem.index
                        onClicked: shell.selectSection(navItem.index)
                    }

                    // Collapsed: the same destination as a glyph with a
                    // tooltip, so the rail stays 76 px wide.
                    DsIconButton {
                        anchors.centerIn: parent
                        visible: !shell.railExpanded
                        icon: navItem.modelData.glyph
                        tooltip: navItem.modelData.title + " (Alt+" + (navItem.index + 1) + ")"
                        selected: shell.currentSection === navItem.index
                        badge: navItem.modelData.key === "chats" ? shell.unreadTotal : 0
                        onClicked: shell.selectSection(navItem.index)
                    }

                    DsBadge {
                        visible: shell.railExpanded
                                 && navItem.modelData.key === "chats"
                                 && shell.unreadTotal > 0
                        anchors.right: parent.right
                        anchors.rightMargin: Design.space3
                        anchors.verticalCenter: parent.verticalCenter
                        text: BeyGramApp.chatListControls.badgeText
                        tone: "accent"
                        z: 1
                    }
                }
            }

            Item { Layout.fillHeight: true; Layout.minimumHeight: Design.space2 }

            // Theme toggle kept from the old sidebar, now a glyph action.
            RowLayout {
                Layout.fillWidth: shell.railExpanded
                Layout.alignment: shell.railExpanded ? Qt.AlignLeft : Qt.AlignHCenter
                spacing: Design.space2

                DsIconButton {
                    icon: BeyGramTheme.darkMode ? "\uE706" : "\uE708"
                    tooltip: BeyGramTheme.darkMode
                             ? "Use light theme (Ctrl+D)"
                             : "Use dark theme (Ctrl+D)"
                    onClicked: BeyGramTheme.toggleDarkMode()
                }

                Text {
                    visible: shell.railExpanded
                    text: BeyGramTheme.darkMode ? "Dark theme" : "Light theme"
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: Design.space1
                Layout.bottomMargin: Design.space1
                color: BeyGramTheme.line
            }

            // Account block. The avatar's presence dot is the account status:
            // it follows the TDLib connection, not a local guess.
            AbstractButton {
                id: accountBlock

                Layout.fillWidth: true
                Layout.preferredHeight: shell.railExpanded ? 58 : 48
                padding: 0
                hoverEnabled: true

                readonly property string displayName: BeyGramApp.accountDisplayName.length > 0
                                                      ? BeyGramApp.accountDisplayName
                                                      : "Telegram account"
                readonly property string secondary: BeyGramApp.accountUsername.length > 0
                                                    ? "@" + BeyGramApp.accountUsername
                                                    : BeyGramApp.connectionTitle

                Accessible.role: Accessible.Button
                Accessible.name: "Profile — " + accountBlock.displayName

                onClicked: shell.selectSection(1)

                DsTooltip {
                    text: accountBlock.displayName
                    visible: !shell.railExpanded && accountBlock.hovered
                }

                background: Rectangle {
                    radius: Design.radiusMd
                    color: shell.railExpanded
                           ? Qt.alpha(BeyGramTheme.surface2, BeyGramTheme.darkMode ? 0.55 : 0.7)
                           : (accountBlock.hovered
                              ? Qt.alpha(BeyGramTheme.surface2, 0.85)
                              : "transparent")
                    border.width: shell.railExpanded ? 1 : 0
                    border.color: Qt.alpha(BeyGramTheme.line, 0.9)
                    antialiasing: true

                    Behavior on color {
                        ColorAnimation { duration: Design.motionFast; easing.type: Easing.OutCubic }
                    }
                }

                contentItem: Item {
                    implicitHeight: 58

                    DsAvatar {
                        id: accountAvatar
                        size: 34
                        initials: BeyGramApp.accountDisplayName.length > 0
                                  ? BeyGramApp.accountDisplayName.charAt(0).toUpperCase()
                                  : "T"
                        online: BeyGramApp.network.online
                        anchors.verticalCenter: parent.verticalCenter
                        x: shell.railExpanded
                           ? Design.space3
                           : Math.round((parent.width - width) / 2)
                    }

                    ColumnLayout {
                        visible: shell.railExpanded
                        anchors.verticalCenter: parent.verticalCenter
                        x: accountAvatar.x + accountAvatar.width + Design.space2 + 2
                        width: parent.width - x - Design.space3
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: accountBlock.displayName
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: accountBlock.secondary
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            elide: Text.ElideRight
                        }
                    }
                }

                // Same 2 px halo as DsIconButton, so keyboard focus is visible.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: Design.radiusMd + 2
                    color: "transparent"
                    border.width: 2
                    border.color: BeyGramTheme.accent
                    visible: accountBlock.visualFocus
                    antialiasing: true
                    z: 3
                }
            }
        }
    }

    // ----------------------------------------------------------------------
    // Content: slim section bar over the page stack
    // ----------------------------------------------------------------------
    Item {
        id: content

        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        DsHeader {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            title: shell.sectionTitle

            trailing: DsIconButton {
                icon: "\uE721"
                tooltip: "Search messages (Alt+6)"
                selected: shell.currentSection === 5
                onClicked: shell.openSearch()
            }
        }

        StackLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.bottom: parent.bottom
            currentIndex: shell.currentSection

            ChatsPage {}
            OverviewPage {}
            SettingsPage {}
            EnvironmentPage {}
            LogsPage {}
            GlobalSearchPage {
                id: searchPage
                model: BeyGramApp.globalSearchResults
                loading: BeyGramApp.globalSearchLoading
                hasMore: BeyGramApp.globalSearchResults
                         ? BeyGramApp.globalSearchResults.hasMore : false
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
            ToolsPage {}
            DesignGalleryPage {}
        }
    }

    // Global overlays: each one is invisible until its facade says it is
    // active, so they can live at the shell root and cover every page.
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        facade: BeyGramApp.playbackBar
    }
    DeepLinkPrompt {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 24
        facade: BeyGramApp.deepLinks
    }
}
