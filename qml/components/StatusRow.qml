import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: row
    property string label: ""
    property string value: ""
    property string detail: ""
    property string tone: "neutral" // neutral | ok | warn | danger | accent

    implicitHeight: inner.implicitHeight

    ColumnLayout {
        id: inner
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 3

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: {
                    if (row.tone === "ok") return BeyGramTheme.okInk
                    if (row.tone === "warn") return BeyGramTheme.warnInk
                    if (row.tone === "danger") return BeyGramTheme.danger
                    if (row.tone === "accent") return BeyGramTheme.accent
                    return BeyGramTheme.muted
                }
            }

            Text {
                text: row.label
                color: BeyGramTheme.text
                font.pixelSize: 13
                font.weight: Font.Medium
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Text {
                text: row.value
                color: {
                    if (row.tone === "ok") return BeyGramTheme.okInk
                    if (row.tone === "warn") return BeyGramTheme.warnInk
                    if (row.tone === "danger") return BeyGramTheme.danger
                    if (row.tone === "accent") return BeyGramTheme.accent
                    return BeyGramTheme.muted
                }
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            visible: row.detail !== ""
            text: row.detail
            color: BeyGramTheme.muted
            font.pixelSize: 12
            lineHeight: 1.25
            wrapMode: Text.WordWrap
        }
    }
}
