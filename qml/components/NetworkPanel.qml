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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    property var facade: null

    readonly property bool hasFacade: root.facade !== null
    readonly property bool online: root.hasFacade ? root.facade.online : false
    readonly property int retryAfter: root.hasFacade ? root.facade.retryAfter : 0
    readonly property string stateText: root.hasFacade ? root.facade.stateText : "Offline"
    readonly property bool proxyOn: root.hasFacade ? root.facade.proxyEnabled : false
    readonly property string proxyType: root.hasFacade ? root.facade.proxyType : "none"
    readonly property bool proxyValid: root.hasFacade ? root.facade.proxyValid : false
    readonly property bool endpointEditable: root.proxyType !== "none"

    readonly property var proxyTypes: [
        { value: "none", label: "Direct connection" },
        { value: "socks5", label: "SOCKS5" },
        { value: "http", label: "HTTP" },
        { value: "mtproto", label: "MTProto" }
    ]

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
        spacing: 10

        // Connection status -------------------------------------------------

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.online
                       ? BeyGramTheme.okInk
                       : (root.retryAfter > 0 ? BeyGramTheme.warnInk : BeyGramTheme.muted)
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            Text {
                Layout.fillWidth: true
                text: root.stateText
                color: BeyGramTheme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Accessible.role: Accessible.StaticText
                Accessible.name: "Connection status: " + root.stateText
            }

            Text {
                text: root.online ? "Online" : "Offline"
                color: root.online ? BeyGramTheme.okInk : BeyGramTheme.muted
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.retryAfter > 0
            text: "Retrying in " + root.retryAfter
                  + (root.retryAfter === 1 ? " second" : " seconds")
            color: BeyGramTheme.warnInk
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: BeyGramTheme.line
        }

        // Proxy -------------------------------------------------------------

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: "Proxy"
                color: BeyGramTheme.text
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            CheckBox {
                id: proxyToggle
                text: "Use proxy"
                palette.text: BeyGramTheme.text
                palette.windowText: BeyGramTheme.text
                palette.buttonText: BeyGramTheme.text
                onToggled: {
                    if (root.hasFacade)
                        root.facade.proxyEnabled = checked
                }
                Accessible.name: text
            }
        }

        Text {
            Layout.fillWidth: true
            text: "Type"
            color: BeyGramTheme.muted
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        ComboBox {
            id: typeBox
            Layout.fillWidth: true
            implicitHeight: 40
            model: root.proxyTypes
            textRole: "label"
            valueRole: "value"
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onActivated: {
                if (root.hasFacade)
                    root.facade.proxyType = currentValue
            }
            Accessible.name: "Proxy type"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Server"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: hostField
                    Layout.fillWidth: true
                    implicitHeight: 40
                    leftPadding: 12
                    rightPadding: 12
                    enabled: root.endpointEditable
                    opacity: enabled ? 1.0 : 0.5
                    selectByMouse: true
                    placeholderText: "proxy.example"
                    color: BeyGramTheme.text
                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.pixelSize: 13
                    onEditingFinished: {
                        if (root.hasFacade)
                            root.facade.proxyHost = text
                    }
                    Accessible.name: "Proxy server"
                    background: Rectangle {
                        radius: 10
                        color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                        border.width: hostField.activeFocus ? 2 : 1
                        border.color: hostField.activeFocus
                                      ? BeyGramTheme.accent
                                      : Qt.alpha(BeyGramTheme.line, 1.0)
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 110
                spacing: 6

                Text {
                    text: "Port"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: portField
                    Layout.fillWidth: true
                    implicitHeight: 40
                    leftPadding: 12
                    rightPadding: 12
                    enabled: root.endpointEditable
                    opacity: enabled ? 1.0 : 0.5
                    selectByMouse: true
                    placeholderText: "1080"
                    color: BeyGramTheme.text
                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.pixelSize: 13
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: 65535 }
                    onEditingFinished: {
                        if (root.hasFacade)
                            root.facade.proxyPort = parseInt(text, 10) || 0
                    }
                    Accessible.name: "Proxy port"
                    background: Rectangle {
                        radius: 10
                        color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                        border.width: portField.activeFocus ? 2 : 1
                        border.color: portField.activeFocus
                                      ? BeyGramTheme.accent
                                      : Qt.alpha(BeyGramTheme.line, 1.0)
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Username"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: userField
                    Layout.fillWidth: true
                    implicitHeight: 40
                    leftPadding: 12
                    rightPadding: 12
                    enabled: root.endpointEditable
                    opacity: enabled ? 1.0 : 0.5
                    selectByMouse: true
                    placeholderText: "Optional"
                    color: BeyGramTheme.text
                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.pixelSize: 13
                    onEditingFinished: {
                        if (root.hasFacade)
                            root.facade.proxyUser = text
                    }
                    Accessible.name: "Proxy username"
                    background: Rectangle {
                        radius: 10
                        color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                        border.width: userField.activeFocus ? 2 : 1
                        border.color: userField.activeFocus
                                      ? BeyGramTheme.accent
                                      : Qt.alpha(BeyGramTheme.line, 1.0)
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Password"
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: passwordField
                    Layout.fillWidth: true
                    implicitHeight: 40
                    leftPadding: 12
                    rightPadding: 12
                    enabled: root.endpointEditable
                    opacity: enabled ? 1.0 : 0.5
                    selectByMouse: true
                    echoMode: TextInput.Password
                    placeholderText: "Optional"
                    color: BeyGramTheme.text
                    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                    selectionColor: BeyGramTheme.accent
                    selectedTextColor: BeyGramTheme.onAccent
                    font.pixelSize: 13
                    // The facade has no password getter, so this field cannot be
                    // bound; it pushes every edit instead and never reads back.
                    onTextEdited: {
                        if (root.hasFacade)
                            root.facade.setProxyPassword(text)
                    }
                    onEditingFinished: {
                        if (root.hasFacade)
                            root.facade.setProxyPassword(text)
                    }
                    Accessible.name: "Proxy password"
                    background: Rectangle {
                        radius: 10
                        color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                        border.width: passwordField.activeFocus ? 2 : 1
                        border.color: passwordField.activeFocus
                                      ? BeyGramTheme.accent
                                      : Qt.alpha(BeyGramTheme.line, 1.0)
                        Behavior on border.color { ColorAnimation { duration: 120 } }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.hasFacade ? root.facade.proxyText : ""
            color: BeyGramTheme.muted
            font.pixelSize: 11
            elide: Text.ElideRight
            Accessible.role: Accessible.StaticText
            Accessible.name: "Proxy summary: " + text
        }

        Text {
            Layout.fillWidth: true
            visible: root.hasFacade && root.proxyOn && !root.proxyValid
            text: root.proxyType === "none"
                  ? "Pick a proxy type to use a proxy."
                  : "Enter a server and a port between 1 and 65535."
            color: BeyGramTheme.warnInk
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            PrimaryButton {
                text: "Apply"
                enabled: root.proxyValid
                onClicked: {
                    if (root.hasFacade)
                        root.facade.applyProxy()
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
