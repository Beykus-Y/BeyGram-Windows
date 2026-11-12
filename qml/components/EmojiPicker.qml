// Emoji picker popup for the message composer.
//
// Presentation only. The owner assigns `facade` (an EmojiFacade); the chips
// drive facade.selectCategory(), the search field writes facade.query and the
// grid renders facade.emojis, so all filtering lives in C++ where it is
// unit-tested. Tapping a cell calls facade.activate(emoji), which is what
// notifies the owner through EmojiFacade::emojiActivated. Nothing in this file
// touches TDLib, the network or the message store.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Popup {
    id: root

    /// EmojiFacade. Every binding below is null-safe, so the picker can be
    /// previewed without an owner.
    property var facade: null

    /// Mirrors of the facade's properties, read once so a delegate never has
    /// to repeat the null check. An absent facade reads as an empty picker.
    readonly property var categories: root.facade ? root.facade.categories : []
    readonly property var emojis: root.facade ? root.facade.emojis : []
    readonly property string query: root.facade ? root.facade.query : ""
    readonly property int categoryIndex: root.facade ? root.facade.categoryIndex : -1

    /// Edge length of one emoji cell; the Flow wraps to as many columns as the
    /// popup width allows.
    readonly property int cellSize: 38
    readonly property int cellSpacing: 2

    implicitWidth: 380
    implicitHeight: 440
    width: implicitWidth
    height: implicitHeight

    padding: 0
    modal: false
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // One tap path for the grid: the facade records the use and re-emits it on
    // its own signal, so the owner only ever listens to facade.emojiActivated.
    function activate(emoji) {
        if (!root.facade || !emoji)
            return
        root.facade.activate(String(emoji))
        root.close()
    }

    // A picker always opens on the full catalog: a stale search from the last
    // time would hide emoji the user came back for. The section selection is
    // kept, because that is a deliberate browsing choice. The field is cleared
    // through the facade, never by assigning text, so its binding survives.
    onOpened: {
        if (root.facade)
            root.facade.query = ""
        searchField.forceActiveFocus()
    }

    background: Rectangle {
        radius: 14
        color: BeyGramTheme.surface
        border.width: 1
        border.color: BeyGramTheme.line
    }

    contentItem: ColumnLayout {
        spacing: 8

        // Section strip. A Flickable keeps a long catalog on one line instead
        // of squeezing every chip.
        Flickable {
            id: chipStrip
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.preferredHeight: 32
            clip: true
            contentWidth: chipRow.width
            contentHeight: chipRow.height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds

            Row {
                id: chipRow
                spacing: 6

                ChoiceChip {
                    text: "All"
                    selected: root.categoryIndex < 0
                    onClicked: {
                        if (root.facade)
                            root.facade.selectCategory(-1)
                    }
                }

                Repeater {
                    model: root.categories

                    delegate: ChoiceChip {
                        // Repeater over a JS array: the roles are `index` and
                        // `modelData`, both taken from the delegate context.
                        text: String(modelData)
                        selected: index === root.categoryIndex
                        onClicked: {
                            if (root.facade)
                                root.facade.selectCategory(index)
                        }
                    }
                }
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.preferredHeight: 34
            implicitHeight: 34
            leftPadding: 13
            rightPadding: 13
            placeholderText: "Search emoji"
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 12
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent
            // The facade trims, so the binding can hand back a shorter string
            // than was typed; that is the facade's search contract.
            text: root.query
            onTextEdited: {
                if (root.facade)
                    root.facade.query = searchField.text
            }
            Keys.onEscapePressed: (event) => {
                // Clearing goes through the facade for the same reason as
                // onOpened: the text property stays bound to root.query.
                if (searchField.text.length > 0 && root.facade) {
                    root.facade.query = ""
                } else {
                    root.close()
                }
                event.accepted = true
            }

            background: Rectangle {
                radius: 17
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: searchField.activeFocus ? BeyGramTheme.accent
                                                      : Qt.alpha(BeyGramTheme.line, 0.9)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12

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

                            readonly property string emoji: String(modelData)

                            width: root.cellSize
                            height: root.cellSize
                            radius: 10
                            color: cellArea.containsMouse ? BeyGramTheme.surface2
                                                          : "transparent"

                            Accessible.role: Accessible.Button
                            Accessible.name: cell.emoji

                            Behavior on color { ColorAnimation { duration: 100 } }

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

            // A Flow skips invisible children, so its height stays 0 while the
            // catalog is empty or the filters hide every emoji.
            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(240, parent.width - 40))
                visible: emojiFlow.height === 0
                text: root.query.length > 0 ? "No emoji match your search"
                                            : "No emoji to show"
                color: BeyGramTheme.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
}
