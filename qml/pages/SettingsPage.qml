import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// SettingsPage — 2-pane desktop settings matching Android BeyGram's SettingsScreen
// categories, with honest backend-wiring transparency and an integrated Diagnostics hub.
Item {
    id: page

    property string activeCategory: "appearance"
    property int diagnosticsTab: 0

    readonly property bool wideLayout: page.width >= 820

    readonly property var categories: [
        { id: "appearance", title: qsTr("Appearance & Style"), glyph: "\uE790" },
        { id: "language", title: qsTr("Language"), glyph: "\uE775" },
        { id: "notifications", title: qsTr("Notifications"), glyph: "\uEA8F" },
        { id: "connection", title: qsTr("Connection & Network"), glyph: "\uE774" },
        { id: "folders", title: qsTr("Folders"), glyph: "\uE8B7" },
        { id: "privacy", title: qsTr("Privacy & Storage"), glyph: "\uE72E" },
        { id: "shortcuts", title: qsTr("Shortcuts"), glyph: "\uE765" },
        { id: "about", title: qsTr("About"), glyph: "\uE946" },
        { id: "diagnostics", title: qsTr("Diagnostics"), glyph: "\uE90F" }
    ]

    readonly property var languageChoices: [
        { code: "auto", title: qsTr("Follow system"), subtitle: qsTr("Matches your Windows regional settings") },
        { code: "ru", title: qsTr("Russian (Русский)"), subtitle: qsTr("Russian interface and date formatting") },
        { code: "en", title: qsTr("English"), subtitle: qsTr("English interface and date formatting") }
    ]

    function openSection(sectionId) {
        const key = String(sectionId || "").trim().toLowerCase()
        if (key === "storage") {
            page.activeCategory = "privacy"
            return
        }
        if (key === "network" || key === "proxy") {
            page.activeCategory = "connection"
            return
        }
        if (key === "environment") {
            page.activeCategory = "diagnostics"
            page.diagnosticsTab = 0
            return
        }
        if (key === "logs") {
            page.activeCategory = "diagnostics"
            page.diagnosticsTab = 1
            return
        }
        if (key === "design") {
            page.activeCategory = "diagnostics"
            page.diagnosticsTab = 2
            return
        }
        if (key === "tools") {
            page.activeCategory = "diagnostics"
            page.diagnosticsTab = 3
            return
        }
        for (let i = 0; i < page.categories.length; ++i) {
            if (page.categories[i].id === key) {
                page.activeCategory = key
                return
            }
        }
        page.activeCategory = "appearance"
    }

    function categoryIndex() {
        for (let i = 0; i < page.categories.length; ++i) {
            if (page.categories[i].id === page.activeCategory)
                return i
        }
        return 0
    }

    readonly property string currentCategoryTitle: {
        const idx = page.categoryIndex()
        return idx >= 0 && idx < page.categories.length ? page.categories[idx].title : qsTr("Settings")
    }

    readonly property string liveDatePreview: {
        const _rev = BeyGramLang.revision
        const nowSec = Math.floor(Date.now() / 1000)
        return BeyGramLang.formatDayDivider(nowSec) + " · " + BeyGramLang.formatChatTimestamp(nowSec)
    }

    component ShortcutRow: DsListRow {
        id: shortcutRow
        property string keys: ""
        hoverEnabled: false

        trailing: Rectangle {
            implicitWidth: keyLabel.implicitWidth + Design.space3
            implicitHeight: 24
            radius: Design.radiusXs
            color: BeyGramTheme.surface2
            border.width: 1
            border.color: BeyGramTheme.line

            Text {
                id: keyLabel
                anchors.centerIn: parent
                text: shortcutRow.keys
                color: BeyGramTheme.text
                font.pixelSize: Design.fontCaption
                font.weight: Font.DemiBold
            }
        }
    }

    component PreviewStatusBanner: Rectangle {
        id: banner
        property string text: qsTr("Local preview · Backend sync in progress")

        Layout.fillWidth: true
        implicitHeight: bannerRow.implicitHeight + Design.space3 * 2
        radius: Design.radiusMd
        color: Qt.alpha(BeyGramTheme.warnInk, 0.10)
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.warnInk, 0.28)

        RowLayout {
            id: bannerRow
            anchors.fill: parent
            anchors.margins: Design.space3
            spacing: Design.space2

            DsBadge {
                text: qsTr("Preview")
                tone: "muted"
                dot: true
            }

            Text {
                Layout.fillWidth: true
                text: banner.text
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                wrapMode: Text.WordWrap
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: qsTr("Settings")
            subtitle: page.currentCategoryTitle
        }

        // Narrow top category tabs (< 820 px)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: narrowTabs.implicitHeight + Design.space2 * 2
            visible: !page.wideLayout
            color: BeyGramTheme.surface

            DsTabs {
                id: narrowTabs
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Design.space4
                anchors.rightMargin: Design.space4
                model: page.categories.map(function (c) { return c.title })
                currentIndex: page.categoryIndex()
                onActivated: function (index) {
                    if (index >= 0 && index < page.categories.length)
                        page.activeCategory = page.categories[index].id
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: BeyGramTheme.line
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Category Rail (236 px on wide screens)
            Rectangle {
                visible: page.wideLayout
                Layout.preferredWidth: 236
                Layout.fillHeight: true
                color: BeyGramTheme.surface

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: BeyGramTheme.line
                }

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Design.space3
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: parent.width
                        spacing: Design.space1

                        Repeater {
                            model: page.categories

                            delegate: Rectangle {
                                id: catItem
                                required property var modelData
                                required property int index

                                readonly property bool current: page.activeCategory === modelData.id

                                Layout.fillWidth: true
                                Layout.preferredHeight: 40
                                radius: Design.rowRadius(BeyGramAppearance.designStyle)
                                color: catItem.current
                                       ? BeyGramTheme.accentSoft
                                       : (catMouse.containsMouse ? BeyGramTheme.surface2 : "transparent")

                                Accessible.role: Accessible.PageTab
                                Accessible.name: modelData.title
                                Accessible.checked: catItem.current

                                Behavior on color {
                                    ColorAnimation {
                                        duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Design.space3
                                    anchors.rightMargin: Design.space3
                                    spacing: Design.space3

                                    DsIcon {
                                        name: catItem.modelData.glyph
                                        size: 16
                                        color: catItem.current ? BeyGramTheme.accentInk : BeyGramTheme.muted
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: catItem.modelData.title
                                        color: catItem.current ? BeyGramTheme.accentInk : BeyGramTheme.text
                                        font.pixelSize: Design.fontSmall
                                        font.weight: catItem.current ? Font.DemiBold : Font.Medium
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: catMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: page.activeCategory = catItem.modelData.id
                                }
                            }
                        }
                    }
                }
            }

            // Right Content Pane
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Regular scrollable categories (readable max measure ~720 px)
                ScrollView {
                    id: settingsScroll
                    anchors.fill: parent
                    visible: page.activeCategory !== "diagnostics"
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: settingsContent.y + settingsContent.implicitHeight + Design.space8
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        id: settingsContent
                        width: Math.min(720, Math.max(300, settingsScroll.availableWidth - Design.space6 * 2))
                        x: Math.round((settingsScroll.availableWidth - width) / 2)
                        y: Design.space6
                        spacing: Design.space5

                        // 1. APPEARANCE & STYLE
                        AppearancePanel {
                            Layout.fillWidth: true
                            visible: page.activeCategory === "appearance"
                            facade: BeyGramApp.appearance
                        }

                        // 2. LANGUAGE
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space4
                            visible: page.activeCategory === "language"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Interface language")
                                description: qsTr("Switches all labels, menus and date formatting immediately.")

                                trailing: DsBadge {
                                    text: BeyGramLang.effectiveLanguage === "ru"
                                          ? qsTr("Active: Russian")
                                          : qsTr("Active: English")
                                    tone: "accent"
                                    dot: true
                                }

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Repeater {
                                            model: page.languageChoices

                                            delegate: ColumnLayout {
                                                id: langRowWrap
                                                required property var modelData
                                                required property int index

                                                readonly property bool selected: BeyGramLang.language === modelData.code

                                                Layout.fillWidth: true
                                                spacing: 0

                                                DsListRow {
                                                    Layout.fillWidth: true
                                                    title: langRowWrap.modelData.title
                                                    subtitle: langRowWrap.modelData.subtitle
                                                    selected: langRowWrap.selected
                                                    onClicked: BeyGramLang.setLanguage(langRowWrap.modelData.code)

                                                    trailing: DsBadge {
                                                        visible: langRowWrap.selected
                                                        text: qsTr("Selected")
                                                        tone: "accent"
                                                    }
                                                }

                                                DsDivider {
                                                    Layout.fillWidth: true
                                                    inset: Design.space3
                                                    visible: langRowWrap.index < page.languageChoices.length - 1
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Live date & time preview")

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space4

                                    content: RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Design.space3

                                        DsIcon {
                                            name: "\uE787"
                                            color: BeyGramTheme.accentInk
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: page.liveDatePreview
                                            color: BeyGramTheme.text
                                            font.pixelSize: Design.fontBody
                                            font.weight: Font.Medium
                                        }
                                    }
                                }
                            }
                        }

                        // 3. NOTIFICATIONS
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space5
                            visible: page.activeCategory === "notifications"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Desktop notifications")
                                description: qsTr("Alerts are delivered via the Windows system tray and suppressed for the open chat, muted chats and your own messages.")

                                trailing: DsBadge {
                                    text: qsTr("Saved locally")
                                    tone: "ok"
                                    dot: true
                                }

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Desktop notifications")
                                            subtitle: qsTr("Show new messages in the Windows system tray.")
                                            onClicked: notifySwitch.toggle()

                                            trailing: DsSwitch {
                                                id: notifySwitch
                                                checked: BeyGramApp.notificationsEnabled
                                                onToggled: BeyGramApp.notificationsEnabled = checked
                                                Accessible.name: qsTr("Desktop notifications")
                                            }
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Show message preview")
                                            subtitle: BeyGramApp.notificationsEnabled
                                                      ? qsTr("Include sender and message text in tray alerts.")
                                                      : qsTr("Enable desktop notifications first.")
                                            onClicked: {
                                                if (previewSwitch.enabled)
                                                    previewSwitch.toggle()
                                            }

                                            trailing: DsSwitch {
                                                id: previewSwitch
                                                checked: BeyGramApp.notificationPreview
                                                enabled: BeyGramApp.notificationsEnabled
                                                onToggled: BeyGramApp.notificationPreview = checked
                                                Accessible.name: qsTr("Show message preview")
                                            }
                                        }
                                    }
                                }
                            }

                            PreviewStatusBanner {
                                text: qsTr("Per-chat mute can be changed directly in each conversation. Global sound, Do Not Disturb and quiet hours below are local preview controls pending backend sync.")
                            }

                            NotificationSettings {
                                Layout.fillWidth: true
                                facade: BeyGramApp.notificationSettings
                            }
                        }

                        // 4. CONNECTION & NETWORK
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space5
                            visible: page.activeCategory === "connection"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Live connection")
                                description: qsTr("Current TDLib transport and startup proxy configuration.")

                                trailing: DsBadge {
                                    text: BeyGramApp.connectionTitle
                                    tone: BeyGramApp.ready && BeyGramApp.running ? "ok" : "muted"
                                    dot: true
                                }

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("Connection status")
                                            subtitle: BeyGramApp.stageTitle
                                            trailing: Text {
                                                text: BeyGramApp.connectionTitle
                                                color: BeyGramApp.ready ? BeyGramTheme.okInk : BeyGramTheme.muted
                                                font.pixelSize: Design.fontSmall
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("Active proxy")
                                            subtitle: qsTr("Configured at startup via BEYGRAM_PROXY environment variable.")
                                            trailing: Text {
                                                text: BeyGramApp.proxySummary
                                                color: BeyGramTheme.text
                                                font.pixelSize: Design.fontSmall
                                                font.weight: Font.Medium
                                            }
                                        }
                                    }
                                }
                            }

                            PreviewStatusBanner {
                                text: qsTr("Interactive proxy form below is a local preview — changes are not yet applied to live TDLib transport.")
                            }

                            NetworkPanel {
                                Layout.fillWidth: true
                                facade: BeyGramApp.network
                            }
                        }

                        // 5. FOLDERS
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space4
                            visible: page.activeCategory === "folders"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Chat folders")
                                description: qsTr("Select a folder to filter your chat list.")

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        DsListRow {
                                            Layout.fillWidth: true
                                            visible: folderListView.count === 0
                                            hoverEnabled: false
                                            title: qsTr("No folders loaded yet")
                                            subtitle: qsTr("Folders sync automatically once connected to Telegram.")
                                            muted: true
                                        }

                                        ListView {
                                            id: folderListView
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: contentHeight
                                            visible: count > 0
                                            interactive: false
                                            model: BeyGramApp.chatFolders

                                            delegate: DsListRow {
                                                id: folderRow
                                                required property var folderId
                                                required property string title
                                                required property int unreadCount
                                                required property bool isMain

                                                width: folderListView.width
                                                title: folderRow.title
                                                subtitle: folderRow.isMain
                                                          ? qsTr("Default chat list")
                                                          : qsTr("Custom folder")
                                                selected: Number(BeyGramApp.currentFolderId) === Number(folderRow.folderId)
                                                onClicked: BeyGramApp.selectFolder(folderRow.folderId)

                                                trailing: RowLayout {
                                                    spacing: Design.space2

                                                    DsBadge {
                                                        visible: folderRow.unreadCount > 0
                                                        text: String(folderRow.unreadCount)
                                                        tone: "accent"
                                                    }

                                                    DsBadge {
                                                        visible: folderRow.selected
                                                        text: qsTr("Active")
                                                        tone: "ok"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            PreviewStatusBanner {
                                text: qsTr("Folder creation, renaming and deletion sync is pending backend wiring.")
                            }
                        }

                        // 6. PRIVACY & STORAGE
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space5
                            visible: page.activeCategory === "privacy"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Local security & storage")
                                description: qsTr("Verified local data protection on this Windows account.")

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space4

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Design.space3

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("Database encryption")
                                            subtitle: qsTr("Session keys and API credentials are protected with Windows DPAPI.")
                                            trailing: DsBadge {
                                                text: qsTr("DPAPI Active")
                                                tone: "ok"
                                                dot: true
                                            }
                                        }

                                        DsDivider { Layout.fillWidth: true }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Design.space1

                                            Text {
                                                text: qsTr("Data directory")
                                                color: BeyGramTheme.muted
                                                font.pixelSize: Design.fontCaption
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: BeyGramApp.dataDirectory
                                                color: BeyGramTheme.text
                                                font.pixelSize: Design.fontSmall
                                                elide: Text.ElideMiddle
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Design.space2

                                            DsButton {
                                                text: qsTr("Open data folder")
                                                tone: "secondary"
                                                size: "sm"
                                                onClicked: BeyGramApp.openDataFolder()
                                            }

                                            DsButton {
                                                text: qsTr("Open log folder")
                                                tone: "secondary"
                                                size: "sm"
                                                onClicked: BeyGramApp.openLogFolder()
                                            }

                                            Item { Layout.fillWidth: true }
                                        }
                                    }
                                }
                            }

                            PreviewStatusBanner {
                                text: qsTr("Local preview · Backend sync in progress — Telegram cloud privacy rules, active sessions and cache breakdown are not yet connected to TDLib.")
                            }

                            PrivacyPanel {
                                Layout.fillWidth: true
                                facade: BeyGramApp.privacy
                            }

                            SessionsPanel {
                                Layout.fillWidth: true
                                facade: BeyGramApp.sessions
                            }

                            StoragePanel {
                                Layout.fillWidth: true
                                facade: BeyGramApp.storage
                            }
                        }

                        // 7. SHORTCUTS
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space4
                            visible: page.activeCategory === "shortcuts"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("Keyboard shortcuts")
                                description: qsTr("Active desktop navigation and messaging shortcuts.")

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Global search")
                                            subtitle: qsTr("Focus search across chats and messages.")
                                            keys: "Ctrl + K"
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Switch sections")
                                            subtitle: qsTr("Jump between Chats, Profile and Settings.")
                                            keys: "Ctrl + 1 / 3 / ,"
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Toggle dark mode")
                                            subtitle: qsTr("Switch between Light and Dark themes.")
                                            keys: "Ctrl + D"
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Refresh")
                                            subtitle: qsTr("Reload current account state.")
                                            keys: "Ctrl + R"
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("Send message")
                                            subtitle: qsTr("Send the message in the composer.")
                                            keys: "Enter"
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        ShortcutRow {
                                            Layout.fillWidth: true
                                            title: qsTr("New line")
                                            subtitle: qsTr("Insert a line break without sending.")
                                            keys: "Shift + Enter"
                                        }
                                    }
                                }
                            }
                        }

                        // 8. ABOUT
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space4
                            visible: page.activeCategory === "about"

                            DsSection {
                                Layout.fillWidth: true
                                title: qsTr("About BeyGram Windows")
                                description: qsTr("Application version, framework and diagnostics.")

                                content: DsCard {
                                    Layout.fillWidth: true
                                    padding: Design.space2

                                    content: ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("BeyGram Windows")
                                            subtitle: qsTr("Native desktop client")
                                            trailing: Text {
                                                text: BeyGramApp.appVersion
                                                color: BeyGramTheme.accentInk
                                                font.pixelSize: Design.fontSmall
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("Qt framework")
                                            subtitle: qsTr("Desktop UI runtime")
                                            trailing: Text {
                                                text: BeyGramApp.qtVersion
                                                color: BeyGramTheme.muted
                                                font.pixelSize: Design.fontSmall
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        DsListRow {
                                            Layout.fillWidth: true
                                            hoverEnabled: false
                                            title: qsTr("Build")
                                            subtitle: BeyGramApp.buildInfo
                                        }

                                        DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.margins: Design.space3
                                            spacing: Design.space2

                                            DsButton {
                                                text: qsTr("Open Diagnostics")
                                                tone: "primary"
                                                size: "sm"
                                                onClicked: page.openSection("diagnostics")
                                            }

                                            DsButton {
                                                text: qsTr("Copy diagnostics")
                                                tone: "secondary"
                                                size: "sm"
                                                onClicked: BeyGramApp.copyDiagnostics()
                                            }

                                            Item { Layout.fillWidth: true }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 9. INTEGRATED DIAGNOSTICS HUB
                ColumnLayout {
                    anchors.fill: parent
                    visible: page.activeCategory === "diagnostics"
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: diagTabs.implicitHeight + Design.space3 * 2
                        color: BeyGramTheme.surface

                        DsTabs {
                            id: diagTabs
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Design.space4
                            anchors.rightMargin: Design.space4
                            model: [
                                qsTr("Environment"),
                                qsTr("Logs"),
                                qsTr("Design Gallery"),
                                qsTr("Utilities")
                            ]
                            currentIndex: page.diagnosticsTab
                            onActivated: function (index) { page.diagnosticsTab = index }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: BeyGramTheme.line
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: page.diagnosticsTab

                        Loader {
                            active: page.activeCategory === "diagnostics" && page.diagnosticsTab === 0
                            sourceComponent: EnvironmentPage { showHeader: false }
                        }

                        Loader {
                            active: page.activeCategory === "diagnostics" && page.diagnosticsTab === 1
                            sourceComponent: LogsPage { showHeader: false }
                        }

                        Loader {
                            active: page.activeCategory === "diagnostics" && page.diagnosticsTab === 2
                            sourceComponent: DesignGalleryPage {}
                        }

                        Loader {
                            active: page.activeCategory === "diagnostics" && page.diagnosticsTab === 3
                            sourceComponent: ToolsPage { showHeader: false }
                        }
                    }
                }
            }
        }
    }
}
