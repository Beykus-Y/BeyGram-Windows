// LinkPreviewCard — compact link / web-page preview card.
//
// Presentational only: the caller supplies already-resolved preview text and an
// optional thumbnail file id. The thumbnail is loaded through the shared
// `beygrammedia` image provider, and `revision` busts the image cache when the
// same file id is downloaded again.
//
// The card is a DsCard at a 12 px radius: site name in accent ink, title at
// 13 px medium, description at 12 px muted, with the thumbnail on the left when
// one exists. Visibility is deliberately not bound here — the parent decides
// when a preview is worth showing. `hasContent` mirrors that decision.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

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

    DsCard {
        id: card

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        radius: Design.radiusMd
        padding: Design.space3
        interactive: true
        onClicked: root.clicked()

        Accessible.role: Accessible.Link
        Accessible.name: root.title.length > 0 ? root.title : root.url
        Accessible.description: root.siteName

        content: RowLayout {
            Layout.fillWidth: true
            spacing: Design.space3

            // Thumbnail slot: only reserved when the caller has a picture, so
            // text-only previews use the full card width.
            Rectangle {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                Layout.alignment: Qt.AlignTop
                visible: root.thumbnailSource.length > 0
                radius: Design.radiusSm
                clip: true
                color: Qt.alpha(BeyGramTheme.surface2, 0.9)
                border.width: 1
                border.color: Qt.alpha(BeyGramTheme.line, 0.8)

                Image {
                    anchors.fill: parent
                    source: root.thumbnailSource
                    sourceSize: Qt.size(112, 112)
                    cache: true
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    visible: root.siteName.length > 0
                    text: root.siteName
                    color: BeyGramTheme.accentInk
                    font.pixelSize: Design.fontCaption
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    Accessible.ignored: true
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.title.length > 0
                    text: root.title
                    color: BeyGramTheme.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    lineHeight: Design.lineTight
                    Accessible.ignored: true
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.description.length > 0
                    text: root.description
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    lineHeight: Design.lineNormal
                    Accessible.ignored: true
                }
            }
        }
    }

    // The URL is not printed in the card, so surface it on hover.
    HoverHandler {
        id: hover
    }

    ToolTip.visible: hover.hovered && root.url.length > 0
    ToolTip.text: root.url
    ToolTip.delay: 800
}
