// StoragePanel - the storage and data-usage settings panel.
//
// Presentation only. Every value comes from `facade` (a StorageFacade) and every
// action goes back through it: ticking a kind calls facade.setSelection() with
// the resulting set, the Clean button calls facade.clean() (which reports the
// ticked kinds through cleanRequested) and the auto-download switch writes
// facade.autoDownload. The panel never scans the cache, formats a byte count or
// deletes a file itself, so every size on screen is the facade's.
//
// The facade is nullable so the panel can be created before the controller is
// wired up; until it is set, the panel renders the empty state.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    /// A StorageFacade, or null before the controller is available.
    property var facade: null

    implicitWidth: 360
    implicitHeight: layout.implicitHeight

    readonly property var kindList: root.facade ? root.facade.kinds : []
    readonly property var sizeList: root.facade ? root.facade.sizes : []
    readonly property var usageList: root.facade ? root.facade.usage : []

    function kindSelected(kind) {
        return root.facade ? root.facade.selectedKinds().indexOf(kind) >= 0 : false
    }

    function toggleKind(kind, on) {
        if (!root.facade)
            return

        const next = root.facade.selectedKinds()
        const at = next.indexOf(kind)
        if (on && at < 0)
            next.push(kind)
        else if (!on && at >= 0)
            next.splice(at, 1)
        root.facade.setSelection(next)
    }

    ColumnLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: qsTr("Storage")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Cached media on this device. Tick the kinds you want to remove.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        // One row per cached kind: the checkbox drives the cleanup selection,
        // the label is the facade's formatted size for that row.
        Repeater {
            model: root.kindList

            delegate: RowLayout {
                id: kindRow
                required property int index
                required property string modelData

                Layout.fillWidth: true
                spacing: 8

                CheckBox {
                    id: kindBox
                    Layout.fillWidth: true
                    text: kindRow.modelData
                    checked: root.kindSelected(kindRow.modelData)
                    palette.text: BeyGramTheme.text
                    palette.windowText: BeyGramTheme.text
                    palette.buttonText: BeyGramTheme.text
                    Accessible.name: text
                    onToggled: root.toggleKind(kindRow.modelData, kindBox.checked)

                    // A user toggle replaces the `checked` binding, so the box is
                    // re-synced explicitly whenever the selection moves: a tick,
                    // a storage refresh that drops a kind, a completed cleanup.
                    Connections {
                        target: root.facade
                        function onChanged() {
                            kindBox.checked = root.kindSelected(kindRow.modelData)
                        }
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: kindRow.index < root.sizeList.length
                          ? root.sizeList[kindRow.index] : ""
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.kindList.length === 0
            text: qsTr("Nothing cached yet.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 2
            height: 1
            color: BeyGramTheme.line
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: qsTr("Total")
                color: BeyGramTheme.text
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            Text {
                text: root.facade ? root.facade.totalText : ""
                color: BeyGramTheme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.facade
                      ? qsTr("Selected: %1").arg(root.facade.selectedText)
                      : ""
                color: BeyGramTheme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            PrimaryButton {
                tone: "danger"
                text: root.facade && root.facade.canClean
                      ? qsTr("Clean %1").arg(root.facade.selectedText)
                      : qsTr("Clean")
                enabled: root.facade ? root.facade.canClean : false
                onClicked: {
                    if (root.facade)
                        root.facade.clean()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            height: 1
            color: BeyGramTheme.line
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Automatic downloads")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        CheckBox {
            id: autoBox
            Layout.fillWidth: true
            text: qsTr("Download media automatically")
            checked: root.facade ? root.facade.autoDownload : true
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            Accessible.name: text
            onToggled: {
                if (root.facade)
                    root.facade.autoDownload = autoBox.checked
            }

            Connections {
                target: root.facade
                function onChanged() {
                    autoBox.checked = root.facade ? root.facade.autoDownload : true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            height: 1
            color: BeyGramTheme.line
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Data usage")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        // Already formatted by the facade:
        // "Photos · 1.0 KB sent · 512 B received".
        Repeater {
            model: root.usageList

            delegate: Text {
                required property string modelData

                Layout.fillWidth: true
                text: modelData
                color: BeyGramTheme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.usageList.length === 0
            text: qsTr("No data usage recorded yet.")
            color: BeyGramTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
