// Full-screen story viewer.
//
// Presentation only: beygram::ui::StoriesFacade owns the session, the playback
// clock and the reactions. This component renders the story on screen, forwards
// presses to the facade and drives tick() from its own timer so the progress bar
// moves without the page having to know about playback.
//
// Gestures:
//  - press anywhere on the story pauses playback, release resumes it;
//  - a tap on the left half goes to the previous story;
//  - a tap on the right half goes to the next story (and closes on the last);
//  - the reaction row stores an emoji for the story on screen;
//  - Escape closes the viewer.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: viewer

    /// beygram::ui::StoriesFacade instance. Nothing is shown while it is null
    /// or closed.
    property var facade: null
    /// Emoji offered in the reaction row.
    property var reactionChoices: ["❤️", "🔥", "👍", "😂", "😮"]
    /// Milliseconds between playback ticks.
    property int tickInterval: 50

    readonly property bool open: facade !== null && facade.active
    readonly property string storyId: open ? facade.currentStoryId : ""
    readonly property real storyProgress: open ? facade.progress : 0
    readonly property bool storyPaused: open && facade.paused
    readonly property string storyReaction: open ? facade.reaction : ""

    // Absolute playback offset, kept here so the facade receives the same kind
    // of positions its progress model expects. It restarts whenever the story
    // on screen changes.
    property double elapsedMs: 0

    anchors.fill: parent
    visible: open
    z: 900
    focus: open

    onStoryIdChanged: elapsedMs = 0

    onOpenChanged: {
        if (open)
            forceActiveFocus()
    }

    Keys.onEscapePressed: {
        if (viewer.facade)
            viewer.facade.close()
    }

    Timer {
        id: ticker
        interval: viewer.tickInterval
        repeat: true
        running: viewer.open && !viewer.storyPaused

        onTriggered: {
            if (!viewer.open)
                return
            viewer.elapsedMs += interval
            viewer.facade.tick(Math.round(viewer.elapsedMs))
        }
    }

    // Dimmed backdrop; a click outside the card dismisses the viewer.
    Rectangle {
        anchors.fill: parent
        color: Qt.alpha("#000000", 0.92)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (viewer.facade)
                    viewer.facade.close()
            }
        }
    }

    Rectangle {
        id: card
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 520)
        radius: 20
        color: "#0B0D12"
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.text, 0.12)
        clip: true

        // ---- progress bar -------------------------------------------------

        Item {
            id: progressTrack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 12
            height: 3

            Rectangle {
                anchors.fill: parent
                radius: 2
                color: Qt.alpha(BeyGramTheme.text, 0.20)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * Math.max(0, Math.min(1, viewer.storyProgress))
                radius: 2
                color: BeyGramTheme.text
            }
        }

        // ---- header: story id, position, close ----------------------------

        RowLayout {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: progressTrack.bottom
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            anchors.topMargin: 10
            spacing: 8

            Text {
                text: viewer.storyId.length > 0 ? qsTr("Story ") + viewer.storyId : ""
                color: "white"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            Text {
                text: viewer.open ? (viewer.facade.index + 1) + " / " + viewer.facade.count : ""
                color: Qt.alpha(BeyGramTheme.text, 0.65)
                font.pixelSize: 11
            }

            Rectangle {
                width: 30
                height: 30
                radius: 15
                color: closeArea.containsMouse ? Qt.alpha(BeyGramTheme.text, 0.14)
                                               : Qt.alpha(BeyGramTheme.text, 0.06)

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close story viewer")

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "white"
                    font.pixelSize: 13
                }

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (viewer.facade)
                            viewer.facade.close()
                    }
                }
            }
        }

        // ---- tap zones: pause while pressed, navigate on release ----------

        Item {
            id: tapLayer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: reactionRow.top
            anchors.topMargin: 8

            MouseArea {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width / 2
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Previous story")

                onPressed: {
                    if (viewer.facade)
                        viewer.facade.setPaused(true)
                }
                onReleased: {
                    if (!viewer.facade)
                        return
                    viewer.facade.setPaused(false)
                    viewer.facade.previous()
                }
                onCanceled: {
                    if (viewer.facade)
                        viewer.facade.setPaused(false)
                }

                Text {
                    anchors.centerIn: parent
                    text: "‹"
                    color: Qt.alpha("#FFFFFF", 0.35)
                    font.pixelSize: 34
                }
            }

            MouseArea {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width / 2
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Next story")

                onPressed: {
                    if (viewer.facade)
                        viewer.facade.setPaused(true)
                }
                onReleased: {
                    if (!viewer.facade)
                        return
                    viewer.facade.setPaused(false)
                    viewer.facade.next()
                }
                onCanceled: {
                    if (viewer.facade)
                        viewer.facade.setPaused(false)
                }

                Text {
                    anchors.centerIn: parent
                    text: "›"
                    color: Qt.alpha("#FFFFFF", 0.35)
                    font.pixelSize: 34
                }
            }

            Text {
                anchors.centerIn: parent
                text: "❚❚"
                color: Qt.alpha("#FFFFFF", 0.85)
                font.pixelSize: 26
                visible: viewer.storyPaused
            }
        }

        // ---- reaction row --------------------------------------------------

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 96
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.alpha("#000000", 0.0) }
                GradientStop { position: 1.0; color: Qt.alpha("#000000", 0.55) }
            }
        }

        RowLayout {
            id: reactionRow
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 26
            spacing: 10

            Repeater {
                model: viewer.reactionChoices

                delegate: Rectangle {
                    id: reactionPill
                    property string emoji: modelData
                    property bool chosen: viewer.storyReaction === emoji

                    width: 44
                    height: 44
                    radius: 22
                    color: chosen ? BeyGramTheme.accentSoft : Qt.alpha("#FFFFFF", 0.10)
                    border.width: 1
                    border.color: chosen ? BeyGramTheme.accent
                                         : Qt.alpha("#FFFFFF", 0.16)

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("React with ") + reactionPill.emoji
                    Accessible.checked: reactionPill.chosen

                    Text {
                        anchors.centerIn: parent
                        text: reactionPill.emoji
                        font.pixelSize: 21
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (viewer.facade)
                                viewer.facade.react(reactionPill.emoji)
                        }
                    }
                }
            }

            // Removing the reaction is only offered once one is stored.
            Rectangle {
                id: clearPill
                visible: viewer.storyReaction.length > 0
                width: 44
                height: 44
                radius: 22
                color: clearArea.containsMouse ? Qt.alpha(BeyGramTheme.text, 0.16)
                                               : Qt.alpha("#FFFFFF", 0.06)

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Remove reaction")

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: Qt.alpha(BeyGramTheme.text, 0.75)
                    font.pixelSize: 15
                }

                MouseArea {
                    id: clearArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (viewer.facade)
                            viewer.facade.clearReaction()
                    }
                }
            }
        }
    }
}
