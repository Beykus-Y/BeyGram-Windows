pragma ComponentBehavior: Bound

// LyricsPanel — synchronized lyrics for the track that is currently playing.
//
// Presentation only. All state lives in the injected `facade` (a
// beygram::ui::LyricsFacade): the panel reads `lines`, `currentIndex` and
// `hasLyrics` and never parses LRC itself. The facade is nullable so the panel
// can be created before the controller is wired up; until it is set, the empty
// state is shown.
//
// The active line is drawn in the accent colour and a larger size, the
// surrounding lines stay muted, and the list scrolls the active line back to
// the centre whenever it changes. A clear() from the facade drops back to the
// empty state.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Rectangle {
    id: root

    /// A LyricsFacade, or null before the controller is available.
    property var facade: null

    readonly property var lyricLines: root._lines
    readonly property int currentIndex: root.facade ? Number(root.facade.currentIndex) : -1
    readonly property bool hasLyrics: root.facade ? Boolean(root.facade.hasLyrics) : false

    // facade.lines is re-read whenever the facade emits changed(), which also
    // fires for index-only moves. Caching the text list here and replacing it
    // only when the content differs keeps the ListView model stable, so moving
    // to the next line scrolls instead of resetting the list.
    property var _lines: []

    implicitWidth: 380
    implicitHeight: 420

    radius: Design.radiusLg
    color: BeyGramTheme.surface
    border.width: 1
    border.color: BeyGramTheme.line
    clip: true

    // Brings the highlighted line back into view. Called after the model and
    // the delegate heights have settled, so the position is accurate.
    function scrollToActiveLine() {
        if (!root.hasLyrics || root.currentIndex < 0 || root.currentIndex >= lineList.count)
            return
        lineList.positionViewAtIndex(root.currentIndex, ListView.Center)
    }

    function sameLines(left, right) {
        if (left === right)
            return true
        if (!left || !right || left.length !== right.length)
            return false
        for (var i = 0; i < left.length; ++i) {
            if (String(left[i]) !== String(right[i]))
                return false
        }
        return true
    }

    function syncLines() {
        const next = root.facade ? root.facade.lines : []
        if (root.sameLines(next, root._lines))
            return
        root._lines = next
    }

    onCurrentIndexChanged: Qt.callLater(root.scrollToActiveLine)
    onLyricLinesChanged: Qt.callLater(root.scrollToActiveLine)
    onFacadeChanged: root.syncLines()
    Component.onCompleted: root.syncLines()

    Connections {
        target: root.facade

        function onChanged() {
            root.syncLines()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Design.space4
        spacing: Design.space3

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            Text {
                Layout.fillWidth: true
                text: qsTr("Lyrics")
                color: BeyGramTheme.text
                font.pixelSize: Design.fontTitle
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                visible: root.hasLyrics
                text: root.currentIndex >= 0
                      ? qsTr("Line %1 of %2").arg(root.currentIndex + 1).arg(root.lyricLines.length)
                      : qsTr("%1 lines").arg(root.lyricLines.length)
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: BeyGramTheme.line
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: lineList

                anchors.fill: parent
                visible: root.hasLyrics
                clip: true
                model: root.lyricLines
                spacing: Design.space1
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 400

                Accessible.role: Accessible.List
                Accessible.name: qsTr("Lyrics")

                delegate: Item {
                    id: lineRow

                    required property int index
                    required property string modelData

                    readonly property bool active: lineRow.index === root.currentIndex

                    width: lineList.width
                    height: lineText.implicitHeight + Design.space3 * 2

                    Accessible.role: Accessible.StaticText
                    Accessible.name: lineRow.modelData

                    Rectangle {
                        anchors.fill: parent
                        radius: Design.radiusSm
                        color: lineRow.active ? BeyGramTheme.accentSoft : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Text {
                        id: lineText

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Design.space3
                        anchors.rightMargin: Design.space3
                        anchors.verticalCenter: parent.verticalCenter

                        text: lineRow.modelData
                        color: lineRow.active ? BeyGramTheme.accent : BeyGramTheme.muted
                        font.pixelSize: lineRow.active ? Design.fontTitle : Design.fontBodyLarge
                        font.weight: lineRow.active ? Font.DemiBold : Font.Normal
                        lineHeight: Design.lineNormal
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignLeft

                        Behavior on color {
                            ColorAnimation {
                                duration: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
            }

            DsEmptyState {
                anchors.fill: parent
                visible: !root.hasLyrics
                glyph: "\u266A"
                title: qsTr("No lyrics available")
                description: qsTr("Synchronized lyrics appear here while a track with lyrics is playing.")
            }
        }
    }
}
