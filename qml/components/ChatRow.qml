import QtQuick
import BeyGram.App 1.0
import "../ds/design.js" as Design

/// Conversation row for the chat list.
///
/// The row is drawn by DsChatRow; this component owns the application-facing
/// API: the chat id used for navigation, the raw last-message timestamp that
/// gets formatted here, and the avatar photo request against BeyGramApp.
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
    property bool online: false

    signal clicked()

    implicitWidth: 320
    implicitHeight: Design.rowHeight

    /// Same provider URL the previous Avatar built: the media provider is asked
    /// for a round bitmap and the revision busts the image cache.
    readonly property string photoSource: row.photoReady && row.photoFileId > 0
                                          ? "image://beygrammedia/" + row.photoFileId
                                            + "?v=" + row.photoRevision + "&round=1"
                                          : ""

    /// Kept for API compatibility; DsAvatar owns the rendering palette now.
    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    function requestAvatar() {
        if (photoFileId > 0)
            BeyGramApp.requestChatPhoto(photoFileId)
    }

    Component.onCompleted: requestAvatar()
    onPhotoFileIdChanged: requestAvatar()

    /// Today shows a clock time, older chats a day/month stamp.
    function timeText() {
        if (!lastDate || lastDate <= 0)
            return ""
        const date = new Date(lastDate * 1000)
        const now = new Date()
        if (date.toDateString() === now.toDateString())
            return Qt.formatTime(date, "hh:mm")
        return Qt.formatDate(date, "dd.MM")
    }

    DsChatRow {
        anchors.fill: parent

        title: row.title
        preview: row.preview
        timeText: row.timeText()
        unread: row.unreadCount
        pinned: row.pinned
        muted: row.muted
        online: row.online
        initials: row.initials
        accentIndex: row.accentIndex
        imageSource: row.photoSource
        selected: row.selected

        onClicked: row.clicked()
    }
}
