import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

/// Built-in wallpaper preset chooser, mirroring the Android dark palette.
/// Presets are rendered locally from gradients; nothing is loaded from disk
/// or the network.
Item {
    id: root

    property string currentPreset: "none"
    property int intensity: 24

    signal presetSelected(string presetId, int intensity)

    /// Stable ids match the persisted Android wallpaper preset ids. Colors are
    /// the dark-variant gradient triplets from WallpaperPresets.kt.
    readonly property var presets: [
        { id: "none", name: "None", colors: [] },
        { id: "lavender-flow", name: "Lavender Flow", colors: ["#18132B", "#272040", "#19172F"] },
        { id: "arctic-blue", name: "Arctic Blue", colors: ["#0E1B2C", "#122C46", "#172033"] },
        { id: "mint-air", name: "Mint Air", colors: ["#102821", "#173A33", "#102B27"] },
        { id: "sunset-peach", name: "Sunset Peach", colors: ["#32212A", "#482D3B", "#2C2134"] },
        { id: "soft-rose", name: "Soft Rose", colors: ["#2B1728", "#3C203B", "#271F39"] },
        { id: "graphite-wave", name: "Graphite Wave", colors: ["#15171E", "#20242E", "#191C26"] },
        { id: "forest-whisper", name: "Forest Whisper", colors: ["#12251F", "#213D31", "#142C28"] },
        { id: "deep-night", name: "Deep Night", colors: ["#101323", "#1E2343", "#141D35"] },
        { id: "violet-mesh", name: "Violet Mesh", colors: ["#231830", "#33234C", "#25213A"] },
        { id: "ocean-current", name: "Ocean Current", colors: ["#0C2530", "#103945", "#0E2535"] },
        { id: "amber-glow", name: "Amber Glow", colors: ["#2F2518", "#453022", "#2D241D"] },
        { id: "sky-paper", name: "Sky Paper", colors: ["#152A38", "#213B4C", "#172B3C"] },
        { id: "terracotta-dust", name: "Terracotta", colors: ["#2E211E", "#493029", "#2A2322"] },
        { id: "silver-mist", name: "Silver Mist", colors: ["#1C202A", "#2C3341", "#1C2534"] },
        { id: "emerald-tide", name: "Emerald Tide", colors: ["#0A2B26", "#12463D", "#0A302B"] }
    ]

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    /// The value the next selection should carry: the slider is the live source
    /// while dragging, otherwise the committed property wins.
    function effectiveIntensity() {
        return Math.max(0, Math.min(45, Math.round(intensitySlider.value)))
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        spacing: 10

        GridLayout {
            id: grid
            columns: 4
            columnSpacing: 10
            rowSpacing: 10

            Repeater {
                model: root.presets

                delegate: Column {
                    id: cell
                    spacing: 4
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: cell.implicitHeight

                    readonly property bool isCurrent: root.currentPreset === modelData.id

                    function swatchColor(index) {
                        return modelData.colors.length === 0
                                ? BeyGramTheme.surface2
                                : modelData.colors[index]
                    }

                    Rectangle {
                        id: swatch
                        width: 72
                        height: 48
                        radius: 10
                        color: BeyGramTheme.surface2
                        border.width: cell.isCurrent ? 2 : 1
                        border.color: cell.isCurrent ? BeyGramTheme.accent : Qt.alpha(BeyGramTheme.line, 0.9)
                        clip: true

                        gradient: Gradient {
                            GradientStop { position: 0.0; color: cell.swatchColor(0) }
                            GradientStop { position: 0.55; color: cell.swatchColor(1) }
                            GradientStop { position: 1.0; color: cell.swatchColor(2) }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: swatch.radius
                            color: Qt.alpha(BeyGramTheme.text, 0.10)
                            opacity: hoverArea.containsMouse && !cell.isCurrent ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }

                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: modelData.name
                            Accessible.checked: cell.isCurrent
                            onClicked: root.presetSelected(modelData.id, root.effectiveIntensity())
                        }
                    }

                    Text {
                        width: 72
                        text: modelData.name
                        font.pixelSize: 10
                        font.weight: cell.isCurrent ? Font.DemiBold : Font.Normal
                        color: cell.isCurrent ? BeyGramTheme.accentInk : BeyGramTheme.muted
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: "Intensity"
                font.pixelSize: 11
                color: BeyGramTheme.muted
            }

            Slider {
                id: intensitySlider
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                from: 0
                to: 45
                stepSize: 1
                value: root.intensity
                Accessible.name: "Wallpaper intensity"
                onPressedChanged: {
                    if (!pressed)
                        root.presetSelected(root.currentPreset, root.effectiveIntensity())
                }

                background: Rectangle {
                    x: intensitySlider.leftPadding
                    y: intensitySlider.topPadding
                       + intensitySlider.availableHeight / 2 - height / 2
                    width: intensitySlider.availableWidth
                    height: 4
                    radius: 2
                    color: BeyGramTheme.line

                    Rectangle {
                        width: intensitySlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: BeyGramTheme.accent
                    }
                }

                handle: Rectangle {
                    x: intensitySlider.leftPadding
                       + intensitySlider.visualPosition
                         * (intensitySlider.availableWidth - width)
                    y: intensitySlider.topPadding
                       + intensitySlider.availableHeight / 2 - height / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: BeyGramTheme.accent
                    border.width: 2
                    border.color: BeyGramTheme.surface
                }
            }

            Text {
                Layout.preferredWidth: 24
                text: Math.round(intensitySlider.value)
                font.pixelSize: 11
                color: BeyGramTheme.muted
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
