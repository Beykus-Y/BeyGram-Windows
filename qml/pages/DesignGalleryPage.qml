import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

// DesignGalleryPage — a living reference for the BeyGram design system.
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

            RowLayout {
                Layout.fillWidth: true
                spacing: Design.space5

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Design gallery")
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontDisplay
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Colour, type and component states rendered from the live theme. Switch the theme to see every token update in place.")
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
                        text: qsTr("Toggle theme")
                        tone: "secondary"
                        size: "sm"
                        onClicked: BeyGramTheme.toggleDarkMode()
                    }
                }
            }

            DsDivider { Layout.fillWidth: true }

            GallerySection {
                title: qsTr("Colour")
                description: qsTr("Semantic roles from BeyGramTheme. They follow the Warm/Graphite preset and the Light/Dark/AMOLED mode.")

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

            GallerySection {
                title: qsTr("Typography")
                description: qsTr("The 11–28 px ramp. Weights run from Font.Normal to Font.Bold.")

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
                                    text: qsTr("%1 px").arg(typeRow.modelData.size)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("BeyGram keeps every conversation close.")
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

            GallerySection {
                title: qsTr("Buttons")
                description: qsTr("DsButton in every tone and size, plus the disabled and loading states.")

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
                                    text: qsTr("Small")
                                    tone: toneGroup.modelData
                                    size: "sm"
                                }
                                DsButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: qsTr("Medium")
                                    tone: toneGroup.modelData
                                    size: "md"
                                }
                                DsButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: qsTr("Large")
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
                        text: qsTr("STATES")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.6
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        DsButton {
                            text: qsTr("Disabled")
                            tone: "primary"
                            enabled: false
                        }
                        DsButton {
                            text: qsTr("Disabled")
                            tone: "secondary"
                            enabled: false
                        }
                        DsButton {
                            text: qsTr("Saving")
                            tone: "primary"
                            loading: true
                        }
                        DsButton {
                            text: qsTr("Deleting")
                            tone: "danger"
                            loading: true
                        }
                    }
                }
            }

            GallerySection {
                title: qsTr("Inputs")
                description: qsTr("Text, search, switch, slider, segmented and chip controls.")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space5

                    DsTextField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        label: qsTr("Email")
                        placeholderText: "you@example.com"
                        text: "avery@beygram.app"
                        clearable: true
                    }

                    DsTextField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        label: qsTr("Password")
                        placeholderText: qsTr("At least 8 characters")
                        text: "short"
                        echoMode: TextInput.Password
                        error: qsTr("Use at least 8 characters")
                    }

                    DsSearchField {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 480
                        Layout.alignment: Qt.AlignLeft
                        placeholderText: qsTr("Search messages")
                        text: "design"
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsSwitch {
                            Layout.fillWidth: true
                            text: qsTr("Read receipts")
                            checked: true
                        }
                        DsSwitch {
                            Layout.fillWidth: true
                            text: qsTr("Auto-download photos")
                            checked: false
                        }
                    }

                    DsSlider {
                        Layout.fillWidth: true
                        label: qsTr("Message text size")
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
                            text: qsTr("Segmented")
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                        }

                        DsSegmented {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 420
                            Layout.alignment: Qt.AlignLeft
                            model: [qsTr("Chats"), qsTr("Shared"), qsTr("Archived")]
                            currentIndex: page.selectedSegment
                            onActivated: (index) => page.selectedSegment = index
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Text {
                            text: qsTr("Chips")
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsChip {
                                text: qsTr("All")
                                selected: page.selectedChip === 0
                                onClicked: page.selectedChip = 0
                            }
                            DsChip {
                                text: qsTr("Unread")
                                selected: page.selectedChip === 1
                                onClicked: page.selectedChip = 1
                            }
                            DsChip {
                                text: qsTr("Groups")
                                selected: page.selectedChip === 2
                                onClicked: page.selectedChip = 2
                            }
                            DsChip {
                                text: qsTr("Channels")
                                selected: page.selectedChip === 3
                                onClicked: page.selectedChip = 3
                            }

                            DsChip {
                                text: qsTr("Design")
                                selected: true
                                removable: true
                                onRemoved: galleryToast.show(qsTr("Chip removed"), "info")
                            }
                        }
                    }
                }
            }

            GallerySection {
                title: qsTr("Surfaces")
                description: qsTr("Cards, list rows and conversation rows share the same surface steps and hairlines.")

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
                                text: qsTr("Weekly digest")
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontBodyLarge
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Three unread messages across two conversations. Nothing else needs your attention.")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                            DsButton {
                                Layout.alignment: Qt.AlignLeft
                                text: qsTr("Open chats")
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
                            title: qsTr("Notifications")
                            subtitle: qsTr("Messages, groups and channels")
                            trailing: DsBadge {
                                text: qsTr("On")
                                tone: "ok"
                            }
                        }

                        DsDivider {
                            Layout.fillWidth: true
                            inset: Design.space3
                        }

                        DsListRow {
                            Layout.fillWidth: true
                            title: qsTr("Media auto-download")
                            subtitle: qsTr("Photos and videos")
                            trailing: Text {
                                text: qsTr("Wi-Fi only")
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
                            title: qsTr("Storage")
                            subtitle: qsTr("Cache and local media")
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
                            preview: qsTr("See you at 18:30")
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
                            title: qsTr("Design team")
                            preview: qsTr("Mira: The new tokens are in.")
                            timeText: qsTr("Yesterday")
                            pinned: true
                            initials: "DT"
                            accentIndex: 3
                        }
                    }
                }
            }

            GallerySection {
                title: qsTr("Overlays")
                description: qsTr("Transient feedback and modal surfaces.")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        DsButton {
                            text: qsTr("Open dialog")
                            tone: "secondary"
                            onClicked: confirmDialog.open()
                        }
                        DsButton {
                            text: qsTr("Open sheet")
                            tone: "secondary"
                            onClicked: shareSheet.open()
                        }
                        DsButton {
                            text: qsTr("Open menu")
                            tone: "secondary"
                            onClicked: chatMenu.openAtCursor()
                        }
                        DsButton {
                            text: qsTr("Show toast")
                            tone: "secondary"
                            onClicked: galleryToast.show(qsTr("Design tokens applied"), "ok")
                        }
                    }
                }
            }

            GallerySection {
                title: qsTr("Chat")
                description: qsTr("Incoming and outgoing bubbles plus the composer, with no chat state attached.")

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space5

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsBubble {
                            Layout.fillWidth: true
                            senderName: "Avery"
                            text: qsTr("Hey! Are we still on for tonight?")
                            timeText: "18:02"
                        }

                        DsBubble {
                            Layout.fillWidth: true
                            outgoing: true
                            text: qsTr("Yes — I booked the corner table for 19:30.")
                            timeText: "18:04"
                            status: "read"
                        }

                        DsBubble {
                            Layout.fillWidth: true
                            outgoing: true
                            text: qsTr("I'll send the address in a moment.")
                            timeText: "18:04"
                            status: "sent"
                            replyPreview: qsTr("Yes — I booked the corner table for 19:30.")
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
                            placeholder: qsTr("Message Avery")
                            text: qsTr("See you at 19:30")
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Design.space2 }
        }
    }

    DsDialog {
        id: confirmDialog
        title: qsTr("Delete conversation?")
        description: qsTr("This removes the conversation from this device. It stays on your other devices.")
        primaryText: qsTr("Delete")
        secondaryText: qsTr("Cancel")
        tone: "danger"
        onAccepted: galleryToast.show(qsTr("Conversation deleted"), "danger")
    }

    DsSheet {
        id: shareSheet
        title: qsTr("Share conversation")

        DsListRow {
            Layout.fillWidth: true
            title: qsTr("Copy link")
            subtitle: qsTr("Anyone with the link can join")
        }
        DsListRow {
            Layout.fillWidth: true
            title: qsTr("Forward")
            subtitle: qsTr("Send to another chat")
        }
        DsButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Done")
            tone: "secondary"
            size: "sm"
            onClicked: shareSheet.close()
        }
    }

    DsMenu {
        id: chatMenu

        DsMenuItem {
            text: qsTr("Mark as read")
            glyph: "✓"
            shortcutText: "Ctrl+R"
            onTriggered: galleryToast.show(qsTr("Marked as read"), "ok")
        }
        DsMenuItem {
            text: qsTr("Mute for 8 hours")
            glyph: "◔"
        }
        DsMenuItem {
            text: qsTr("Delete conversation")
            glyph: "×"
            tone: "danger"
            onTriggered: galleryToast.show(qsTr("Conversation deleted"), "danger")
        }
    }

    DsToast {
        id: galleryToast
        anchors.fill: parent
        z: 100
    }
}
