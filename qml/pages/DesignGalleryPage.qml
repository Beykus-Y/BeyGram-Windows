import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

// DesignGalleryPage — a living reference for the BeyGram design system.
//
// Every section is built from the public Ds* components. Only types that are
// present in qml/ds are referenced; a missing component is omitted rather than
// referenced as an undefined QML type.
Item {
    id: page

    property int selectedChip: 0
    property int selectedSegment: 0

    readonly property var colourTokens: [
        { name: "bg", value: BeyGramTheme.bg },
        { name: "surface", value: BeyGramTheme.surface },
        { name: "surface2", value: BeyGramTheme.surface2 },
        { name: "line", value: BeyGramTheme.line },
        { name: "text", value: BeyGramTheme.text },
        { name: "muted", value: BeyGramTheme.muted },
        { name: "accent", value: BeyGramTheme.accent },
        { name: "accentSoft", value: BeyGramTheme.accentSoft },
        { name: "ok", value: BeyGramTheme.ok },
        { name: "warn", value: BeyGramTheme.warn },
        { name: "danger", value: BeyGramTheme.danger }
    ]

    readonly property var typeScale: [
        { name: "caption", size: Design.fontCaption, weight: Font.Normal },
        { name: "small", size: Design.fontSmall, weight: Font.Normal },
        { name: "body", size: Design.fontBody, weight: Font.Normal },
        { name: "title", size: Design.fontTitle, weight: Font.DemiBold },
        { name: "headline", size: Design.fontHeadline, weight: Font.DemiBold },
        { name: "display", size: Design.fontDisplay, weight: Font.Bold }
    ]

    // A heading plus a DsCard body. The inline component cannot see page-level
    // ids, so it reads the BeyGramTheme singleton directly.
    component GallerySection: ColumnLayout {
        id: section

        property string title: ""
        property string description: ""
        default property alias content: body.data

        Layout.fillWidth: true
        spacing: Design.space3

        Text {
            Layout.fillWidth: true
            text: section.title
            color: BeyGramTheme.text
            font.pixelSize: Design.fontTitle
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: section.description
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            lineHeight: Design.lineNormal
            wrapMode: Text.WordWrap
        }

        DsCard {
            id: card

            Layout.fillWidth: true
            padding: Design.space5
            spacing: 0
            content: ColumnLayout {
                id: body

                Layout.fillWidth: true
                spacing: Design.space4
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: contentColumn

            width: Math.min(960, Math.max(320, page.width - Design.space6 * 2))
            x: Math.max(Design.space6, (page.width - width) / 2)
            y: Design.space6
            spacing: Design.space6

            // --- Page header -------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space5

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Text {
                        Layout.fillWidth: true
                        text: "Design gallery"
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontDisplay
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Colour, type and component states rendered from the live theme. "
                              + "Switch the theme to see every token update in place."
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontBody
                        lineHeight: Design.lineNormal
                        wrapMode: Text.WordWrap
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: Design.space2

                    RowLayout {
                        spacing: Design.space2

                        DsChip {
                            text: BeyGramTheme.presetName
                            selected: true
                        }
                        DsChip {
                            text: BeyGramTheme.modeName
                        }
                    }

                    DsButton {
                        Layout.alignment: Qt.AlignRight
                        text: "Toggle theme"
                        tone: "secondary"
                        size: "sm"
                        onClicked: BeyGramTheme.toggleDarkMode()
                    }
                }
            }

            DsDivider { Layout.fillWidth: true }

            // --- Colour ------------------------------------------------------
            GallerySection {
                title: "Colour"
                description: "Semantic roles from BeyGramTheme. They follow the Warm/Graphite preset and the Light/Dark/AMOLED mode."

                Flow {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Repeater {
                        model: page.colourTokens

                        delegate: ColumnLayout {
                            id: swatch
                            required property var modelData

                            width: 108
                            spacing: Design.space1

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                radius: Design.radiusSm
                                color: swatch.modelData.value
                                border.width: 1
                                border.color: Qt.alpha(BeyGramTheme.text, 0.14)
                            }

                            Text {
                                Layout.fillWidth: true
                                text: swatch.modelData.name
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontSmall
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: String(swatch.modelData.value)
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // --- Typography --------------------------------------------------
            GallerySection {
                title: "Typography"
                description: "The 11–28 px ramp. Weights run from Font.Normal to Font.Bold."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Repeater {
                        model: page.typeScale

                        delegate: RowLayout {
                            id: typeRow
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: Design.space4

                            ColumnLayout {
                                Layout.preferredWidth: 96
                                spacing: 0

                                Text {
                                    text: typeRow.modelData.name
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontSmall
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: typeRow.modelData.size + " px"
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: "BeyGram keeps every conversation close."
                                color: BeyGramTheme.text
                                font.pixelSize: typeRow.modelData.size
                                font.weight: typeRow.modelData.weight
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // --- Buttons -----------------------------------------------------
            GallerySection {
                title: "Buttons"
                description: "DsButton in every tone and size, plus the disabled and loading states."

                Flow {
                    Layout.fillWidth: true
                    spacing: Design.space5

                    Repeater {
                        model: ["primary", "secondary", "ghost", "danger"]

                        delegate: ColumnLayout {
                            id: toneGroup
                            required property string modelData

                            spacing: Design.space2

                            Text {
                                text: toneGroup.modelData.toUpperCase()
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.6
                            }

                            RowLayout {
                                spacing: Design.space2

                                DsButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Small"
                                    tone: toneGroup.modelData
                                    size: "sm"
                                }
                                DsButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Medium"
                                    tone: toneGroup.modelData
                                    size: "md"
                                }
                                DsButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: "Large"
                                    tone: toneGroup.modelData
                                    size: "lg"
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Text {
                        text: "STATES"
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.6
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        DsButton {
                            text: "Disabled"
                            tone: "primary"
                            enabled: false
                        }
                        DsButton {
                            text: "Disabled"
                            tone: "secondary"
                            enabled: false
                        }
                        DsButton {
                            text: "Saving"
                            tone: "primary"
                            loading: true
                        }
                        DsButton {
                            text: "Deleting"
                            tone: "danger"
                            loading: true
                        }
                    }
                }
            }

            // --- Inputs ------------------------------------------------------
            GallerySection {
                title: "Inputs"
                description: "Text, search, switch, slider, segmented and chip controls."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space5

                    DsTextField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        label: "Email"
                        placeholderText: "you@example.com"
                        text: "avery@beygram.app"
                        clearable: true
                    }

                    DsTextField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        label: "Password"
                        placeholderText: "At least 8 characters"
                        text: "short"
                        echoMode: TextInput.Password
                        error: "Use at least 8 characters"
                    }

                    DsSearchField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        placeholderText: "Search messages"
                        text: "design"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsSwitch {
                            Layout.fillWidth: true
                            text: "Read receipts"
                            checked: true
                        }
                        DsSwitch {
                            Layout.fillWidth: true
                            text: "Auto-download photos"
                            checked: false
                        }
                    }

                    DsSlider {
                        Layout.fillWidth: true
                        label: "Message text size"
                        valueText: Math.round(value) + "%"
                        from: 80
                        to: 140
                        value: 100
                        stepSize: 5
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Text {
                            text: "Segmented"
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                        }

                        DsSegmented {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 420
                            Layout.alignment: Qt.AlignLeft
                            model: ["Chats", "Shared", "Archived"]
                            currentIndex: page.selectedSegment
                            onActivated: (index) => page.selectedSegment = index
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Text {
                            text: "Chips"
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsChip {
                                text: "All"
                                selected: page.selectedChip === 0
                                onClicked: page.selectedChip = 0
                            }
                            DsChip {
                                text: "Unread"
                                selected: page.selectedChip === 1
                                onClicked: page.selectedChip = 1
                            }
                            DsChip {
                                text: "Groups"
                                selected: page.selectedChip === 2
                                onClicked: page.selectedChip = 2
                            }
                            DsChip {
                                text: "Channels"
                                selected: page.selectedChip === 3
                                onClicked: page.selectedChip = 3
                            }

                            DsChip {
                                text: "Design"
                                selected: true
                                removable: true
                                onRemoved: galleryToast.show("Chip removed", "info")
                            }
                        }
                    }
                }
            }

            // --- Surfaces ----------------------------------------------------
            GallerySection {
                title: "Surfaces"
                description: "Cards, list rows and conversation rows share the same surface steps and hairlines."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space4

                    DsCard {
                        Layout.fillWidth: true
                        elevated: true
                        padding: Design.space4
                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            Text {
                                Layout.fillWidth: true
                                text: "Weekly digest"
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontBodyLarge
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Three unread messages across two conversations. Nothing else needs your attention."
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                            DsButton {
                                Layout.alignment: Qt.AlignLeft
                                text: "Open chats"
                                tone: "ghost"
                                size: "sm"
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        DsListRow {
                            Layout.fillWidth: true
                            title: "Notifications"
                            subtitle: "Messages, groups and channels"
                            trailing: DsBadge {
                                text: "On"
                                tone: "ok"
                            }
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: "Media auto-download"
                            subtitle: "Photos and videos"
                            trailing: Text {
                                text: "Wi-Fi only"
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                            }
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: "Storage"
                            subtitle: "Cache and local media"
                            trailing: Text {
                                text: "1.2 GB"
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        DsChatRow {
                            Layout.fillWidth: true
                            title: "Avery Chen"
                            preview: "See you at 18:30"
                            timeText: "12:04"
                            unread: 2
                            initials: "AC"
                            accentIndex: 1
                            online: true
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3 + Design.avatarMedium + Design.space3
                        }

                        DsChatRow {
                            Layout.fillWidth: true
                            title: "Design team"
                            preview: "Mira: The new tokens are in."
                            timeText: "Yesterday"
                            pinned: true
                            initials: "DT"
                            accentIndex: 3
                        }
                    }
                }
            }

            // --- Overlays ----------------------------------------------------
            GallerySection {
                title: "Overlays"
                description: "Transient feedback and modal surfaces."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        DsButton {
                            text: "Open dialog"
                            tone: "secondary"
                            onClicked: confirmDialog.open()
                        }
                        DsButton {
                            text: "Open sheet"
                            tone: "secondary"
                            onClicked: shareSheet.open()
                        }
                        DsButton {
                            text: "Open menu"
                            tone: "secondary"
                            onClicked: chatMenu.openAtCursor()
                        }
                        DsButton {
                            text: "Show toast"
                            tone: "secondary"
                            onClicked: galleryToast.show("Design tokens applied", "ok")
                        }
                    }
                }
            }

            // --- Chat --------------------------------------------------------
            GallerySection {
                title: "Chat"
                description: "Incoming and outgoing bubbles plus the composer, with no chat state attached."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space5

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsBubble {
                            Layout.fillWidth: true
                            senderName: "Avery"
                            text: "Hey! Are we still on for tonight?"
                            timeText: "18:02"
                        }

                        DsBubble {
                            Layout.fillWidth: true
                            outgoing: true
                            text: "Yes — I booked the corner table for 19:30."
                            timeText: "18:04"
                            status: "read"
                        }

                        DsBubble {
                            Layout.fillWidth: true
                            outgoing: true
                            text: "I'll send the address in a moment."
                            timeText: "18:04"
                            status: "sent"
                            replyPreview: "Yes — I booked the corner table for 19:30."
                            reactions: "👍 2"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: composer.implicitHeight
                        radius: Design.composerRadius
                        color: BeyGramTheme.surface
                        border.width: 1
                        border.color: BeyGramTheme.line

                        DsComposer {
                            id: composer
                            anchors.fill: parent
                            placeholder: "Message Avery"
                            text: "See you at 19:30"
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Design.space2 }
        }
    }

    DsDialog {
        id: confirmDialog
        title: "Delete conversation?"
        description: "This removes the conversation from this device. It stays on your other devices."
        primaryText: "Delete"
        secondaryText: "Cancel"
        tone: "danger"
        onAccepted: galleryToast.show("Conversation deleted", "danger")
    }

    DsSheet {
        id: shareSheet
        title: "Share conversation"

        DsListRow {
            Layout.fillWidth: true
            title: "Copy link"
            subtitle: "Anyone with the link can join"
        }
        DsListRow {
            Layout.fillWidth: true
            title: "Forward"
            subtitle: "Send to another chat"
        }
        DsButton {
            Layout.alignment: Qt.AlignRight
            text: "Done"
            tone: "secondary"
            size: "sm"
            onClicked: shareSheet.close()
        }
    }

    DsMenu {
        id: chatMenu

        DsMenuItem {
            text: "Mark as read"
            glyph: "✓"
            shortcutText: "Ctrl+R"
            onTriggered: galleryToast.show("Marked as read", "ok")
        }
        DsMenuItem {
            text: "Mute for 8 hours"
            glyph: "◔"
        }
        DsMenuItem {
            text: "Delete conversation"
            glyph: "×"
            tone: "danger"
            onTriggered: galleryToast.show("Conversation deleted", "danger")
        }
    }

    DsToast {
        id: galleryToast
        anchors.fill: parent
        z: 100
    }
}
