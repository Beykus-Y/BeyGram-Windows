pragma ComponentBehavior: Bound

import QtQuick
import BeyGram.App 1.0
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

/// Conversation row for the chat list.
///
/// The row is drawn by DsChatRow; this component owns the application-facing
/// API: the chat id used for navigation, localized timestamp formatting via
/// BeyGramLang.formatChatTimestamp(), and avatar photo requests against BeyGramApp.
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

    readonly property bool compact: BeyGramAppearance.listDensity === "Compact"

    implicitWidth: 320
    implicitHeight: compact ? Design.rowHeightCompact : Design.rowHeight
    height: implicitHeight

    // Note: DsAvatar applies its own circular MultiEffect mask.
    readonly property string photoSource: row.photoReady && row.photoFileId > 0
                                          ? "image://beygrammedia/" + row.photoFileId
                                            + "?v=" + row.photoRevision
                                          : ""

    /// Kept for API compatibility; DsAvatar owns the rendering palette.
    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    function requestAvatar() {
        if (row.photoFileId > 0)
            BeyGramApp.requestChatPhoto(row.photoFileId)
    }

    Component.onCompleted: row.requestAvatar()
    onPhotoFileIdChanged: row.requestAvatar()

    function timeText() {
        var _rev = BeyGramLang.revision
        var ts = Number(row.lastDate)
        if (!ts || ts <= 0)
            return ""
        return BeyGramLang.formatChatTimestamp(ts)
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
