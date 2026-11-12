import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// ToolsPage — ten account utilities behind one rail.
//
// The page keeps the public `tab` property and the ordered panel list. The
// ad-hoc button strip is replaced by a vertical rail inside a card, and the
// active panel is hosted in an elevated card with its own scroll area.
Item {
    id: page

    property int tab: 0

    readonly property var tabNames: [
        "Profile", "Appearance", "Notifications", "Privacy", "Sessions",
        "Storage", "Network", "Shortcuts", "Statistics", "Premium"
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            title: "Tools"
            subtitle: "Account utilities, diagnostics and preferences, one panel at a time."
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Design.space5
            spacing: Design.space4

            // --- Rail ----------------------------------------------------
            DsCard {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                padding: Design.space2

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space1

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Design.space2
                        Layout.topMargin: Design.space1
                        Layout.bottomMargin: Design.space1
                        text: "Panels"
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.3
                    }

                    Repeater {
                        model: page.tabNames

                        delegate: Rectangle {
                            id: railRow

                            required property int index
                            required property var modelData

                            readonly property bool current: page.tab === railRow.index

                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: Design.radiusSm
                            color: railRow.current
                                   ? BeyGramTheme.accentSoft
                                   : (railMouse.containsMouse ? BeyGramTheme.surface2 : "transparent")

                            Accessible.role: Accessible.PageTab
                            Accessible.name: railRow.modelData
                            Accessible.checked: railRow.current

                            Behavior on color {
                                ColorAnimation {
                                    duration: Design.motionFast
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: Design.space3
                                anchors.rightMargin: Design.space3
                                anchors.verticalCenter: parent.verticalCenter
                                text: railRow.modelData
                                color: railRow.current ? BeyGramTheme.accentInk
                                                       : BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                font.weight: railRow.current ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight

                                Behavior on color {
                                    ColorAnimation {
                                        duration: Design.motionFast
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }

                            MouseArea {
                                id: railMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: page.tab = railRow.index
                            }
                        }
                    }
                }
            }

            // --- Active panel --------------------------------------------
            DsCard {
                id: panelHost

                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: Design.space5
                elevated: true

                ScrollView {
                    id: panelScroll

                    anchors.fill: parent
                    anchors.margins: panelHost.padding
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: panelColumn.implicitHeight
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        id: panelColumn

                        width: panelScroll.availableWidth
                        spacing: Design.space4

                        ProfileEditor {
                            Layout.fillWidth: true
                            visible: page.tab === 0
                            facade: BeyGramApp.profileEditor
                        }

                        AppearancePanel {
                            Layout.fillWidth: true
                            visible: page.tab === 1
                            facade: BeyGramApp.appearance
                        }

                        NotificationSettings {
                            Layout.fillWidth: true
                            visible: page.tab === 2
                            facade: BeyGramApp.notificationSettings
                        }

                        PrivacyPanel {
                            Layout.fillWidth: true
                            visible: page.tab === 3
                            facade: BeyGramApp.privacy
                        }

                        SessionsPanel {
                            Layout.fillWidth: true
                            visible: page.tab === 4
                            facade: BeyGramApp.sessions
                        }

                        StoragePanel {
                            Layout.fillWidth: true
                            visible: page.tab === 5
                            facade: BeyGramApp.storage
                        }

                        NetworkPanel {
                            Layout.fillWidth: true
                            visible: page.tab === 6
                            facade: BeyGramApp.network
                        }

                        ShortcutsPanel {
                            Layout.fillWidth: true
                            visible: page.tab === 7
                            facade: BeyGramApp.shortcuts
                        }

                        StatisticsPanel {
                            Layout.fillWidth: true
                            visible: page.tab === 8
                            facade: BeyGramApp.statistics
                        }

                        PremiumCard {
                            Layout.fillWidth: true
                            visible: page.tab === 9
                            facade: BeyGramApp.premium
                        }
                    }
                }
            }
        }
    }
}
