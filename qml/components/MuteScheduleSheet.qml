// MuteScheduleSheet - bottom sheet for per-chat mute durations.
//
// Presentation only. The parent assigns `facade` (a MuteScheduleFacade) after
// loading the chat. The facade owns the current label, the offered tokens and
// the mute request; the sheet maps each token to its display text and closes
// once a duration is picked. It keeps no state of its own, never talks to
// TDLib and never logs.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds"
import "../ds/design.js" as Design

DsSheet {
    id: root

    /// MuteScheduleFacade from the C++ side. A null facade renders the empty
    /// unmuted state and offers no options instead of failing.
    property var facade: null

    readonly property var options: root.facade ? root.facade.options : []
    readonly property string currentLabel: root.facade ? root.facade.label
                                                       : qsTr("Unmuted")

    function optionTitle(option) {
        switch (option) {
        case "1h":
            return qsTr("For 1 hour")
        case "8h":
            return qsTr("For 8 hours")
        case "1d":
            return qsTr("For 1 day")
        case "1w":
            return qsTr("For 1 week")
        case "forever":
            return qsTr("Until I turn it back on")
        case "off":
            return qsTr("Unmute")
        default:
            return option
        }
    }

    function optionDescription(option) {
        switch (option) {
        case "1h":
            return qsTr("Notifications are silenced for an hour.")
        case "8h":
            return qsTr("Notifications are silenced for eight hours.")
        case "1d":
            return qsTr("Notifications are silenced for a full day.")
        case "1w":
            return qsTr("Notifications are silenced for a week.")
        case "forever":
            return qsTr("Stay muted until you turn notifications back on.")
        case "off":
            return qsTr("Notifications and sounds resume.")
        default:
            return ""
        }
    }

    function optionGlyph(option) {
        if (option === "forever")
            return "\uE72E" // Lock
        if (option === "off")
            return "\uE73E" // CheckMark
        return "\uE823"     // Clock
    }

    // The duration rows describe a fixed span, so only the two exact states can
    // be marked: unmuted, or muted forever. A timed mute is described by the
    // current label above, because none of the fixed rows is its exact match.
    function isCurrent(option) {
        if (!root.facade)
            return false
        if (option === "off")
            return !root.facade.muted
        if (option === "forever")
            return root.facade.muted && root.facade.label === "Muted forever"
        return false
    }

    function pick(option) {
        if (root.facade)
            root.facade.choose(option)
        root.close()
    }

    title: qsTr("Mute notifications")

    DsCard {
        Layout.fillWidth: true
        elevated: true
        padding: Design.space3

        content: RowLayout {
            Layout.fillWidth: true
            spacing: Design.space3

            DsIcon {
                Layout.alignment: Qt.AlignVCenter
                name: root.facade && root.facade.muted ? "\uE72E" : "\uE73E"
                size: Design.iconSizeLarge
                color: root.facade && root.facade.muted ? BeyGramTheme.accentInk
                                                        : BeyGramTheme.okInk
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Current status")
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.currentLabel
                    color: BeyGramTheme.text
                    font.pixelSize: Design.fontBodyLarge
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
            }
        }
    }

    Repeater {
        id: optionRepeater
        model: root.options

        delegate: DsListRow {
            id: optionRow

            required property string modelData

            Layout.fillWidth: true
            selected: root.isCurrent(optionRow.modelData)
            title: root.optionTitle(optionRow.modelData)
            subtitle: root.optionDescription(optionRow.modelData)

            leading: DsIcon {
                name: root.optionGlyph(optionRow.modelData)
                size: Design.iconSize
                color: optionRow.modelData === "off" ? BeyGramTheme.okInk
                                                      : BeyGramTheme.accentInk
            }

            trailing: DsIcon {
                visible: root.isCurrent(optionRow.modelData)
                name: "\uE73E"
                size: 14
                color: BeyGramTheme.accent
            }

            onClicked: root.pick(optionRow.modelData)
        }
    }

    DsDivider {
        Layout.fillWidth: true
        Layout.topMargin: Design.space2
    }

    DsButton {
        Layout.fillWidth: true
        text: qsTr("Cancel")
        tone: "secondary"
        onClicked: root.close()
    }
}
