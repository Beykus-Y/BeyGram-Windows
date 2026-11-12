import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    property int tab: 0

    readonly property var tabNames: [
        "Profile", "Appearance", "Notifications", "Privacy", "Sessions",
        "Storage", "Network", "Shortcuts", "Statistics", "Premium"
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            text: "Tools"
            color: BeyGramTheme.text
            font.pixelSize: 26
            font.weight: Font.DemiBold
        }

        ScrollView {
            Layout.fillWidth: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            RowLayout {
                spacing: 6
                Repeater {
                    model: page.tabNames
                    delegate: SecondaryButton {
                        text: (page.tab === index ? "• " : "") + modelData
                        onClicked: page.tab = index
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: page.tab

            ProfileEditor { facade: BeyGramApp.profileEditor }
            AppearancePanel { facade: BeyGramApp.appearance }
            NotificationSettings { facade: BeyGramApp.notificationSettings }
            PrivacyPanel { facade: BeyGramApp.privacy }
            SessionsPanel { facade: BeyGramApp.sessions }
            StoragePanel { facade: BeyGramApp.storage }
            NetworkPanel { facade: BeyGramApp.network }
            ShortcutsPanel { facade: BeyGramApp.shortcuts }
            StatisticsPanel { facade: BeyGramApp.statistics }
            PremiumCard { facade: BeyGramApp.premium }
        }
    }
}
