pragma ComponentBehavior: Bound

// ChatFolderTabs.qml — horizontal folder tab strip for the chat list.
//
// Consumes ChatFolderListModel (roles: folderId, title, icon, unreadCount, isMain)
// or a plain array of folder objects, renders DesignStyle-aware pill/tab chips
// with per-folder unread badges, and supports horizontal mouse-wheel + drag scroll.
import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    property var model: null
    property var currentFolderId: 0

    signal folderSelected(var folderId)

    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
    readonly property bool _isMaterial2: BeyGramAppearance.designStyle === "Material2"
    readonly property bool _isMaterial3: BeyGramAppearance.designStyle === "Material3"
    readonly property bool _isSignature: BeyGramAppearance.designStyle === "Signature"
    readonly property bool _amoled: BeyGramTheme.modeName === "Amoled"

    readonly property int _count: {
        if (!root.model)
            return 0
        if (root.model.count !== undefined)
            return Number(root.model.count)
        if (root.model.length !== undefined)
            return Number(root.model.length)
        return 0
    }

    visible: root._count > 1
    implicitWidth: tabsRow.implicitWidth
    implicitHeight: visible ? (Design.controlHeightSmall + 6) : 0

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: tabsRow.implicitWidth
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width
        clip: true

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                if (flick.contentWidth <= flick.width)
                    return
                var delta = Math.abs(event.pixelDelta.x) > 0
                            ? event.pixelDelta.x
                            : (event.angleDelta.x !== 0 ? event.angleDelta.x / 2 : event.angleDelta.y / 2)
                var maxX = Math.max(0, flick.contentWidth - flick.width)
                flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - delta))
                event.accepted = true
            }
        }

        Row {
            id: tabsRow
            height: flick.height
            spacing: Design.space1 + 2

            Repeater {
                id: repeater
                model: root.model

                delegate: Item {
                    id: tabItem

                    required property int index
                    required property var folderId
                    required property string title
                    required property int unreadCount

                    readonly property bool selected: Number(root.currentFolderId) === Number(tabItem.folderId)
                    readonly property bool hovered: tabMouse.containsMouse
                    readonly property int tabRadius: root._isMaterial2
                                                     ? Design.radiusXs
                                                     : (root._isMaterial3 ? Design.radiusPill : Design.radiusMd)

                    width: pill.implicitWidth
                    height: flick.height

                    Rectangle {
                        id: pill
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: pillRow.implicitWidth + Design.space3 * 2
                        height: Design.controlHeightSmall
                        radius: tabItem.tabRadius
                        color: {
                            if (root._isMaterial2)
                                return tabItem.hovered ? BeyGramTheme.surface2 : "transparent"
                            if (tabItem.selected)
                                return BeyGramTheme.accentSoft
                            if (tabItem.hovered)
                                return Qt.alpha(BeyGramTheme.surface2, 0.9)
                            return root._amoled ? Qt.alpha(BeyGramTheme.surface2, 0.45) : "transparent"
                        }
                        border.width: {
                            if (root._isMaterial2)
                                return 0
                            if (tabItem.selected && (root._amoled || root._isSignature))
                                return 1
                            if (!tabItem.selected && root._amoled)
                                return 1
                            return 0
                        }
                        border.color: tabItem.selected
                                      ? Qt.alpha(BeyGramTheme.accent, 0.35)
                                      : BeyGramTheme.line
                        antialiasing: true

                        Behavior on color {
                            ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                        }

                        RowLayout {
                            id: pillRow
                            anchors.centerIn: parent
                            spacing: Design.space1 + 2

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                text: tabItem.title.length > 0 ? tabItem.title : qsTr("All chats")
                                color: tabItem.selected ? BeyGramTheme.accentInk : BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                font.weight: tabItem.selected ? Font.DemiBold : Font.Medium
                                elide: Text.ElideRight

                                Behavior on color {
                                    ColorAnimation { duration: root._animFast; easing.type: Easing.OutCubic }
                                }
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                visible: tabItem.unreadCount > 0
                                implicitWidth: Math.max(16, badgeText.implicitWidth + 8)
                                implicitHeight: 16
                                radius: height / 2
                                color: tabItem.selected ? BeyGramTheme.accent : BeyGramTheme.surface2
                                border.width: tabItem.selected ? 0 : 1
                                border.color: BeyGramTheme.line
                                antialiasing: true

                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: tabItem.unreadCount > 99 ? "99+" : String(tabItem.unreadCount)
                                    color: tabItem.selected ? BeyGramTheme.onAccent : BeyGramTheme.muted
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        // Material 2 / underline indicator
                        Rectangle {
                            visible: root._isMaterial2 && tabItem.selected
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: Design.space2
                            anchors.rightMargin: Design.space2
                            height: 2
                            radius: 1
                            color: BeyGramTheme.accent
                        }
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: pill
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.folderSelected(tabItem.folderId)
                    }

                    Accessible.role: Accessible.PageTab
                    Accessible.name: tabItem.title
                    Accessible.checked: tabItem.selected
                }
            }
        }
    }
}
