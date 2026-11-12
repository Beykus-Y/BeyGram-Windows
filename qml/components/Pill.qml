import QtQuick

// Pill — compatibility adapter over DsBadge.
//
// The legacy `tone` vocabulary (neutral | ok | warn | danger | accent) maps
// onto DsBadge's four tones. DsBadge has no warn tone, so `warn` uses the
// accent tone, which reads as the attention colour in both presets.
Item {
    id: root

    property string text: ""
    property string tone: "neutral" // neutral | ok | warn | danger | accent

    readonly property string _badgeTone: {
        if (root.tone === "ok")
            return "ok"
        if (root.tone === "danger")
            return "danger"
        if (root.tone === "warn" || root.tone === "accent")
            return "accent"
        return "muted"
    }

    implicitWidth: badge.implicitWidth
    implicitHeight: badge.implicitHeight

    DsBadge {
        id: badge
        anchors.fill: parent
        text: root.text
        tone: root._badgeTone
    }
}
