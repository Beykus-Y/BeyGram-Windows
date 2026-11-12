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
//
// Layout comes from the design system: a DsSection per group, a DsCard for each
// group, DsListRow per setting, DsSwitch for the toggles and DsDialog for the
// destructive cleanup confirmation.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

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
        spacing: Design.space4

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Storage")
            description: qsTr("Cached media on this device. Choose the kinds you want to remove.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: root.kindList.length === 0
                        title: qsTr("Nothing cached yet.")
                        muted: true
                        hoverEnabled: false
                    }

                    // One row per cached kind: the switch drives the cleanup
                    // selection, the subtitle is the facade's formatted size.
                    Repeater {
                        model: root.kindList

                        delegate: ColumnLayout {
                            id: kindRow

                            required property int index
                            required property string modelData

                            Layout.fillWidth: true
                            spacing: 0

                            DsListRow {
                                Layout.fillWidth: true
                                title: kindRow.modelData
                                subtitle: kindRow.index < root.sizeList.length
                                          ? root.sizeList[kindRow.index] : ""

                                trailing: DsSwitch {
                                    id: kindSwitch

                                    checked: root.kindSelected(kindRow.modelData)
                                    Accessible.name: kindRow.modelData

                                    onToggled: root.toggleKind(kindRow.modelData, kindSwitch.checked)
                                }
                            }

                            // A user toggle replaces the `checked` binding, so
                            // the switch is re-synced whenever the selection
                            // moves: a tick, a refresh that drops a kind, or a
                            // completed cleanup.
                            Connections {
                                target: root.facade

                                function onChanged() {
                                    kindSwitch.checked = root.kindSelected(kindRow.modelData)
                                }
                            }

                            DsDivider {
                                Layout.fillWidth: true
                                inset: Design.space3
                                visible: kindRow.index < root.kindList.length - 1
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                        visible: root.kindList.length > 0
                    }

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Total")
                        hoverEnabled: false

                        trailing: Text {
                            text: root.facade ? root.facade.totalText : ""
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontBody
                            font.weight: Font.DemiBold
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            text: root.facade
                                  ? qsTr("Selected: %1").arg(root.facade.selectedText)
                                  : ""
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            elide: Text.ElideRight
                        }

                        DsButton {
                            text: root.facade && root.facade.canClean
                                  ? qsTr("Clean %1").arg(root.facade.selectedText)
                                  : qsTr("Clean")
                            tone: "danger"
                            size: "sm"
                            enabled: root.facade ? root.facade.canClean : false
                            onClicked: cleanDialog.open()
                        }
                    }
                }
            }
        }

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Automatic downloads")

            content: DsCard {
                Layout.fillWidth: true

                content: DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Download media automatically")
                    subtitle: qsTr("New photos and videos arrive without asking.")

                    trailing: DsSwitch {
                        id: autoBox

                        checked: root.facade ? root.facade.autoDownload : true
                        Accessible.name: qsTr("Download media automatically")

                        onToggled: {
                            if (root.facade)
                                root.facade.autoDownload = autoBox.checked
                        }
                    }
                }
            }
        }

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Data usage")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        visible: root.usageList.length === 0
                        title: qsTr("No data usage recorded yet.")
                        muted: true
                        hoverEnabled: false
                    }

                    // Already formatted by the facade:
                    // "Photos · 1.0 KB sent · 512 B received".
                    Repeater {
                        model: root.usageList

                        delegate: DsListRow {
                            required property int index
                            required property string modelData

                            Layout.fillWidth: true
                            title: modelData
                            muted: true
                            hoverEnabled: false
                        }
                    }
                }
            }
        }
    }

    // Cleaning deletes cached files, so it is confirmed before the facade is
    // asked to run it.
    DsDialog {
        id: cleanDialog

        title: qsTr("Clean cached media?")
        description: root.facade && root.facade.canClean
                     ? qsTr("This removes %1 of cached media from this device.").arg(root.facade.selectedText)
                     : qsTr("This removes the ticked media from this device.")
        primaryText: qsTr("Clean")
        secondaryText: qsTr("Cancel")
        tone: "danger"

        onAccepted: {
            if (root.facade)
                root.facade.clean()
        }
    }

    // A user toggle replaces the `checked` binding, so the switch is re-synced
    // whenever the facade reports a real change.
    Connections {
        target: root.facade

        function onChanged() {
            autoBox.checked = root.facade ? root.facade.autoDownload : true
        }
    }
}
