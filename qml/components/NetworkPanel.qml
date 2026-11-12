// NetworkPanel - connection status and proxy form.
//
// The parent owns the facade: it assigns `facade` (a NetworkFacade) and reacts
// to proxyApplyRequested() by mapping the payload onto TelegramProxySettings.
// This file only renders the form and writes the user's edits back into the
// facade; it never talks to TDLib, the network or storage.
//
// Edits are committed on editingFinished (or on every keystroke for the
// password, which has no getter), so the facade stays the single source of
// truth. The fields are driven by Binding elements rather than by `text:`
// expressions, because typing into a TextField replaces a property binding and
// an external change would then no longer reach the field.
//
// Layout comes from the design system: a DsSection per group, a DsCard for each
// group, DsListRow for the settings, DsSwitch / DsSegmented for the choices and
// DsTextField for the proxy fields.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    property var facade: null

    readonly property bool hasFacade: root.facade !== null
    readonly property bool online: root.hasFacade ? root.facade.online : false
    readonly property int retryAfter: root.hasFacade ? root.facade.retryAfter : 0
    readonly property string stateText: root.hasFacade ? root.facade.stateText : qsTr("Offline")
    readonly property bool proxyOn: root.hasFacade ? root.facade.proxyEnabled : false
    readonly property string proxyType: root.hasFacade ? root.facade.proxyType : "none"
    readonly property bool proxyValid: root.hasFacade ? root.facade.proxyValid : false
    readonly property bool endpointEditable: root.proxyType !== "none"

    readonly property var proxyTypes: [
        { value: "none", label: qsTr("Direct") },
        { value: "socks5", label: qsTr("SOCKS5") },
        { value: "http", label: qsTr("HTTP") },
        { value: "mtproto", label: qsTr("MTProto") }
    ]

    /// The labels alone, which is the shape DsSegmented takes as its model.
    readonly property var proxyTypeLabels: root.proxyTypes.map(function (type) {
        return type.label
    })

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    // Index of the facade's type in proxyTypes; 0 (direct) when it is unknown.
    function typeIndex() {
        if (!root.hasFacade)
            return 0

        const current = root.facade.proxyType
        for (let i = 0; i < root.proxyTypes.length; ++i) {
            if (root.proxyTypes[i].value === current)
                return i
        }
        return 0
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Design.space4

        // Connection status -------------------------------------------------

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Connection")
            description: qsTr("Telegram keeps the connection open in the background.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        title: root.stateText
                        subtitle: root.online
                                  ? qsTr("Messages are delivered as they arrive.")
                                  : qsTr("Sending and receiving are paused.")
                        hoverEnabled: false

                        trailing: DsBadge {
                            text: root.online ? qsTr("Online") : qsTr("Offline")
                            tone: root.online ? "ok" : "muted"
                            dot: true
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: Design.space3
                        Layout.rightMargin: Design.space3
                        Layout.bottomMargin: Design.space3
                        visible: root.retryAfter > 0
                        text: root.retryAfter === 1
                              ? qsTr("Retrying in 1 second")
                              : qsTr("Retrying in %1 seconds").arg(root.retryAfter)
                        color: BeyGramTheme.warnInk
                        font.pixelSize: Design.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // Proxy -------------------------------------------------------------

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Proxy")
            description: qsTr("Route Telegram traffic through a proxy server.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Use proxy")
                        subtitle: qsTr("Send connections through the server below.")

                        trailing: DsSwitch {
                            id: proxyToggle

                            Accessible.name: qsTr("Use proxy")

                            onToggled: {
                                if (root.hasFacade)
                                    root.facade.proxyEnabled = proxyToggle.checked
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Type")
                        hoverEnabled: false

                        trailing: DsSegmented {
                            id: typeBox

                            width: 300
                            model: root.proxyTypeLabels

                            onActivated: function (index) {
                                if (root.hasFacade)
                                    root.facade.proxyType = root.proxyTypes[index].value
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        spacing: Design.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsTextField {
                                id: hostField

                                Layout.fillWidth: true
                                label: qsTr("Server")
                                placeholderText: "proxy.example"
                                enabled: root.endpointEditable
                                opacity: enabled ? 1.0 : Design.opacityDisabled
                                Accessible.name: qsTr("Proxy server")

                                onEditingFinished: {
                                    if (root.hasFacade)
                                        root.facade.proxyHost = text
                                }
                            }

                            DsTextField {
                                id: portField

                                Layout.preferredWidth: 120
                                label: qsTr("Port")
                                placeholderText: "1080"
                                enabled: root.endpointEditable
                                opacity: enabled ? 1.0 : Design.opacityDisabled
                                inputMethodHints: Qt.ImhDigitsOnly
                                validator: IntValidator { bottom: 0; top: 65535 }
                                Accessible.name: qsTr("Proxy port")

                                onEditingFinished: {
                                    if (root.hasFacade)
                                        root.facade.proxyPort = parseInt(text, 10) || 0
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsTextField {
                                id: userField

                                Layout.fillWidth: true
                                label: qsTr("Username")
                                placeholderText: qsTr("Optional")
                                enabled: root.endpointEditable
                                opacity: enabled ? 1.0 : Design.opacityDisabled
                                Accessible.name: qsTr("Proxy username")

                                onEditingFinished: {
                                    if (root.hasFacade)
                                        root.facade.proxyUser = text
                                }
                            }

                            DsTextField {
                                id: passwordField

                                Layout.fillWidth: true
                                label: qsTr("Password")
                                placeholderText: qsTr("Optional")
                                enabled: root.endpointEditable
                                opacity: enabled ? 1.0 : Design.opacityDisabled
                                echoMode: TextInput.Password
                                Accessible.name: qsTr("Proxy password")

                                // The facade has no password getter, so this
                                // field cannot be bound; it pushes every edit
                                // instead and never reads back.
                                onTextEdited: {
                                    if (root.hasFacade)
                                        root.facade.setProxyPassword(text)
                                }
                                onEditingFinished: {
                                    if (root.hasFacade)
                                        root.facade.setProxyPassword(text)
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.hasFacade ? root.facade.proxyText : ""
                            visible: text.length > 0
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            elide: Text.ElideRight
                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("Proxy summary: ") + text
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.hasFacade && root.proxyOn && !root.proxyValid
                            text: root.proxyType === "none"
                                  ? qsTr("Pick a proxy type to use a proxy.")
                                  : qsTr("Enter a server and a port between 1 and 65535.")
                            color: BeyGramTheme.warnInk
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            Item { Layout.fillWidth: true }

                            DsButton {
                                text: qsTr("Apply")
                                tone: "primary"
                                enabled: root.proxyValid
                                onClicked: {
                                    if (root.hasFacade)
                                        root.facade.applyProxy()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // The switch, the selector and the fields are all written by the user, and
    // a user edit replaces a property binding. Binding elements survive that, so
    // an external change still reaches the control.
    Binding {
        target: proxyToggle
        property: "checked"
        value: root.proxyOn
        when: root.hasFacade
        restoreMode: Binding.RestoreNone
    }

    Binding {
        target: typeBox
        property: "currentIndex"
        value: root.typeIndex()
        when: root.hasFacade
        restoreMode: Binding.RestoreNone
    }

    Binding {
        target: hostField
        property: "text"
        value: root.hasFacade ? root.facade.proxyHost : ""
        when: root.hasFacade && !hostField.activeFocus
        restoreMode: Binding.RestoreNone
    }

    Binding {
        target: portField
        property: "text"
        value: root.hasFacade && root.facade.proxyPort > 0
               ? String(root.facade.proxyPort)
               : ""
        when: root.hasFacade && !portField.activeFocus
        restoreMode: Binding.RestoreNone
    }

    Binding {
        target: userField
        property: "text"
        value: root.hasFacade ? root.facade.proxyUser : ""
        when: root.hasFacade && !userField.activeFocus
        restoreMode: Binding.RestoreNone
    }
}
