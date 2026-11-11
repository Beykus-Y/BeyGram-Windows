import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

/// Compact link / web-page preview card.
///
/// Presentational only: the caller supplies already-resolved preview text and
/// an optional thumbnail file id. The thumbnail is loaded through the shared
/// `beygrammedia` image provider, and `revision` busts the image cache when the
/// same file id is downloaded again.
///
/// Visibility is deliberately not bound here — the parent decides when a
/// preview is worth showing. `hasContent` mirrors that decision.
Item {
    id: root

    property string title: ""
    property string description: ""
    property string siteName: ""
    property string url: ""
    property int thumbnailFileId: 0
    property bool thumbnailReady: false
    property int revision: 0

    signal clicked()

    readonly property bool hasContent: url.length > 0
                                       || title.length > 0
                                       || description.length > 0
    readonly property string thumbnailSource: thumbnailReady && thumbnailFileId > 0
                                              ? "image://beygrammedia/" + thumbnailFileId
                                                + "?v=" + revision
                                              : ""

    implicitWidth: 320
    implicitHeight: card.implicitHeight

    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        implicitHeight: content.implicitHeight + 20
        radius: 10
        color: Qt.alpha(BeyGramTheme.surface2, mouse.containsMouse ? 0.95 : 0.72)
        border.width: 1
        border.color: Qt.alpha(BeyGramTheme.line, 0.7)
        clip: true

        Behavior on color { ColorAnimation { duration: 140 } }

        // Accent rail marking the card as an outbound link preview.
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            width: 3
            radius: 1.5
            color: BeyGramTheme.accent
        }

        RowLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 10
            spacing: 11

            Rectangle {
                Layout.preferredWidth: 60
                Layout.preferredHeight: 60
                Layout.alignment: Qt.AlignTop
                radius: 8
                clip: true
                color: Qt.alpha(BeyGramTheme.surface2, 0.6)
                border.width: 1
                border.color: Qt.alpha(BeyGramTheme.line, 0.55)

                Image {
                    anchors.fill: parent
                    source: root.thumbnailSource
                    sourceSize: Qt.size(120, 120)
                    cache: true
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    visible: root.siteName.length > 0
                    text: root.siteName
                    color: BeyGramTheme.muted
                    font.pixelSize: 10
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.title.length > 0
                    text: root.title
                    color: BeyGramTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.description.length > 0
                    text: root.description
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    lineHeight: 1.25
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }
    }
}
