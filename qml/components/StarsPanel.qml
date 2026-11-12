// The Telegram Stars screen: balance header, package grid, buy button and the
// recent transaction list.
//
// Presentation only. Every value comes from `facade` (a StarsFacade) and every
// action goes back through it: selecting a row calls facade.select(index) and
// the buy button calls facade.buy(), which reports the choice through
// purchaseRequested. Nothing here touches TDLib, the network, or storage, and
// nothing decides what a star is worth.
//
// The facade is nullable so the panel can be created before the controller is
// wired up; until it is set, the panel renders the empty state.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    /// A StarsFacade, or null before the controller is available.
    property var facade: null

    /// Number of package chips per row.
    property int packageColumns: 3

    implicitWidth: 360
    implicitHeight: layout.implicitHeight

    readonly property var labels: root.facade ? root.facade.packageLabels : []
    readonly property var rows: root.facade ? root.facade.transactions : []
    readonly property bool hasSelection: root.facade ? root.facade.canBuy : false

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        // Balance header. The text is already formatted by the facade
        // ("1 234 ⭐"), so the panel never does number formatting itself.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: qsTr("Stars balance")
                color: BeyGramTheme.muted
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                text: root.facade ? root.facade.balanceText : ""
                color: BeyGramTheme.text
                font.pixelSize: 28
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }

        // Package grid: one chip per offered package, labelled with its star
        // amount and price ("500 ⭐ · 9.99 USD"). The selected chip is the one
        // the buy button will report.
        GridLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            columns: root.packageColumns
            columnSpacing: 8
            rowSpacing: 8

            Repeater {
                model: root.labels

                delegate: ChoiceChip {
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    text: modelData
                    selected: root.facade ? root.facade.selectedIndex === index : false
                    onClicked: {
                        if (root.facade)
                            root.facade.select(index)
                    }
                }
            }
        }

        PrimaryButton {
            Layout.fillWidth: true
            Layout.topMargin: 2
            tone: "accent"
            enabled: root.hasSelection
            text: root.facade && root.facade.priceText.length > 0
                  ? qsTr("Buy for %1").arg(root.facade.priceText)
                  : qsTr("Buy")
            onClicked: {
                if (root.facade)
                    root.facade.buy()
            }
        }

        // Transaction history, newest first, already formatted by the facade
        // ("+100 ⭐ · Stars purchase · 2024-05-01").
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 8
            text: qsTr("Recent transactions")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        Repeater {
            model: root.rows

            delegate: Text {
                required property string modelData

                Layout.fillWidth: true
                text: modelData
                color: BeyGramTheme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.rows.length === 0
            text: qsTr("No transactions yet.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
