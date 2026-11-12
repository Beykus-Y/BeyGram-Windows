import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// SettingsPage — appearance, notifications, profile, desktop and about.
//
// Every value is owned by BeyGramApp or BeyGramTheme; the page only binds and
// writes them. Layout follows the design system: a page header, titled
// sections, and one card per group.
Item {
    id: page

    readonly property var presetNames: ["Warm", "Graphite"]
    readonly property var modeLabels: ["Follow system", "Light", "Dark", "AMOLED"]
    readonly property var modeNames: ["FollowSystem", "Light", "Dark", "Amoled"]

    function presetIndex() {
        const index = page.presetNames.indexOf(BeyGramTheme.presetName)
        return index < 0 ? 0 : index
    }

    function modeIndex() {
        const index = page.modeNames.indexOf(BeyGramTheme.modeName)
        return index < 0 ? 0 : index
    }

    // A read-only shortcut line: action on the left, key chord on the right.
    component ShortcutRow: DsListRow {
        id: shortcutRow

        property string keys: ""

        hoverEnabled: false

        trailing: Text {
            text: shortcutRow.keys
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            font.weight: Font.DemiBold
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Settings"
            subtitle: "Appearance and desktop behaviour are stored locally for this Windows account."
        }

        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: settingsContent.y + settingsContent.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: settingsContent
                width: Math.min(760, Math.max(320, settingsScroll.availableWidth - Design.space8 * 2))
                x: Math.max(Design.space8, (settingsScroll.availableWidth - width) / 2)
                y: Design.space6
                spacing: Design.space6

                // --- Appearance ---------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: "Appearance"
                    description: "Colour family and brightness for the whole client."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space5

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            Text {
                                Layout.fillWidth: true
                                text: "Colour family"
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.3
                            }

                            DsSegmented {
                                id: presetSegment

                                Layout.fillWidth: true
                                model: page.presetNames
                                currentIndex: page.presetIndex()
                                onActivated: function(index) {
                                    BeyGramTheme.presetName = page.presetNames[index]
                                }
                            }

                            DsDivider { Layout.fillWidth: true }

                            Text {
                                Layout.fillWidth: true
                                text: "Brightness"
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.3
                            }

                            DsSegmented {
                                id: modeSegment

                                Layout.fillWidth: true
                                model: page.modeLabels
                                currentIndex: page.modeIndex()
                                onActivated: function(index) {
                                    BeyGramTheme.modeName = page.modeNames[index]
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: BeyGramTheme.darkMode
                                      ? "Dark palette active."
                                      : "Light palette active."
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                            }
                        }
                    }
                }

                // --- Notifications ------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: "Notifications"
                    description: "Alerts delivered through the Windows system tray."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space2

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                title: "Desktop notifications"
                                subtitle: "Show new messages in the Windows system tray."
                                onClicked: notifySwitch.toggle()

                                trailing: DsSwitch {
                                    id: notifySwitch

                                    checked: BeyGramApp.notificationsEnabled
                                    onToggled: BeyGramApp.notificationsEnabled = checked
                                    Accessible.name: "Desktop notifications"
                                }
                            }

                            DsDivider { inset: Design.space3 }

                            DsListRow {
                                Layout.fillWidth: true
                                title: "Show message preview"
                                subtitle: BeyGramApp.notificationsEnabled
                                          ? "Message text appears in the alert."
                                          : "Enable desktop notifications first."
                                onClicked: {
                                    if (previewSwitch.enabled)
                                        previewSwitch.toggle()
                                }

                                trailing: DsSwitch {
                                    id: previewSwitch

                                    checked: BeyGramApp.notificationPreview
                                    enabled: BeyGramApp.notificationsEnabled
                                    onToggled: BeyGramApp.notificationPreview = checked
                                    Accessible.name: "Show message preview"
                                }
                            }
                        }
                    }
                }

                // --- Profile ------------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: "Profile"
                    description: "The name, username and bio other people see."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space5

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            Text {
                                Layout.fillWidth: true
                                text: "Change the name, username and bio other people see. "
                                      + "Leave a field empty to keep its current value."
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Design.space2

                                DsButton {
                                    text: "Edit profile"
                                    tone: "secondary"
                                    onClicked: profileDialog.open()
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }

                // --- Desktop ------------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: "Desktop"
                    description: "Keyboard shortcuts and how tray alerts behave."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space2

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            ShortcutRow {
                                Layout.fillWidth: true
                                title: "Switch section"
                                subtitle: "Jump between the eight main sections."
                                keys: "Alt + 1…8"
                            }

                            DsDivider { inset: Design.space3 }

                            ShortcutRow {
                                Layout.fillWidth: true
                                title: "Toggle light / dark"
                                subtitle: "Flip the brightness mode."
                                keys: "Ctrl + D"
                            }

                            DsDivider { inset: Design.space3 }

                            ShortcutRow {
                                Layout.fillWidth: true
                                title: "Refresh"
                                subtitle: "Reload the current view from the account."
                                keys: "Ctrl + R"
                            }

                            DsDivider { inset: Design.space3 }

                            ShortcutRow {
                                Layout.fillWidth: true
                                title: "Send message"
                                subtitle: "Send the composer contents."
                                keys: "Enter"
                            }

                            DsDivider { inset: Design.space3 }

                            ShortcutRow {
                                Layout.fillWidth: true
                                title: "New line"
                                subtitle: "Add a line break without sending."
                                keys: "Shift + Enter"
                            }

                            DsDivider { inset: Design.space3 }

                            Text {
                                Layout.fillWidth: true
                                Layout.margins: Design.space3
                                text: "Desktop notifications use the Windows system tray and are "
                                      + "suppressed for the open chat, muted chats and your own messages."
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                // --- About --------------------------------------------------
                DsSection {
                    Layout.fillWidth: true
                    title: "About"
                    description: "Build information and local data."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space2

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                hoverEnabled: false
                                title: "BeyGram"
                                subtitle: "Windows client"
                                trailing: Text {
                                    text: BeyGramApp.appVersion
                                    color: BeyGramTheme.accent
                                    font.pixelSize: Design.fontSmall
                                    font.weight: Font.DemiBold
                                }
                            }

                            DsDivider { inset: Design.space3 }

                            DsListRow {
                                Layout.fillWidth: true
                                hoverEnabled: false
                                title: "Qt"
                                subtitle: "Application framework"
                                trailing: Text {
                                    text: BeyGramApp.qtVersion
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                    font.weight: Font.DemiBold
                                }
                            }

                            DsDivider { inset: Design.space3 }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.margins: Design.space3
                                spacing: Design.space2

                                Text {
                                    Layout.fillWidth: true
                                    text: "Data folder"
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                    font.weight: Font.DemiBold
                                    font.letterSpacing: 0.3
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: BeyGramApp.dataDirectory
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontSmall
                                    elide: Text.ElideMiddle
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Design.space2

                                    DsButton {
                                        text: "Open data folder"
                                        tone: "secondary"
                                        size: "sm"
                                        onClicked: BeyGramApp.openDataFolder()
                                    }

                                    DsButton {
                                        text: "Copy diagnostics"
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

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Design.space2
                }
            }
        }
    }

    // DsSegmented writes currentIndex itself when a segment is clicked, which
    // drops the binding. Re-sync from the theme whenever it announces a change
    // so Ctrl+D and external resets are reflected here too.
    Connections {
        target: BeyGramTheme

        function onThemeChanged() {
            presetSegment.currentIndex = page.presetIndex()
            modeSegment.currentIndex = page.modeIndex()
        }
    }

    DsDialog {
        id: profileDialog

        title: "Edit profile"
        description: "Leave a field empty to keep it unchanged. The first name cannot be empty."

        DsTextField {
            id: firstNameField
            Layout.fillWidth: true
            label: "First name"
            placeholderText: "Required"
            clearable: true
        }

        DsTextField {
            id: lastNameField
            Layout.fillWidth: true
            label: "Last name"
            placeholderText: "Optional"
            clearable: true
        }

        DsTextField {
            id: usernameField
            Layout.fillWidth: true
            label: "Username"
            placeholderText: "Optional"
            clearable: true
        }

        DsTextField {
            id: bioField
            Layout.fillWidth: true
            label: "Bio"
            placeholderText: "Optional"
            clearable: true
        }

        // A custom footer keeps the Save button disabled until a first name is
        // present, which the shared DsDialog footer cannot express.
        footer: Item {
            implicitHeight: 36 + Design.space5

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Design.space5
                anchors.rightMargin: Design.space5
                anchors.bottomMargin: Design.space5
                spacing: Design.space2

                Item { Layout.fillWidth: true }

                DsButton {
                    text: "Cancel"
                    tone: "secondary"
                    onClicked: profileDialog.close()
                }

                DsButton {
                    text: "Save"
                    enabled: firstNameField.text.trim().length > 0
                    onClicked: {
                        BeyGramApp.saveOwnProfile(firstNameField.text,
                                                  lastNameField.text,
                                                  usernameField.text,
                                                  bioField.text)
                        profileDialog.close()
                    }
                }
            }
        }

        onOpened: {
            firstNameField.text = ""
            lastNameField.text = ""
            usernameField.text = ""
            bioField.text = ""
        }
    }
}
