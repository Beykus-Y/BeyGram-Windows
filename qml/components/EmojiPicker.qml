pragma ComponentBehavior: Bound

// Emoji picker popup for the message composer.
//
// Presentation only. The owner assigns `facade` (an EmojiFacade); the chips
// drive facade.selectCategory(), the search field writes facade.query and the
// grid renders facade.emojis. If the facade has no catalog loaded yet, a curated
// Unicode catalog is seeded once on open so emoji search and categories work
// immediately in the composer.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Popup {
    id: root

    property var facade: null

    readonly property var categories: root.facade ? root.facade.categories : []
    readonly property var emojis: root.facade ? root.facade.emojis : []
    readonly property string query: root.facade ? root.facade.query : ""
    readonly property int categoryIndex: root.facade ? root.facade.categoryIndex : -1

    readonly property int cellSize: 38
    readonly property int cellSpacing: 2
    readonly property int _animFast: Design.motion(Design.motionFast, BeyGramAppearance.reduceMotion)

    implicitWidth: 372
    implicitHeight: 416
    width: implicitWidth
    height: implicitHeight

    padding: 0
    modal: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function ensureCatalog() {
        if (!root.facade)
            return
        if (root.facade.categories.length > 0 || root.facade.emojis.length > 0)
            return
        const catalog = [
            // Smileys
            ["\uD83D\uDE00", "grinning smile happy", qsTr("Smileys")],
            ["\uD83D\uDE03", "smile joy open", qsTr("Smileys")],
            ["\uD83D\uDE04", "grin happy laugh", qsTr("Smileys")],
            ["\uD83D\uDE01", "beam grin teeth", qsTr("Smileys")],
            ["\uD83D\uDE02", "joy tears laugh lol", qsTr("Smileys")],
            ["\uD83E\uDD23", "rofl laugh floor", qsTr("Smileys")],
            ["\uD83D\uDE0A", "blush smile warm", qsTr("Smileys")],
            ["\uD83D\uDE07", "angel halo innocent", qsTr("Smileys")],
            ["\uD83D\uDE42", "slightly smiling", qsTr("Smileys")],
            ["\uD83D\uDE09", "wink flirt", qsTr("Smileys")],
            ["\uD83D\uDE0D", "heart eyes love", qsTr("Smileys")],
            ["\uD83E\uDD70", "smiling hearts adore", qsTr("Smileys")],
            ["\uD83D\uDE18", "kiss blowing heart", qsTr("Smileys")],
            ["\uD83D\uDE0E", "sunglasses cool", qsTr("Smileys")],
            ["\uD83E\uDD14", "thinking hmm wonder", qsTr("Smileys")],
            ["\uD83E\uDD28", "raised eyebrow", qsTr("Smileys")],
            ["\uD83D\uDE10", "neutral face", qsTr("Smileys")],
            ["\uD83D\uDE0F", "smirk", qsTr("Smileys")],
            ["\uD83D\uDE12", "unamused", qsTr("Smileys")],
            ["\uD83D\uDE44", "eyeroll rolling", qsTr("Smileys")],
            ["\uD83D\uDE14", "pensive sad", qsTr("Smileys")],
            ["\uD83D\uDE34", "sleeping zzz", qsTr("Smileys")],
            ["\uD83E\uDD73", "party celebrate birthday", qsTr("Smileys")],
            ["\uD83E\uDD2F", "mind blown", qsTr("Smileys")],
            ["\uD83D\uDE31", "scream shock", qsTr("Smileys")],
            ["\uD83D\uDE2D", "crying sob tears", qsTr("Smileys")],
            ["\uD83D\uDE21", "angry mad", qsTr("Smileys")],
            ["\uD83E\uDD7A", "pleading puppy eyes", qsTr("Smileys")],
            // Gestures
            ["\uD83D\uDC4D", "thumbs up like yes ok", qsTr("Gestures")],
            ["\uD83D\uDC4E", "thumbs down dislike no", qsTr("Gestures")],
            ["\uD83D\uDC4F", "clap applause bravo", qsTr("Gestures")],
            ["\uD83D\uDE4C", "raised hands hooray", qsTr("Gestures")],
            ["\uD83D\uDC4B", "wave hello hi bye", qsTr("Gestures")],
            ["\uD83E\uDD1D", "handshake deal", qsTr("Gestures")],
            ["\uD83D\uDE4F", "pray thanks please", qsTr("Gestures")],
            ["\uD83D\uDCAA", "muscle strong flex", qsTr("Gestures")],
            ["\u270C\uFE0F", "peace victory", qsTr("Gestures")],
            ["\uD83E\uDD1E", "crossed fingers luck", qsTr("Gestures")],
            ["\uD83D\uDC4C", "ok hand perfect", qsTr("Gestures")],
            ["\uD83E\uDEF6", "heart hands", qsTr("Gestures")],
            // Hearts & Symbols
            ["\u2764\uFE0F", "red heart love", qsTr("Hearts")],
            ["\uD83E\uDDE1", "orange heart", qsTr("Hearts")],
            ["\uD83D\uDC9B", "yellow heart", qsTr("Hearts")],
            ["\uD83D\uDC9A", "green heart", qsTr("Hearts")],
            ["\uD83D\uDC99", "blue heart", qsTr("Hearts")],
            ["\uD83D\uDC9C", "purple heart", qsTr("Hearts")],
            ["\uD83D\uDDA4", "black heart", qsTr("Hearts")],
            ["\uD83E\uDD0D", "white heart", qsTr("Hearts")],
            ["\uD83D\uDC94", "broken heart", qsTr("Hearts")],
            ["\uD83D\uDC96", "sparkling heart", qsTr("Hearts")],
            ["\uD83D\uDD25", "fire hot lit", qsTr("Symbols")],
            ["\u2728", "sparkles magic", qsTr("Symbols")],
            ["\u2B50", "star favorite", qsTr("Symbols")],
            ["\uD83C\uDF1F", "glowing star", qsTr("Symbols")],
            ["\uD83D\uDCA5", "boom collision", qsTr("Symbols")],
            ["\uD83D\uDCAF", "100 hundred perfect", qsTr("Symbols")],
            ["\u2705", "check done yes", qsTr("Symbols")],
            ["\u26A1", "zap lightning fast", qsTr("Symbols")],
            // Celebration & Objects
            ["\uD83C\uDF89", "tada party confetti", qsTr("Objects")],
            ["\uD83C\uDF8A", "confetti ball", qsTr("Objects")],
            ["\uD83C\uDF81", "gift present", qsTr("Objects")],
            ["\uD83C\uDFC6", "trophy win", qsTr("Objects")],
            ["\uD83C\uDFB5", "music note", qsTr("Objects")],
            ["\uD83D\uDCF7", "camera photo", qsTr("Objects")],
            ["\uD83D\uDCBB", "laptop computer code", qsTr("Objects")],
            ["\uD83D\uDE80", "rocket launch ship", qsTr("Objects")],
            ["\u2615", "coffee tea cafe", qsTr("Food")],
            ["\uD83C\uDF55", "pizza food", qsTr("Food")],
            ["\uD83C\uDF54", "burger food", qsTr("Food")],
            ["\uD83C\uDF70", "cake birthday dessert", qsTr("Food")],
            ["\uD83C\uDF7B", "cheers beer", qsTr("Food")],
            ["\uD83C\uDF4E", "apple fruit", qsTr("Food")],
            ["\uD83C\uDF38", "cherry blossom flower", qsTr("Nature")],
            ["\uD83C\uDF3F", "herb leaf", qsTr("Nature")],
            ["\u2600\uFE0F", "sun bright", qsTr("Nature")],
            ["\uD83C\uDF19", "moon night", qsTr("Nature")],
            ["\uD83D\uDC31", "cat kitty", qsTr("Nature")],
            ["\uD83D\uDC36", "dog puppy", qsTr("Nature")]
        ]
        const emojisList = []
        const namesList = []
        const catsList = []
        for (let i = 0; i < catalog.length; ++i) {
            emojisList.push(catalog[i][0])
            namesList.push(catalog[i][1])
            catsList.push(catalog[i][2])
        }
        root.facade.setCatalog(emojisList, namesList, catsList)
    }

    function activate(emoji) {
        if (!root.facade || !emoji)
            return
        root.facade.activate(String(emoji))
        root.close()
    }

    onOpened: {
        root.ensureCatalog()
        if (root.facade)
            root.facade.query = ""
        searchField.forceActiveFocus()
    }

    background: Rectangle {
        radius: Design.cardRadius(BeyGramAppearance.designStyle)
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.modeName === "Amoled"
                      ? Qt.lighter(BeyGramTheme.line, 1.35)
                      : BeyGramTheme.line
    }

    contentItem: ColumnLayout {
        spacing: Design.space2

        Flickable {
            id: chipStrip
            Layout.fillWidth: true
            Layout.leftMargin: Design.space3
            Layout.rightMargin: Design.space3
            Layout.topMargin: Design.space3
            Layout.preferredHeight: 32
            clip: true
            contentWidth: chipRow.width
            contentHeight: chipRow.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => {
                    if (chipStrip.contentWidth <= chipStrip.width)
                        return
                    var delta = Math.abs(event.pixelDelta.x) > 0
                                ? event.pixelDelta.x
                                : (event.angleDelta.x !== 0 ? event.angleDelta.x / 2 : event.angleDelta.y / 2)
                    var maxX = Math.max(0, chipStrip.contentWidth - chipStrip.width)
                    chipStrip.contentX = Math.max(0, Math.min(maxX, chipStrip.contentX - delta))
                    event.accepted = true
                }
            }

            Row {
                id: chipRow
                spacing: 6

                ChoiceChip {
                    text: qsTr("All")
                    selected: root.categoryIndex < 0
                    onClicked: {
                        if (root.facade)
                            root.facade.selectCategory(-1)
                    }
                }

                Repeater {
                    model: root.categories

                    delegate: ChoiceChip {
                        required property int index
                        required property string modelData

                        text: modelData
                        selected: index === root.categoryIndex
                        onClicked: {
                            if (root.facade)
                                root.facade.selectCategory(index)
                        }
                    }
                }
            }
        }

        DsSearchField {
            id: searchField
            Layout.fillWidth: true
            Layout.leftMargin: Design.space3
            Layout.rightMargin: Design.space3
            Layout.preferredHeight: 36
            placeholderText: qsTr("Search emoji...")
            text: root.query
            onTextEdited: {
                if (root.facade)
                    root.facade.query = searchField.text
            }
            onCleared: {
                if (root.facade)
                    root.facade.query = ""
                text = Qt.binding(function() { return root.query })
            }
            Keys.onEscapePressed: (event) => {
                if (searchField.text.length > 0 && root.facade)
                    root.facade.query = ""
                else
                    root.close()
                event.accepted = true
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Design.space3
            Layout.rightMargin: Design.space3
            Layout.bottomMargin: Design.space3

            Flickable {
                id: scroller
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: emojiFlow.height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Flow {
                    id: emojiFlow
                    width: scroller.width
                    spacing: root.cellSpacing

                    Repeater {
                        model: root.emojis

                        delegate: Rectangle {
                            id: cell

                            required property string modelData
                            readonly property string emoji: modelData

                            width: root.cellSize
                            height: root.cellSize
                            radius: Design.radiusSm
                            color: cellArea.containsMouse ? BeyGramTheme.surface2
                                                          : "transparent"

                            Accessible.role: Accessible.Button
                            Accessible.name: cell.emoji

                            Behavior on color {
                                ColorAnimation { duration: root._animFast }
                            }

                            Text {
                                anchors.centerIn: parent
                                width: parent.width - 6
                                text: cell.emoji
                                color: BeyGramTheme.text
                                font.pixelSize: 22
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: cellArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.activate(cell.emoji)
                            }
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(240, parent.width - 40))
                visible: emojiFlow.height === 0
                text: root.query.length > 0 ? qsTr("No emoji match your search")
                                            : qsTr("No emoji to show")
                color: BeyGramTheme.muted
                font.pixelSize: Design.fontSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
