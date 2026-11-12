// SelectionToolbar — floating pill action bar shown while a message selection
// is active.
//
// Presentation only. All selection state lives in the injected `facade`
// (beygram::ui::SelectionFacade): the bar reads its count, asks it to open the
// forward picker and clears the selection through it. Deletion is not owned by
// the facade, so the Delete action only announces `deleteRequested()` for the
// page to perform. Nothing here touches TDLib, the network, the message store
// or logging.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: toolbar

    /// The SelectionFacade driving this toolbar. May be null while the page is
    /// still being constructed.
    property var facade

    readonly property int selectedCount: toolbar.facade ? toolbar.facade.count : 0
    readonly property bool hasSelection: toolbar.selectedCount > 0

    /// Emitted when the user asks to delete the selected messages. The page
    /// performs the request and then clears the selection through the facade.
    signal deleteRequested()

    visible: toolbar.facade ? toolbar.facade.active : false
    implicitWidth: pill.implicitWidth
    implicitHeight: pill.implicitHeight + Design.space2

    // Floating surface: a pill one step above the page, with a hairline so it
    // stays legible over both the message list and the composer.
    Rectangle {
        id: pill

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: content.implicitWidth + Design.space3 * 2
        implicitHeight: Design.controlHeightLarge
        radius: Design.radiusPill
        color: BeyGramTheme.surface2
        border.width: 1
        border.color: BeyGramTheme.line
        antialiasing: true

        RowLayout {
            id: content
            anchors.centerIn: parent
            spacing: Design.space2

            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: Design.space1

                DsBadge {
                    Layout.alignment: Qt.AlignVCenter
                    tone: "accent"
                    text: toolbar.selectedCount.toString()
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("selected")
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                }
            }

            DsButton {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Forward")
                tone: "primary"
                size: "sm"
                enabled: toolbar.hasSelection
                onClicked: {
                    if (toolbar.facade)
                        toolbar.facade.beginForward();
                }
            }

            DsButton {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Delete")
                tone: "danger"
                size: "sm"
                enabled: toolbar.hasSelection
                onClicked: toolbar.deleteRequested()
            }

            DsButton {
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Cancel")
                tone: "secondary"
                size: "sm"
                enabled: !!toolbar.facade
                onClicked: {
                    if (toolbar.facade)
                        toolbar.facade.clear();
                }
            }
        }
    }
}
