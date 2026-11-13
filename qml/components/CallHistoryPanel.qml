// CallHistoryPanel - the call log of one account.
//
// The parent owns the data layer: it assigns a CallHistoryFacade to `facade`
// and reacts to callBackRequested(callId) by starting a new call. This file
// only renders the prepared rows; it never talks to TDLib, the network or the
// clock.
//
// Each entry is a compact JSON object built by the facade:
//
//   { id, name, outgoing, missed, date, duration, durationLabel }
//
// `date` is epoch seconds and is formatted here through BeyGramLang so the
// panel follows the selected language; `durationLabel` is the facade's mm:ss
// rendering. A missed call is painted in the danger colour on the direction
// badge, the name and the "Missed" label. The call-back button emits the
// facade's callBackRequested() with the row's id.
//
// Layout comes from the design system: one DsSection, one DsCard, a row per
// call with DsIconButton for the call-back action, DsDivider between rows and
// DsEmptyState when there is nothing to show.

import QtQuick
import QtQuick.Layouts
import BeyGram.App 1.0
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: panel

    property var facade: null

    readonly property bool hasFacade: panel.facade !== null
    readonly property var entries: panel.hasFacade ? panel.facade.entries : []
    readonly property int missedCount: panel.hasFacade ? panel.facade.missedCount : 0
    readonly property bool empty: panel.hasFacade ? panel.facade.empty : true

    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"
    readonly property int rowHeight: panel.compact ? Design.rowHeightCompact : Design.rowHeight

    implicitWidth: 520
    implicitHeight: content.implicitHeight

    // Reads one facade entry. A malformed line falls back to an empty object,
    // so one bad record cannot take the whole panel down.
    function parseEntry(text) {
        try {
            const value = JSON.parse(text)
            return value && typeof value === "object" ? value : ({})
        } catch (error) {
            return ({})
        }
    }

    function entryName(call) {
        const name = call && call.name ? String(call.name) : ""
        return name.length > 0 ? name : qsTr("Unknown")
    }

    function entryDate(call) {
        const date = call && call.date ? Number(call.date) : 0
        return date > 0 ? BeyGramLang.formatDateTime(date) : ""
    }

    function entryDuration(call) {
        const label = call && call.durationLabel ? String(call.durationLabel) : ""
        return label
    }

    function entryId(call) {
        return call && call.id ? String(call.id) : ""
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Calls")
            description: qsTr("Calls made and received on this account, newest first.")

            trailing: DsBadge {
                visible: panel.missedCount > 0
                text: panel.missedCount === 1
                      ? qsTr("1 missed")
                      : qsTr("%1 missed").arg(panel.missedCount)
                tone: "danger"
                dot: true
            }

            content: DsCard {
                Layout.fillWidth: true
                padding: 0

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsEmptyState {
                        Layout.fillWidth: true
                        Layout.margins: Design.space4
                        visible: panel.empty
                        glyph: "\uD83D\uDCDE"
                        title: qsTr("No calls yet")
                        description: qsTr("Calls you make and receive will appear here.")
                    }

                    Repeater {
                        model: panel.entries

                        delegate: ColumnLayout {
                            id: callRow

                            required property int index
                            required property string modelData

                            readonly property var call: panel.parseEntry(callRow.modelData)
                            readonly property bool missed: callRow.call.missed === true
                            readonly property bool outgoing: callRow.call.outgoing === true
                            readonly property color ink: callRow.missed
                                                         ? BeyGramTheme.danger
                                                         : (callRow.outgoing
                                                            ? BeyGramTheme.accent
                                                            : BeyGramTheme.okInk)

                            Layout.fillWidth: true
                            spacing: 0

                            Item {
                                Layout.fillWidth: true
                                implicitHeight: panel.rowHeight

                                Accessible.role: Accessible.ListItem
                                Accessible.name: panel.entryName(callRow.call)
                                Accessible.description: {
                                    const parts = []
                                    if (callRow.missed)
                                        parts.push(qsTr("Missed"))
                                    else
                                        parts.push(callRow.outgoing ? qsTr("Outgoing")
                                                                    : qsTr("Incoming"))
                                    const date = panel.entryDate(callRow.call)
                                    if (date.length > 0)
                                        parts.push(date)
                                    const duration = panel.entryDuration(callRow.call)
                                    if (!callRow.missed && duration.length > 0)
                                        parts.push(duration)
                                    return parts.join(", ")
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Design.space3
                                    anchors.rightMargin: Design.space3
                                    spacing: Design.space3

                                    // Direction badge: an arrow turned the way
                                    // the call travelled, red when it was missed.
                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        implicitWidth: 34
                                        implicitHeight: 34
                                        radius: width / 2
                                        color: Qt.alpha(callRow.ink, callRow.missed ? 0.14 : 0.10)
                                        border.width: 1
                                        border.color: Qt.alpha(callRow.ink, 0.28)
                                        antialiasing: true

                                        Text {
                                            anchors.centerIn: parent
                                            text: callRow.outgoing ? "\u2197" : "\u2199"
                                            color: callRow.ink
                                            font.pixelSize: Design.fontBodyLarge
                                            font.weight: Font.DemiBold
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            Accessible.ignored: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: panel.entryName(callRow.call)
                                            color: callRow.missed ? BeyGramTheme.danger
                                                                  : BeyGramTheme.text
                                            font.pixelSize: Design.fontBody
                                            font.weight: Font.Medium
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            visible: text.length > 0
                                            text: panel.entryDate(callRow.call)
                                            color: BeyGramTheme.muted
                                            font.pixelSize: Design.fontSmall
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: text.length > 0
                                        text: callRow.missed
                                              ? qsTr("Missed")
                                              : panel.entryDuration(callRow.call)
                                        color: callRow.missed ? BeyGramTheme.danger
                                                              : BeyGramTheme.muted
                                        font.pixelSize: Design.fontSmall
                                        font.weight: callRow.missed ? Font.DemiBold
                                                                    : Font.Normal
                                    }

                                    DsIconButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        icon: "\uE717" // Segoe MDL2 Assets: Phone
                                        tooltip: qsTr("Call back")
                                        tone: "soft"
                                        size: "sm"
                                        enabled: panel.hasFacade
                                                 && panel.entryId(callRow.call).length > 0
                                        onClicked: {
                                            if (panel.facade)
                                                panel.facade.callBackRequested(
                                                    panel.entryId(callRow.call))
                                        }
                                    }
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: callRow.index < panel.entries.length - 1
                            }
                        }
                    }
                }
            }
        }
    }
}
