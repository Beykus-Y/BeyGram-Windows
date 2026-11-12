import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// EnvironmentPage — what this executable actually knows about its runtime.
//
// Every diagnostic the page used to show is still here, now as key/value rows
// inside DsSection groups. Long values wrap to two lines and every row carries
// an inline DsIconButton, so a path can be taken away without selecting text
// by hand.
Item {
    id: page

    readonly property bool mediaAvailable: BeyGramApp.bgMediaTitle.indexOf("not") < 0

    // One key/value diagnostic row. DsListRow owns the surface, padding and
    // height; the label and the mono value live in its leading slot so they
    // can use the diagnostics type scale instead of the chat-row scale.
    component DiagRow: DsListRow {
        id: row

        property string label: ""
        property string value: ""
        property string detail: ""
        property color valueColor: BeyGramTheme.text

        readonly property int rowHeight: row.detail.length > 0
                                         ? Design.rowHeight + Design.space5
                                         : Design.rowHeight

        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredHeight: row.rowHeight
        implicitHeight: row.rowHeight
        height: row.rowHeight

        hoverEnabled: false
        title: ""
        subtitle: ""

        Accessible.name: row.label + ": " + row.value
        Accessible.description: row.detail

        function copyValue() {
            if (row.value.length === 0)
                return
            var text = row.detail.length > 0 ? row.value + " — " + row.detail : row.value
            clipboard.text = text
            clipboard.selectAll()
            clipboard.copy()
        }

        // TextEdit is the clipboard bridge; it stays invisible and never takes
        // part in the row layout.
        TextEdit {
            id: clipboard
            visible: false
            width: 1
            height: 1
            readOnly: true
            textFormat: TextEdit.PlainText
        }

        leading: RowLayout {
            // 12 px side margins, two 12 px gaps around the empty title column
            // and the 28 px trailing copy button.
            width: Math.max(0, row.width - Design.space3 * 4 - 28)
            spacing: Design.space3

            Text {
                Layout.preferredWidth: 128
                Layout.maximumWidth: 128
                Layout.alignment: Qt.AlignVCenter
                text: row.label
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                elide: Text.ElideRight
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: Design.space1

                Text {
                    Layout.fillWidth: true
                    text: row.value
                    color: row.valueColor
                    font.pixelSize: Design.fontSmall
                    font.family: "Consolas"
                    textFormat: Text.PlainText
                    lineHeight: Design.lineNormal
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: row.detail
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    textFormat: Text.PlainText
                    lineHeight: Design.lineNormal
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }

        trailing: DsIconButton {
            icon: "\uE8C8" // Segoe MDL2 Assets: Copy
            size: "sm"
            tooltip: "Copy " + row.label.toLowerCase()
            onClicked: row.copyValue()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Environment"
            subtitle: "What this executable actually knows about its runtime."

            trailing: DsIconButton {
                icon: "\uE8C8" // Segoe MDL2 Assets: Copy
                tooltip: "Copy diagnostics"
                onClicked: BeyGramApp.copyDiagnostics()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.min(920, Math.max(320, page.width - Design.space8 * 2))
                x: Math.max(Design.space8, (page.width - width) / 2)
                y: Design.space6
                spacing: Design.space6

                DsSection {
                    Layout.fillWidth: true
                    title: "Application"
                    description: "The build and the folders it uses on this PC."

                    trailing: Row {
                        spacing: Design.space2

                        DsButton {
                            text: "Open data folder"
                            tone: "secondary"
                            size: "sm"
                            onClicked: BeyGramApp.openDataFolder()
                        }

                        DsButton {
                            text: "Open log folder"
                            tone: "secondary"
                            size: "sm"
                            onClicked: BeyGramApp.openLogFolder()
                        }
                    }

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space1
                        spacing: 0

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DiagRow {
                                label: "Version"
                                value: "BeyGram Windows " + BeyGramApp.appVersion
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Build"
                                value: BeyGramApp.buildInfo
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "TDLib path"
                                value: BeyGramApp.tdlibPath !== "" ? BeyGramApp.tdlibPath
                                                                   : "not loaded"
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Data directory"
                                value: BeyGramApp.dataDirectory
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Log directory"
                                value: BeyGramApp.logDirectory
                            }
                        }
                    }
                }

                DsSection {
                    Layout.fillWidth: true
                    title: "Telegram core"
                    description: "Connection, credentials and transport as reported by the native "
                                 + "session. Set BEYGRAM_PROXY (for example "
                                 + "socks5://127.0.0.1:7897) before starting BeyGram; proxy "
                                 + "credentials are never logged."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space1
                        spacing: 0

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DiagRow {
                                label: "tdjson runtime"
                                value: BeyGramApp.tdlibAvailable ? "Loaded" : "Not available"
                                detail: BeyGramApp.tdlibDetail
                                valueColor: BeyGramApp.tdlibAvailable
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.danger
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Credentials"
                                value: BeyGramApp.credentialsConfigured ? "Present" : "Absent"
                                detail: BeyGramApp.credentialsConfigured
                                        ? "Read from BEYGRAM_TELEGRAM_API_ID / "
                                          + "BEYGRAM_TELEGRAM_API_HASH."
                                        : BeyGramApp.credentialsHint
                                valueColor: BeyGramApp.credentialsConfigured
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.warnInk
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Stage"
                                value: BeyGramApp.stageName
                                detail: BeyGramApp.stageDetail !== ""
                                        ? BeyGramApp.stageTitle + " — " + BeyGramApp.stageDetail
                                        : BeyGramApp.stageTitle
                                valueColor: BeyGramApp.ready
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.text
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Transport"
                                value: BeyGramApp.connectionTitle
                                valueColor: BeyGramApp.running
                                            ? BeyGramTheme.accentInk
                                            : BeyGramTheme.text
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: "Proxy"
                                value: BeyGramApp.proxySummary
                                valueColor: BeyGramApp.proxySummary === "Direct connection"
                                            ? BeyGramTheme.text
                                            : BeyGramTheme.okInk
                            }
                        }
                    }
                }

                DsSection {
                    Layout.fillWidth: true
                    title: "Media core"
                    description: "Background media support available to this build."

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space4

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsBadge {
                                Layout.alignment: Qt.AlignLeft
                                text: page.mediaAvailable ? "Available" : "Unavailable"
                                tone: page.mediaAvailable ? "ok" : "danger"
                                dot: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.bgMediaTitle
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontBody
                                font.weight: Font.DemiBold
                                textFormat: Text.PlainText
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                text: BeyGramApp.bgMediaDetail
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                textFormat: Text.PlainText
                                lineHeight: Design.lineNormal
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: Design.space2 }
            }
        }
    }
}
