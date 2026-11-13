// EqualizerPanel — the music equalizer panel.
//
// Presentation only. Every value comes from the injected `facade`
// (beygram::ui::EqualizerFacade) and every edit goes back through it: a band
// slider calls facade.setGain(index, dB), a preset chip calls
// facade.applyPreset(name), the switch calls facade.setEnabled() and Reset calls
// facade.reset(). Nothing here attaches an audio effect: the facade holds the
// curve and the switch, and the playback backend reads them.
//
// The facade is nullable so the panel can be created before the controller is
// wired up. Until it is set, the band list is empty, the switch is off and the
// controls do nothing.
//
// The band index a slider passes to setGain() is its position in the repeater,
// which is the same position the facade's label and gain lists use. The gain is
// shown from the facade's own "x.y dB" string, and the slider parses the number
// back out of it, so a preset or a reset moves every handle.
//
// Layout comes from the design system: a DsSection header, a DsCard holding the
// enable row, the vertical band sliders and the preset chips, and a DsButton for
// Reset.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    /// The EqualizerFacade driving this panel. May be null while the page is
    /// still being constructed, so every binding below is null-safe.
    property var facade: null

    readonly property var bandLabels: root.facade ? root.facade.bandLabels : []
    readonly property var gains: root.facade ? root.facade.gains : []
    readonly property var presets: root.facade ? root.facade.presets : []
    readonly property string preset: root.facade ? root.facade.preset : ""
    readonly property bool equalizerOn: root.facade ? root.facade.enabled : false

    /// The range the model clamps to, and the step a slider moves in.
    readonly property int minGainDb: -12
    readonly property int maxGainDb: 12
    readonly property real gainStepDb: 0.5

    implicitWidth: 440
    implicitHeight: layout.implicitHeight

    /// The label of a band, or "" when the facade has no such band yet.
    function bandLabelAt(bandIndex) {
        const label = root.bandLabels[bandIndex]
        return label === undefined || label === null ? "" : String(label)
    }

    /// The formatted gain of a band, or "" when the facade has no such band.
    function gainTextAt(bandIndex) {
        const text = root.gains[bandIndex]
        return text === undefined || text === null ? "" : String(text)
    }

    /// The name of a preset, or "" when the facade has no such preset.
    function presetAt(presetIndex) {
        const name = root.presets[presetIndex]
        return name === undefined || name === null ? "" : String(name)
    }

    /// The numeric gain of a band, parsed out of the facade's formatted string.
    /// The slider works in this number; the label keeps the facade's text.
    function gainAt(bandIndex) {
        const value = parseFloat(root.gainTextAt(bandIndex))
        if (isNaN(value))
            return 0
        return Math.max(root.minGainDb, Math.min(root.maxGainDb, value))
    }

    // A facade attached after construction has to seed the switch, which a
    // user toggle would otherwise have left without a binding. The guard covers
    // the property being assigned before the switch is created.
    onFacadeChanged: {
        if (enableSwitch)
            enableSwitch.checked = root.equalizerOn
    }

    ColumnLayout {
        id: layout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Equalizer")
            description: qsTr("Shape the tone of music playback.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Enable equalizer")
                        subtitle: !root.facade
                                  ? ""
                                  : (root.equalizerOn
                                     ? qsTr("The curve is applied to music playback.")
                                     : qsTr("The curve is kept but not applied."))
                        hoverEnabled: false

                        trailing: DsSwitch {
                            id: enableSwitch

                            checked: root.equalizerOn
                            Accessible.name: qsTr("Enable equalizer")

                            onToggled: {
                                if (root.facade)
                                    root.facade.setEnabled(enableSwitch.checked)
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    // One column per band: the formatted gain, the vertical
                    // slider and the frequency label, low to high.
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Design.space2
                        spacing: Design.space2

                        Repeater {
                            id: bandRepeater

                            // The count, not the label list: every facade
                            // change re-reads bandLabels() as a new array, and
                            // modelling the array itself would rebuild the
                            // delegates — and drop the slider under the pointer
                            // — on every gain edit. A count that stays 5 keeps
                            // them alive; each delegate reads its own label.
                            model: root.bandLabels.length

                            delegate: ColumnLayout {
                                id: bandColumn

                                required property int index

                                Layout.fillWidth: true
                                spacing: Design.space2

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.gainTextAt(bandColumn.index)
                                    color: BeyGramTheme.text
                                    font.pixelSize: Design.fontSmall
                                    font.weight: Font.DemiBold
                                    Accessible.ignored: true
                                }

                                Slider {
                                    id: bandSlider

                                    readonly property int handleSize: 16

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 168
                                    orientation: Qt.Vertical
                                    from: root.minGainDb
                                    to: root.maxGainDb
                                    stepSize: root.gainStepDb
                                    snapMode: Slider.SnapAlways
                                    // No padding, so the track, the 0 dB tick and
                                    // the handle all share the same geometry.
                                    leftPadding: 0
                                    rightPadding: 0
                                    topPadding: 0
                                    bottomPadding: 0
                                    value: root.gainAt(bandColumn.index)
                                    hoverEnabled: true
                                    activeFocusOnTab: true
                                    opacity: root.facade ? 1.0 : Design.opacityDisabled
                                    Accessible.name: qsTr("%1 band").arg(root.bandLabelAt(bandColumn.index))
                                    Accessible.description: root.gainTextAt(bandColumn.index)

                                    onMoved: {
                                        if (root.facade)
                                            root.facade.setGain(bandColumn.index, bandSlider.value)
                                    }

                                    background: Item {
                                        id: bandTrack

                                        readonly property real zeroY: bandSlider.height / 2
                                        readonly property real handleCenterY: (1 - bandSlider.visualPosition)
                                                                                * (bandSlider.height - bandSlider.handleSize)
                                                                                + bandSlider.handleSize / 2

                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.top: parent.top
                                            anchors.bottom: parent.bottom
                                            width: 4
                                            radius: width / 2
                                            color: BeyGramTheme.surface2
                                        }

                                        // Fill from the 0 dB line to the handle,
                                        // so a boost and a cut read differently.
                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: 4
                                            radius: width / 2
                                            color: BeyGramTheme.accent
                                            y: Math.min(bandTrack.zeroY, bandTrack.handleCenterY)
                                            height: Math.abs(bandTrack.handleCenterY - bandTrack.zeroY)
                                            visible: height > 0.5
                                        }

                                        // The 0 dB tick.
                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: bandTrack.zeroY - height / 2
                                            width: 12
                                            height: 1
                                            color: BeyGramTheme.line
                                        }
                                    }

                                    handle: Rectangle {
                                        x: (bandSlider.width - width) / 2
                                        y: (1 - bandSlider.visualPosition) * (bandSlider.height - height)
                                        width: bandSlider.handleSize
                                        height: bandSlider.handleSize
                                        radius: width / 2
                                        color: BeyGramTheme.accent
                                        border.width: 2
                                        border.color: BeyGramTheme.surface

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: parent.width + 6
                                            height: parent.height + 6
                                            radius: width / 2
                                            color: "transparent"
                                            border.width: 1
                                            border.color: BeyGramTheme.accent
                                            visible: bandSlider.visualFocus
                                        }
                                    }
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.bandLabelAt(bandColumn.index)
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontCaption
                                    Accessible.ignored: true
                                }

                                // A drag writes the slider's value directly,
                                // which replaces the binding, so the handle is
                                // re-synced whenever the facade reports a real
                                // change: a preset, a reset or another view.
                                Connections {
                                    target: root.facade

                                    function onChanged() {
                                        if (!bandSlider.pressed)
                                            bandSlider.value = root.gainAt(bandColumn.index)
                                    }
                                }
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Design.space2
                        spacing: Design.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            Text {
                                text: qsTr("Presets")
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontBody
                                font.weight: Font.DemiBold
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            Text {
                                text: root.facade
                                      ? (root.preset.length > 0 ? root.preset : qsTr("Custom"))
                                      : ""
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            Repeater {
                                // The count, for the same reason as the band
                                // repeater: the name list is re-read as a new
                                // array on every change, while the six presets
                                // never change.
                                model: root.presets.length

                                delegate: DsChip {
                                    id: presetChip

                                    required property int index

                                    readonly property string presetName: root.presetAt(presetChip.index)

                                    text: presetChip.presetName
                                    selected: root.preset === presetChip.presetName

                                    onClicked: {
                                        if (root.facade)
                                            root.facade.applyPreset(presetChip.presetName)
                                    }
                                }
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Design.space2
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Each band can be cut or boosted by up to %1 dB.").arg(root.maxGainDb)
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            elide: Text.ElideRight
                        }

                        DsButton {
                            text: qsTr("Reset")
                            icon: "\uE72C" // Segoe MDL2 Assets: Refresh
                            tone: "secondary"
                            size: "sm"
                            enabled: root.facade ? true : false
                            onClicked: {
                                if (root.facade)
                                    root.facade.reset()
                            }
                        }
                    }
                }
            }
        }
    }

    // A user toggle replaces the switch's `checked` binding, so it is re-synced
    // whenever the facade reports a real change.
    Connections {
        target: root.facade

        function onChanged() {
            enableSwitch.checked = root.equalizerOn
        }
    }
}
