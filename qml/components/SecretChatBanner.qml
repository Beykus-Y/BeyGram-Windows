// SecretChatBanner — compact strip shown above the message list of a secret
// chat. It reads the chat's self-destruct timer from `facade` and lets the user
// pick a new one; the pick is handed back to the facade, which stores it and
// emits the request. This component is presentation only: it owns no timer, no
// transport and no state that outlives the binding.
//
// `facade` is a SecurityFacade (or null). The banner hides itself when no secret
// chat is loaded, so a parent can keep it in the layout unconditionally.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: banner

    property var facade: null

    readonly property bool secret: banner.facade ? banner.facade.secret : false
    readonly property int ttlSeconds: banner.facade ? banner.facade.ttlSeconds : 0
    readonly property string ttlText: banner.facade ? banner.facade.ttlText : qsTr("Off")

    // Height is fixed; `visible` is Item's own property (default overridden
    // below) so a parent can place the banner without checking the chat first.
    height: visible ? 44 : 0
    implicitHeight: 44
    visible: banner.secret
    clip: true

    // Picks a timer through the facade. The menu item carries the seconds; the
    // facade clamps and announces them.
    function chooseTtl(seconds) {
        if (banner.facade)
            banner.facade.setTtl(seconds)
    }

    // Menu items stay plain text: the check is derived from the facade, so the
    // menu never toggles a local `checked` that would drift from the chat.
    function optionText(seconds, label) {
        return (banner.ttlSeconds === seconds ? "\u2713  " : "     ") + label
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 0
        color: BeyGramTheme.surface2
        border.width: 0

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: BeyGramTheme.line
        }
    }

    // Left accent bar, matching the other chat banners.
    Rectangle {
        id: accentBar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        radius: 0
        color: BeyGramTheme.accentInk
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 9

        Text {
            id: secretGlyph
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("SECRET")
            color: BeyGramTheme.accentInk
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }

        Text {
            id: ttlLabel
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Self-destruct: %1").arg(banner.ttlText)
            color: BeyGramTheme.text
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }

        ToolButton {
            id: ttlButton
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 46
            implicitHeight: 26
            padding: 0
            focusPolicy: Qt.NoFocus
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Self-destruct timer")
            Accessible.description: qsTr("Current timer: %1").arg(banner.ttlText)
            onClicked: ttlMenu.open()

            background: Rectangle {
                radius: 13
                color: ttlButton.hovered ? Qt.alpha(BeyGramTheme.accentInk, 0.12)
                                         : "transparent"
                border.width: 1
                border.color: BeyGramTheme.line
            }

            contentItem: Text {
                text: qsTr("TTL")
                color: ttlButton.hovered ? BeyGramTheme.text : BeyGramTheme.muted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            // Declared inside the button so the popup centres on it. The menu
            // closes itself after a pick; the facade decides what the value
            // means for the loaded chat.
            Menu {
                id: ttlMenu

                MenuItem {
                    text: banner.optionText(0, qsTr("Off"))
                    onTriggered: banner.chooseTtl(0)
                }

                MenuItem {
                    text: banner.optionText(300, qsTr("5 minutes"))
                    onTriggered: banner.chooseTtl(300)
                }

                MenuItem {
                    text: banner.optionText(3600, qsTr("1 hour"))
                    onTriggered: banner.chooseTtl(3600)
                }

                MenuItem {
                    text: banner.optionText(86400, qsTr("1 day"))
                    onTriggered: banner.chooseTtl(86400)
                }

                MenuItem {
                    text: banner.optionText(604800, qsTr("1 week"))
                    onTriggered: banner.chooseTtl(604800)
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Change self-destruct timer")
            ToolTip.delay: 600
        }
    }
}
