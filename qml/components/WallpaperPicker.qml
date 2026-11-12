import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds"
import "../ds/design.js" as Design

/// Built-in wallpaper preset chooser, mirroring Android `WallpaperPresets.kt`.
/// Renders live mini `ChatWallpaper` previews for all 16 presets + None.
Item {
    id: root

    property string currentPreset: "none"
    property int intensity: 24

    signal presetSelected(string presetId, int intensity)

    readonly property var presets: [
        { id: "none", name: qsTr("None") },
        { id: "lavender-flow", name: qsTr("Lavender Flow") },
        { id: "arctic-blue", name: qsTr("Arctic Blue") },
        { id: "mint-air", name: qsTr("Mint Air") },
        { id: "sunset-peach", name: qsTr("Sunset Peach") },
        { id: "soft-rose", name: qsTr("Soft Rose") },
        { id: "blue-notes", name: qsTr("Blue Notes") },
        { id: "graphite-wave", name: qsTr("Graphite Wave") },
        { id: "forest-whisper", name: qsTr("Forest Whisper") },
        { id: "deep-night", name: qsTr("Deep Night") },
        { id: "violet-mesh", name: qsTr("Violet Mesh") },
        { id: "ocean-current", name: qsTr("Ocean Current") },
        { id: "amber-glow", name: qsTr("Amber Glow") },
        { id: "sky-paper", name: qsTr("Sky Paper") },
        { id: "terracotta-dust", name: qsTr("Terracotta") },
        { id: "silver-mist", name: qsTr("Silver Mist") },
        { id: "emerald-tide", name: qsTr("Emerald Tide") }
    ]

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    function effectiveIntensity() {
        return Math.max(0, Math.min(45, Math.round(intensitySlider.value)))
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        Flow {
            id: swatchFlow
            Layout.fillWidth: true
            spacing: Design.space3

            Repeater {
                model: root.presets

                delegate: Column {
                    id: cell
                    required property var modelData

                    width: 84
                    spacing: Design.space1

                    readonly property bool isCurrent: root.currentPreset === cell.modelData.id

                    Item {
                        id: swatchFrame
                        width: 84
                        height: 58

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 1
                            radius: Design.radiusSm - 1
                            color: BeyGramTheme.surface2
                            clip: true

                            ChatWallpaper {
                                anchors.fill: parent
                                presetId: cell.modelData.id
                                intensity: cell.modelData.id === "none" ? 0 : Math.max(20, root.intensity)
                            }
                        }

                        // Empty diagonal/subtle icon for "None"
                        Text {
                            anchors.centerIn: parent
                            visible: cell.modelData.id === "none"
                            text: "—"
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontBody
                        }

                        // Hover & border ring
                        Rectangle {
                            anchors.fill: parent
                            radius: Design.radiusSm + 1
                            color: hoverArea.containsMouse && !cell.isCurrent
                                   ? Qt.alpha(BeyGramTheme.text, 0.08)
                                   : "transparent"
                            border.width: cell.isCurrent ? 2 : 1
                            border.color: cell.isCurrent
                                          ? BeyGramTheme.accent
                                          : BeyGramTheme.line
                            antialiasing: true
                        }

                        // Active checkmark pill
                        Rectangle {
                            visible: cell.isCurrent
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 5
                            width: 18
                            height: 18
                            radius: 9
                            color: BeyGramTheme.accent

                            Text {
                                anchors.centerIn: parent
                                text: "\uE73E"
                                font.family: "Segoe MDL2 Assets"
                                font.pixelSize: 10
                                color: BeyGramTheme.onAccent
                            }
                        }

                        MouseArea {
                            id: hoverArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: cell.modelData.name
                            Accessible.checked: cell.isCurrent
                            onClicked: root.presetSelected(cell.modelData.id, root.effectiveIntensity())
                        }
                    }

                    Text {
                        width: 84
                        text: cell.modelData.name
                        font.pixelSize: Design.fontCaption
                        font.weight: cell.isCurrent ? Font.DemiBold : Font.Normal
                        color: cell.isCurrent ? BeyGramTheme.accentInk : BeyGramTheme.muted
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }

        DsSlider {
            id: intensitySlider
            Layout.fillWidth: true
            label: qsTr("Pattern intensity")
            valueText: Math.round(value) + "%"
            from: 0
            to: 45
            stepSize: 1
            value: root.intensity
            enabled: root.currentPreset !== "none"
            Accessible.name: qsTr("Wallpaper pattern intensity")

            onMoved: root.presetSelected(root.currentPreset, root.effectiveIntensity())
        }
    }
}
