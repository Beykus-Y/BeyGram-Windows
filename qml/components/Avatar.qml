import QtQuick
import BeyGram.Theme 1.0

/// Circular avatar — compatibility adapter over DsAvatar.
///
/// The public surface is unchanged: `fileId` / `ready` / `revision` still build
/// the media-provider URL, `initials` is the fallback, and `size` drives the
/// diameter. Rendering (circle mask, initials, presence dot) is delegated to
/// DsAvatar.
///
/// `tint` and `ink` stay for source compatibility. Callers that pass one of
/// DsAvatar's six palette colours (the `avatarColors` arrays in ChatRow,
/// ArchivePage and ForwardDialog) keep exactly the same colour, because the
/// matching palette index is recovered from `tint`. Any other tint falls back
/// to palette index 0, since DsAvatar owns the palette.
Item {
    id: root

    property int size: 46
    property int fileId: 0
    property bool ready: false
    property int revision: 0
    property string initials: "?"
    property color tint: BeyGramTheme.accentSoft
    property color ink: BeyGramTheme.accentInk
    // Explicit palette index; -1 derives it from `tint`.
    property int accentIndex: -1
    // Optional explicit source; wins over the fileId/ready/revision URL.
    property string imageSource: ""
    property bool online: false
    property bool muted: false

    implicitWidth: size
    implicitHeight: size

    // Media provider URL kept as before; DsAvatar adds its own circular mask.
    readonly property string _source: {
        if (root.imageSource.length > 0)
            return root.imageSource
        if (!root.ready || root.fileId <= 0)
            return ""
        return "image://beygrammedia/" + root.fileId + "?v=" + root.revision
                + "&round=1"
    }

    readonly property int _tintIndex: {
        for (var i = 0; i < avatar.avatarPalette.length; ++i) {
            if (Qt.colorEqual(root.tint, avatar.avatarPalette[i]))
                return i
        }
        return -1
    }

    readonly property int _resolvedAccentIndex: root.accentIndex >= 0
                                                ? root.accentIndex
                                                : (root._tintIndex >= 0 ? root._tintIndex : 0)

    DsAvatar {
        id: avatar
        anchors.fill: parent
        size: root.size
        initials: root.initials
        imageSource: root._source
        accentIndex: root._resolvedAccentIndex
        online: root.online
        muted: root.muted
    }
}
