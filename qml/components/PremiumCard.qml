import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

/// Telegram Premium summary card.
///
/// Presentational: every string comes from the `facade` (PremiumFacade), so the
/// card never reads the clock, formats a countdown or guesses a status. The
/// facade is a `var` the page may still be filling in, so each binding goes
/// through `ready` and the card renders its empty state until one is attached.
///
/// The Upgrade button only calls `facade.upgrade()`, which announces the intent
/// through `upgradeRequested()`; opening the purchase flow is the page's job.
Card {
    id: root

    property var facade: null

    readonly property bool ready: facade !== null && facade !== undefined
    readonly property bool premium: ready ? facade.premium : false
    readonly property bool loading: ready ? facade.loading : false
    readonly property string badge: ready ? facade.badge : ""
    readonly property string expiry: ready ? facade.expiryText : ""
    readonly property var featureList: ready ? facade.features : []

    implicitWidth: 320
    spacing: 12

    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Telegram Premium")

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Telegram Premium")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        // Hidden for a non-Premium account: the facade's badge is empty then,
        // so the card never shows a "Premium" label it cannot back up.
        Pill {
            visible: root.badge.length > 0
            text: root.badge
            tone: "accent"
        }
    }

    Text {
        Layout.fillWidth: true
        visible: root.expiry.length > 0
        text: root.expiry
        color: root.premium ? BeyGramTheme.accent : BeyGramTheme.muted
        font.pixelSize: 12
        font.weight: Font.Medium
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        visible: !root.premium && !root.loading
        text: qsTr("No Premium subscription on this account yet.")
        color: BeyGramTheme.muted
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        lineHeight: 1.35
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        // The facade may still be waiting for the feature list; an empty list
        // leaves the card without a stray heading.
        visible: root.featureList.length > 0

        Repeater {
            model: root.featureList

            delegate: RowLayout {
                id: featureRow

                required property var modelData

                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 5
                    width: 6
                    height: 6
                    radius: 3
                    color: BeyGramTheme.accent
                }

                Text {
                    Layout.fillWidth: true
                    text: String(featureRow.modelData)
                    color: BeyGramTheme.text
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: root.loading
        text: qsTr("Loading Premium details…")
        color: BeyGramTheme.muted
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    PrimaryButton {
        Layout.fillWidth: true
        text: qsTr("Upgrade")
        // A click while the status is still loading would only race the answer.
        enabled: root.ready && !root.loading
        onClicked: {
            if (root.ready)
                root.facade.upgrade()
        }
    }
}
