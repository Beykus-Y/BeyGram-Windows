import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    component FieldLabel: Text {
        color: BeyGramTheme.muted
        font.pixelSize: 12
    }
    component FieldValue: Text {
        color: BeyGramTheme.text
        font.pixelSize: 12
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(560, page.width - 64)
            x: 32
            y: 28
            spacing: 18

            Text {
                text: "Environment"
                color: BeyGramTheme.text
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }
            Text {
                text: "What this executable actually knows about its runtime."
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }

            Card {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Application"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 24
                    rowSpacing: 9

                    FieldLabel { text: "Version" }
                    FieldValue { text: "BeyGram Windows " + BeyGramApp.appVersion }
                    FieldLabel { text: "Build" }
                    FieldValue { text: BeyGramApp.buildInfo }
                    FieldLabel { text: "TDLib path" }
                    FieldValue { text: BeyGramApp.tdlibPath !== "" ? BeyGramApp.tdlibPath : "not loaded" }
                    FieldLabel { text: "Data directory" }
                    FieldValue { text: BeyGramApp.dataDirectory }
                    FieldLabel { text: "Log directory" }
                    FieldValue { text: BeyGramApp.logDirectory }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    SecondaryButton {
                        text: "Open data folder"
                        onClicked: BeyGramApp.openDataFolder()
                    }
                    SecondaryButton {
                        text: "Open log folder"
                        onClicked: BeyGramApp.openLogFolder()
                    }
                    SecondaryButton {
                        text: "Copy diagnostics"
                        onClicked: BeyGramApp.copyDiagnostics()
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Telegram core"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                StatusRow {
                    Layout.fillWidth: true
                    label: "tdjson runtime"
                    value: BeyGramApp.tdlibAvailable ? "Loaded" : "Not available"
                    tone: BeyGramApp.tdlibAvailable ? "ok" : "danger"
                    detail: BeyGramApp.tdlibDetail
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Credentials"
                    value: BeyGramApp.credentialsConfigured ? "Present" : "Absent"
                    tone: BeyGramApp.credentialsConfigured ? "ok" : "warn"
                    detail: BeyGramApp.credentialsConfigured
                            ? "Read from BEYGRAM_TELEGRAM_API_ID / BEYGRAM_TELEGRAM_API_HASH."
                            : BeyGramApp.credentialsHint
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Stage"
                    value: BeyGramApp.stageName
                    tone: BeyGramApp.ready ? "ok" : "neutral"
                    detail: BeyGramApp.stageDetail !== ""
                            ? BeyGramApp.stageTitle + " — " + BeyGramApp.stageDetail
                            : BeyGramApp.stageTitle
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Transport"
                    value: BeyGramApp.connectionTitle
                    tone: BeyGramApp.running ? "accent" : "neutral"
                }
                StatusRow {
                    Layout.fillWidth: true
                    label: "Proxy"
                    value: BeyGramApp.proxySummary
                    tone: BeyGramApp.proxySummary === "Direct connection" ? "neutral" : "ok"
                    detail: "Set BEYGRAM_PROXY (for example socks5://127.0.0.1:7897) before starting BeyGram. Proxy credentials are never logged."
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    text: "Media core"
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: BeyGramApp.bgMediaTitle
                    color: BeyGramApp.bgMediaTitle.indexOf("not") >= 0 ? BeyGramTheme.warnInk : BeyGramTheme.okInk
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Text {
                    text: BeyGramApp.bgMediaDetail
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                    Layout.fillWidth: true
                }
            }

            Item { Layout.fillWidth: true; height: 8 }
        }
    }
}
