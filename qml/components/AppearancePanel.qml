import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds"
import "../ds/design.js" as Design

// AppearancePanel — live appearance editor wired to BeyGramAppearance
// (AppearanceStore, persisted to appearance.json) and WallpaperPicker.
// Also syncs reduceMotion if an optional AppearanceFacade is attached.
Item {
    id: root

    property var facade: null

    readonly property var presetOptions: [
        { id: "Warm", label: qsTr("Warm") },
        { id: "Graphite", label: qsTr("Graphite") }
    ]

    readonly property var modeOptions: [
        { id: "FollowSystem", label: qsTr("System") },
        { id: "Light", label: qsTr("Light") },
        { id: "Dark", label: qsTr("Dark") },
        { id: "Amoled", label: qsTr("AMOLED") }
    ]

    readonly property var styleOptions: [
        { id: "Default", label: qsTr("Default") },
        { id: "Material2", label: qsTr("Material 2") },
        { id: "Material3", label: qsTr("Material 3") },
        { id: "LiquidGlass", label: qsTr("Liquid Glass") },
        { id: "Signature", label: qsTr("Signature") }
    ]

    readonly property var bubbleOptions: [
        { id: "Rounded", label: qsTr("Rounded") },
        { id: "Soft", label: qsTr("Soft") },
        { id: "Square", label: qsTr("Square") }
    ]

    readonly property var densityOptions: [
        { id: "Comfortable", label: qsTr("Comfortable") },
        { id: "Compact", label: qsTr("Compact") }
    ]

    function indexOfOption(options, value) {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].id === value)
                return i
        }
        return 0
    }

    function syncControls() {
        presetSeg.currentIndex = indexOfOption(presetOptions, BeyGramAppearance.themePreset)
        modeSeg.currentIndex = indexOfOption(modeOptions, BeyGramAppearance.themeMode)
        bubbleSeg.currentIndex = indexOfOption(bubbleOptions, BeyGramAppearance.bubbleStyle)
        densitySeg.currentIndex = indexOfOption(densityOptions, BeyGramAppearance.listDensity)
        if (!textSizeSlider.pressed)
            textSizeSlider.value = BeyGramAppearance.messageTextSize
        reduceMotionSwitch.checked = BeyGramAppearance.reduceMotion
    }

    implicitWidth: 460
    implicitHeight: content.implicitHeight

    Component.onCompleted: syncControls()

    Connections {
        target: BeyGramAppearance
        function onChanged() {
            root.syncControls()
        }
    }

    Connections {
        target: BeyGramTheme
        function onThemeChanged() {
            root.syncControls()
        }
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space5

        // --- Live Preview ---------------------------------------------------
        DsSection {
            Layout.fillWidth: true
            title: qsTr("Preview")
            description: qsTr("Changes apply immediately across chats and panels.")

            content: DsCard {
                Layout.fillWidth: true
                padding: 0

                content: Item {
                    Layout.fillWidth: true
                    implicitHeight: previewCol.implicitHeight + Design.space5 * 2

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: Math.max(4, Design.cardRadius(BeyGramAppearance.designStyle) - 1)
                        color: "transparent"
                        clip: true

                        ChatWallpaper {
                            anchors.fill: parent
                            presetId: BeyGramAppearance.wallpaperPreset
                            intensity: BeyGramAppearance.wallpaperPreset === "none"
                                       ? 0
                                       : Math.max(22, BeyGramAppearance.wallpaperIntensity)
                        }
                    }

                    ColumnLayout {
                        id: previewCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: Design.space4
                        spacing: Design.space2

                        DsBubble {
                            Layout.fillWidth: true
                            senderName: qsTr("Mira")
                            text: qsTr("How does the new theme look on desktop?")
                            timeText: "18:42"
                        }

                        DsBubble {
                            Layout.fillWidth: true
                            outgoing: true
                            text: qsTr("Warm, crisp, and super easy on the eyes.")
                            timeText: "18:43"
                            status: "read"
                        }
                    }
                }
            }
        }

        // --- Colour & Brightness --------------------------------------------
        DsSection {
            Layout.fillWidth: true
            title: qsTr("Theme & Brightness")
            description: qsTr("Colour family and brightness mode for the whole client.")

            content: DsCard {
                Layout.fillWidth: true
                padding: Design.space4

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Theme preset")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: presetSeg
                        Layout.fillWidth: true
                        model: root.presetOptions.map(function (o) { return o.label })
                        currentIndex: root.indexOfOption(root.presetOptions, BeyGramAppearance.themePreset)
                        onActivated: function (index) {
                            const id = root.presetOptions[index].id
                            BeyGramAppearance.setThemePreset(id)
                            BeyGramTheme.presetName = id
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Mode")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: modeSeg
                        Layout.fillWidth: true
                        model: root.modeOptions.map(function (o) { return o.label })
                        currentIndex: root.indexOfOption(root.modeOptions, BeyGramAppearance.themeMode)
                        onActivated: function (index) {
                            const id = root.modeOptions[index].id
                            BeyGramAppearance.setThemeMode(id)
                            BeyGramTheme.modeName = id
                        }
                    }
                }
            }
        }

        // --- Design Style & Layout ------------------------------------------
        DsSection {
            Layout.fillWidth: true
            title: qsTr("Style & Layout")
            description: qsTr("Surface shape, message bubbles, density and text scale.")

            content: DsCard {
                Layout.fillWidth: true
                padding: Design.space4

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Design style")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Repeater {
                            model: root.styleOptions

                            delegate: DsChip {
                                required property var modelData
                                text: modelData.label
                                selected: BeyGramAppearance.designStyle === modelData.id
                                onClicked: BeyGramAppearance.setDesignStyle(modelData.id)
                            }
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Bubble style")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: bubbleSeg
                        Layout.fillWidth: true
                        model: root.bubbleOptions.map(function (o) { return o.label })
                        currentIndex: root.indexOfOption(root.bubbleOptions, BeyGramAppearance.bubbleStyle)
                        onActivated: function (index) {
                            BeyGramAppearance.setBubbleStyle(root.bubbleOptions[index].id)
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Chat list density")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                    }

                    DsSegmented {
                        id: densitySeg
                        Layout.fillWidth: true
                        model: root.densityOptions.map(function (o) { return o.label })
                        currentIndex: root.indexOfOption(root.densityOptions, BeyGramAppearance.listDensity)
                        onActivated: function (index) {
                            BeyGramAppearance.setListDensity(root.densityOptions[index].id)
                        }
                    }

                    DsDivider { Layout.fillWidth: true }

                    DsSlider {
                        id: textSizeSlider
                        Layout.fillWidth: true
                        label: qsTr("Message text size")
                        valueText: Math.round(value) + " px"
                        from: 12
                        to: 18
                        stepSize: 1
                        value: BeyGramAppearance.messageTextSize
                        Accessible.name: qsTr("Message text size")
                        onMoved: BeyGramAppearance.setMessageTextSize(Math.round(value))
                    }

                    DsDivider { Layout.fillWidth: true }

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Reduce motion")
                        subtitle: qsTr("Minimize transitions and animations across the interface.")
                        onClicked: reduceMotionSwitch.toggle()

                        trailing: DsSwitch {
                            id: reduceMotionSwitch
                            checked: BeyGramAppearance.reduceMotion
                            Accessible.name: qsTr("Reduce motion")
                            onToggled: {
                                BeyGramAppearance.setReduceMotion(checked)
                                if (root.facade)
                                    root.facade.reduceMotion = checked
                            }
                        }
                    }
                }
            }
        }

        // --- Chat Wallpaper -------------------------------------------------
        DsSection {
            Layout.fillWidth: true
            title: qsTr("Chat wallpaper")
            description: qsTr("Deterministic built-in gradients and patterns, matching Android BeyGram.")

            content: DsCard {
                Layout.fillWidth: true
                padding: Design.space4

                content: WallpaperPicker {
                    Layout.fillWidth: true
                    currentPreset: BeyGramAppearance.wallpaperPreset
                    intensity: BeyGramAppearance.wallpaperIntensity
                    onPresetSelected: function (presetId, intensity) {
                        BeyGramAppearance.setWallpaperPreset(presetId)
                        BeyGramAppearance.setWallpaperIntensity(intensity)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            DsButton {
                text: qsTr("Reset to defaults")
                tone: "secondary"
                size: "sm"
                onClicked: resetDialog.open()
            }
        }
    }

    DsDialog {
        id: resetDialog
        title: qsTr("Reset appearance?")
        description: qsTr("Theme preset, brightness mode, design style, bubble style, text size and wallpaper will return to their defaults.")
        primaryText: qsTr("Reset")
        secondaryText: qsTr("Cancel")
        tone: "danger"
        onAccepted: {
            BeyGramAppearance.setThemePreset("Warm")
            BeyGramAppearance.setThemeMode("FollowSystem")
            BeyGramTheme.presetName = "Warm"
            BeyGramTheme.modeName = "FollowSystem"
            BeyGramAppearance.setDesignStyle("Default")
            BeyGramAppearance.setBubbleStyle("Rounded")
            BeyGramAppearance.setListDensity("Comfortable")
            BeyGramAppearance.setMessageTextSize(14)
            BeyGramAppearance.setReduceMotion(false)
            BeyGramAppearance.setWallpaperPreset("none")
            BeyGramAppearance.setWallpaperIntensity(24)
            if (root.facade)
                root.facade.reset()
        }
    }
}
