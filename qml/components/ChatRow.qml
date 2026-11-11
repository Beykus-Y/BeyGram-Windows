import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: row
    property var chatId: 0
    property string title: ""
    property string preview: ""
    property int unreadCount: 0
    property bool pinned: false
    property bool muted: false
    property string initials: "?"
    property int accentIndex: 0
    property var lastDate: 0
    property bool selected: false
    property int photoFileId: 0
    property bool photoReady: false
    property int photoRevision: 0
    signal clicked()

    function requestAvatar() {
        if (photoFileId > 0)
            BeyGramApp.requestChatPhoto(photoFileId)
    }

    Component.onCompleted: requestAvatar()
    onPhotoFileIdChanged: requestAvatar()

    implicitHeight: 74

    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    function timeText() {
        if (!lastDate || lastDate <= 0)
            return ""
        const date = new Date(lastDate * 1000)
        const now = new Date()
        if (date.toDateString() === now.toDateString())
            return Qt.formatTime(date, "hh:mm")
        return Qt.formatDate(date, "dd.MM")
    }

    Rectangle {
        anchors.fill: parent
        radius: 0
        color: row.selected
               ? BeyGramTheme.accentSoft
               : (mouse.containsMouse ? BeyGramTheme.surface2 : "transparent")
        border.width: 0
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 14
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 11

        Avatar {
            size: 54
            fileId: row.photoFileId
            ready: row.photoReady
            revision: row.photoRevision
            initials: row.initials
            tint: row.avatarColors[Math.abs(row.accentIndex) % row.avatarColors.length]
            ink: "white"
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    Layout.fillWidth: true
                    text: row.title
                    color: BeyGramTheme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.fillWidth: true
                text: row.preview
                color: BeyGramTheme.muted
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            spacing: 4
            Layout.alignment: Qt.AlignTop
            Text {
                Layout.alignment: Qt.AlignRight
                text: row.timeText()
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }
            Rectangle {
                Layout.alignment: Qt.AlignRight
                visible: row.unreadCount > 0
                width: Math.max(20, badgeText.implicitWidth + 10)
                height: 20
                radius: 10
                color: BeyGramTheme.accent
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: row.unreadCount > 99 ? "99+" : row.unreadCount
                    color: BeyGramTheme.onAccent
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: row.clicked()
    }
}
