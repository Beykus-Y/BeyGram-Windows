import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: row
    property int chatId: 0
    property string title: ""
    property string preview: ""
    property int unreadCount: 0
    property bool pinned: false
    property bool muted: false
    property string initials: "?"
    property int accentIndex: 0
    property var lastDate: 0
    property bool selected: false
    signal clicked()

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
        radius: 12
        color: row.selected
               ? Qt.alpha(BeyGramTheme.accentSoft, 0.95)
               : (mouse.containsMouse ? Qt.alpha(BeyGramTheme.surface2, 0.55) : "transparent")
        border.width: row.selected ? 1 : 0
        border.color: Qt.alpha(BeyGramTheme.accent, 0.22)
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 11

        Rectangle {
            width: 46
            height: 46
            radius: 23
            color: row.avatarColors[Math.abs(row.accentIndex) % row.avatarColors.length]
            Text {
                anchors.centerIn: parent
                text: row.initials
                color: "white"
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
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
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    visible: row.pinned
                    text: "PIN"
                    color: BeyGramTheme.accentInk
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                }
            }

            Text {
                Layout.fillWidth: true
                text: row.preview
                color: BeyGramTheme.muted
                font.pixelSize: 11
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
                font.pixelSize: 9
            }
            Rectangle {
                Layout.alignment: Qt.AlignRight
                visible: row.unreadCount > 0
                width: Math.max(18, badgeText.implicitWidth + 9)
                height: 18
                radius: 9
                color: BeyGramTheme.accent
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: row.unreadCount > 99 ? "99+" : row.unreadCount
                    color: BeyGramTheme.onAccent
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                }
            }
            Text {
                Layout.alignment: Qt.AlignRight
                visible: row.muted && row.unreadCount > 0
                text: "muted"
                color: BeyGramTheme.muted
                font.pixelSize: 8
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
