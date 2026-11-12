import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// EnvironmentPage — runtime diagnostics, paths, TDLib, credentials and DPAPI storage.
Item {
    id: page

    property bool showHeader: true

    readonly property bool mediaAvailable: BeyGramApp.bgMediaTitle.indexOf("not") < 0

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
            const text = row.detail.length > 0 ? row.value + " — " + row.detail : row.value
            BeyGramApp.copyText(text)
        }

        leading: RowLayout {
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
            icon: "\uE8C8"
            size: "sm"
            tooltip: qsTr("Copy %1").arg(row.label.toLowerCase())
            onClicked: row.copyValue()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            visible: page.showHeader
            title: qsTr("Environment")
            subtitle: qsTr("Runtime paths, TDLib status, credentials and local storage.")

            trailing: DsIconButton {
                icon: "\uE8C8"
                tooltip: qsTr("Copy diagnostics")
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
                    title: qsTr("Application")
                    description: qsTr("The build and the folders it uses on this PC.")

                    trailing: Row {
                        spacing: Design.space2

                        DsButton {
                            text: qsTr("Open data folder")
                            tone: "secondary"
                            size: "sm"
                            onClicked: BeyGramApp.openDataFolder()
                        }

                        DsButton {
                            text: qsTr("Open log folder")
                            tone: "secondary"
                            size: "sm"
                            onClicked: BeyGramApp.openLogFolder()
                        }

                        DsButton {
                            text: qsTr("Copy diagnostics")
                            tone: "secondary"
                            size: "sm"
                            visible: !page.showHeader
                            onClicked: BeyGramApp.copyDiagnostics()
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
                                label: qsTr("Version")
                                value: qsTr("BeyGram Windows %1").arg(BeyGramApp.appVersion)
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Build")
                                value: BeyGramApp.buildInfo
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("TDLib path")
                                value: BeyGramApp.tdlibPath !== "" ? BeyGramApp.tdlibPath
                                                                   : qsTr("not loaded")
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Data directory")
                                value: BeyGramApp.dataDirectory
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Log directory")
                                value: BeyGramApp.logDirectory
                            }
                        }
                    }
                }

                DsSection {
                    Layout.fillWidth: true
                    title: qsTr("Telegram core")
                    description: qsTr("Connection, credentials and transport reported by the native session. Set BEYGRAM_PROXY (for example socks5://127.0.0.1:7897) before starting BeyGram; proxy credentials are never logged.")

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space1
                        spacing: 0

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            DiagRow {
                                label: qsTr("tdjson runtime")
                                value: BeyGramApp.tdlibAvailable ? qsTr("Loaded") : qsTr("Not available")
                                detail: BeyGramApp.tdlibDetail
                                valueColor: BeyGramApp.tdlibAvailable
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.danger
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Credentials")
                                value: BeyGramApp.credentialsConfigured ? qsTr("Present") : qsTr("Absent")
                                detail: BeyGramApp.credentialsConfigured
                                        ? qsTr("Source: %1").arg(BeyGramApp.credentialsSource)
                                        : BeyGramApp.credentialsHint
                                valueColor: BeyGramApp.credentialsConfigured
                                            ? BeyGramTheme.okInk
                                            : BeyGramTheme.warnInk
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Stage")
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
                                label: qsTr("Transport")
                                value: BeyGramApp.connectionTitle
                                valueColor: BeyGramApp.running
                                            ? BeyGramTheme.accentInk
                                            : BeyGramTheme.text
                            }

                            DsDivider { Layout.fillWidth: true; inset: Design.space3 }

                            DiagRow {
                                label: qsTr("Proxy")
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
                    title: qsTr("Session storage & DPAPI")
                    description: qsTr("The TDLib database is encrypted with a key generated on this PC and wrapped by Windows DPAPI. A lock file prevents a second BeyGram process from opening the same account.")
                }

                DsSection {
                    Layout.fillWidth: true
                    title: qsTr("Media core")
                    description: qsTr("Background media support available to this build.")

                    content: DsCard {
                        Layout.fillWidth: true
                        padding: Design.space4

                        content: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsBadge {
                                Layout.alignment: Qt.AlignLeft
                                text: page.mediaAvailable ? qsTr("Available") : qsTr("Unavailable")
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
