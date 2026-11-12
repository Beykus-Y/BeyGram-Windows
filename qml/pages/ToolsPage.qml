import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "../ds/design.js" as Design

// ToolsPage — ten account utility panels behind one rail.
Item {
    id: page

    property bool showHeader: true
    property int tab: 0

    readonly property var tabNames: [
        qsTr("Profile"),
        qsTr("Appearance"),
        qsTr("Notifications"),
        qsTr("Privacy"),
        qsTr("Sessions"),
        qsTr("Storage"),
        qsTr("Network"),
        qsTr("Shortcuts"),
        qsTr("Statistics"),
        qsTr("Premium")
    ]

    readonly property bool isPreviewOnlyTab: page.tab >= 2

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        DsHeader {
            Layout.fillWidth: true
            visible: page.showHeader
            title: qsTr("Tools")
            subtitle: qsTr("Account utilities, diagnostics and preferences, one panel at a time.")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Design.space5
            spacing: Design.space4

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
                        text: qsTr("Panels")
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

                        Rectangle {
                            Layout.fillWidth: true
                            visible: page.isPreviewOnlyTab
                            implicitHeight: previewNote.implicitHeight + Design.space3 * 2
                            radius: Design.radiusMd
                            color: Qt.alpha(BeyGramTheme.warnInk, 0.10)
                            border.width: 1
                            border.color: Qt.alpha(BeyGramTheme.warnInk, 0.28)

                            RowLayout {
                                id: previewNote
                                anchors.fill: parent
                                anchors.margins: Design.space3
                                spacing: Design.space2

                                DsBadge {
                                    text: qsTr("Preview")
                                    tone: "muted"
                                    dot: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Local preview / Backend sync in progress for this facade panel.")
                                    color: BeyGramTheme.muted
                                    font.pixelSize: Design.fontSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

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
