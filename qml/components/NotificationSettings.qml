// NotificationSettings - the notification settings section.
//
// Presentation only. `facade` is a NotificationsFacade (nullable until the
// controller is wired up); every control reads a facade property and writes
// back through a facade setter. The two quiet-hours fields are the only local
// state: they hold the text being typed and are applied as a pair, because a
// half-typed time is not a window.
//
// Nothing here touches TDLib, storage or the clock. Saving a chat calls
// facade.saveChat(), which emits chatSettingsRequested only when something
// actually changed, so the Save button can stay enabled without sending
// duplicates.
//
// Layout comes from the design system: a DsSection per group, a DsCard for each
// group, DsListRow with a DsSwitch for the toggles, DsTextField for the quiet
// hours and DsChip for the sound picker.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

ColumnLayout {
    id: root

    /// A NotificationsFacade, or null before the controller is available.
    property var facade: null

    /// True while the quiet-hours text is not a pair of times.
    property bool quietInvalid: false

    readonly property bool ready: root.facade !== null && root.facade !== undefined
    readonly property bool notificationsOn: root.ready && Boolean(root.facade.enabled)

    // The sound picker offers "Silent" itself, because the facade lists stored
    // sounds only: row 0 is silent and row i + 1 is soundTitles[i].
    readonly property var soundLabels: [qsTr("Silent")].concat(root.ready ? root.facade.soundTitles : [])
    readonly property int soundRow: root.ready ? Number(root.facade.soundIndex) + 1 : 0

    spacing: Design.space4

    function minutesToText(minutes) {
        const value = Math.max(0, Math.min(1439, Math.round(Number(minutes))))
        const hour = Math.floor(value / 60)
        const minute = value % 60
        return (hour < 10 ? "0" : "") + hour + ":" + (minute < 10 ? "0" : "") + minute
    }

    // "22:00" -> 1320, or -1 when the text does not name a time of day.
    function textToMinutes(text) {
        const match = /^([0-9]{1,2}):([0-9]{2})$/.exec(String(text).trim())
        if (!match)
            return -1
        const hour = Number(match[1])
        const minute = Number(match[2])
        if (hour > 23 || minute > 59)
            return -1
        return hour * 60 + minute
    }

    // Pulls every control back to the facade. Guarded writes keep a user
    // interaction from being overwritten and stop the sync from feeding itself;
    // a quiet-hours field the user is editing is left alone.
    function syncFromFacade() {
        if (!root.ready)
            return

        const enabled = Boolean(root.facade.enabled)
        if (enabledBox.checked !== enabled)
            enabledBox.checked = enabled

        const sound = Boolean(root.facade.sound)
        if (soundBox.checked !== sound)
            soundBox.checked = sound

        const preview = Boolean(root.facade.preview)
        if (previewBox.checked !== preview)
            previewBox.checked = preview

        const dnd = Boolean(root.facade.dnd)
        if (dndBox.checked !== dnd)
            dndBox.checked = dnd

        const chatMuted = Boolean(root.facade.chatMuted)
        if (chatMuteBox.checked !== chatMuted)
            chatMuteBox.checked = chatMuted

        const chatSound = Boolean(root.facade.chatSound)
        if (chatSoundBox.checked !== chatSound)
            chatSoundBox.checked = chatSound

        const chatPreview = Boolean(root.facade.chatPreview)
        if (chatPreviewBox.checked !== chatPreview)
            chatPreviewBox.checked = chatPreview

        if (!quietStartField.activeFocus)
            quietStartField.text = root.minutesToText(root.facade.quietStart)
        if (!quietEndField.activeFocus)
            quietEndField.text = root.minutesToText(root.facade.quietEnd)
    }

    function applyQuietHours() {
        if (!root.ready)
            return

        const start = root.textToMinutes(quietStartField.text)
        const end = root.textToMinutes(quietEndField.text)
        if (start < 0 || end < 0) {
            root.quietInvalid = true
            return
        }

        root.quietInvalid = false
        root.facade.setQuietHours(start, end)
        // The model clamps the bounds and equal times turn the window off, so
        // show what it actually stored rather than what was typed.
        root.syncFromFacade()
    }

    function clearQuietHours() {
        if (!root.ready)
            return

        root.quietInvalid = false
        root.facade.setQuietHours(0, 0)
        root.syncFromFacade()
    }

    Component.onCompleted: root.syncFromFacade()
    onFacadeChanged: root.syncFromFacade()

    Connections {
        target: root.facade
        function onChanged() { root.syncFromFacade() }
    }

    // Global switches.
    DsSection {
        Layout.fillWidth: true
        title: qsTr("Notifications")
        description: qsTr("What happens when a message arrives while the window is in the background.")

        content: DsCard {
            Layout.fillWidth: true

            content: ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Desktop notifications")
                    subtitle: qsTr("Show an alert for every new message.")

                    trailing: DsSwitch {
                        id: enabledBox

                        enabled: root.ready
                        Accessible.name: qsTr("Desktop notifications")

                        onToggled: if (root.ready) root.facade.enabled = enabledBox.checked
                    }
                }

                DsDivider {
                    Layout.fillWidth: true
                    inset: Design.space3
                }

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Play notification sound")
                    subtitle: qsTr("A short sound for each alert.")

                    trailing: DsSwitch {
                        id: soundBox

                        enabled: root.notificationsOn
                        Accessible.name: qsTr("Play notification sound")

                        onToggled: if (root.ready) root.facade.sound = soundBox.checked
                    }
                }

                DsDivider {
                    Layout.fillWidth: true
                    inset: Design.space3
                }

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Show message preview")
                    subtitle: qsTr("Include the message text in the alert.")

                    trailing: DsSwitch {
                        id: previewBox

                        enabled: root.notificationsOn
                        Accessible.name: qsTr("Show message preview")

                        onToggled: if (root.ready) root.facade.preview = previewBox.checked
                    }
                }

                DsDivider {
                    Layout.fillWidth: true
                    inset: Design.space3
                }

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Do not disturb")
                    subtitle: qsTr("Keeps everything silent until you turn it off.")

                    trailing: DsSwitch {
                        id: dndBox

                        enabled: root.notificationsOn
                        Accessible.name: qsTr("Do not disturb")

                        onToggled: if (root.ready) root.facade.dnd = dndBox.checked
                    }
                }
            }
        }
    }

    // Quiet hours.
    DsSection {
        Layout.fillWidth: true
        title: qsTr("Quiet hours")
        description: qsTr("Notifications stay silent between these times. Equal times turn the window off.")

        content: DsCard {
            Layout.fillWidth: true

            content: ColumnLayout {
                Layout.fillWidth: true
                spacing: Design.space3

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    DsTextField {
                        id: quietStartField

                        Layout.preferredWidth: 110
                        label: qsTr("From")
                        maximumLength: 5
                        horizontalAlignment: TextInput.AlignHCenter
                        placeholderText: "22:00"
                        enabled: root.notificationsOn
                        opacity: enabled ? 1.0 : Design.opacityDisabled
                        Accessible.name: qsTr("Quiet hours start")

                        onTextEdited: root.quietInvalid = false
                    }

                    DsTextField {
                        id: quietEndField

                        Layout.preferredWidth: 110
                        label: qsTr("To")
                        maximumLength: 5
                        horizontalAlignment: TextInput.AlignHCenter
                        placeholderText: "08:00"
                        enabled: root.notificationsOn
                        opacity: enabled ? 1.0 : Design.opacityDisabled
                        Accessible.name: qsTr("Quiet hours end")

                        onTextEdited: root.quietInvalid = false
                    }

                    DsButton {
                        Layout.alignment: Qt.AlignBottom
                        text: qsTr("Apply")
                        tone: "primary"
                        size: "sm"
                        enabled: root.notificationsOn
                        onClicked: root.applyQuietHours()
                    }

                    DsButton {
                        Layout.alignment: Qt.AlignBottom
                        text: qsTr("Off")
                        tone: "secondary"
                        size: "sm"
                        enabled: root.notificationsOn
                        onClicked: root.clearQuietHours()
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.quietInvalid
                          ? qsTr("Use a 24-hour time like 22:00.")
                          : qsTr("Quiet hours: %1").arg(root.ready ? root.facade.quietText : qsTr("Off"))
                    color: root.quietInvalid ? BeyGramTheme.danger : BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    wrapMode: Text.WordWrap
                    Accessible.name: text
                }
            }
        }
    }

    // Sound picker: "Silent" plus every sound the facade offers.
    DsSection {
        Layout.fillWidth: true
        title: qsTr("Notification sound")
        description: qsTr("The sound plays for chats that are not muted.")

        content: DsCard {
            Layout.fillWidth: true

            content: ColumnLayout {
                Layout.fillWidth: true
                spacing: Design.space2

                Flow {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Repeater {
                        model: root.soundLabels

                        delegate: DsChip {
                            required property int index
                            required property string modelData

                            text: modelData
                            selected: root.soundRow === index
                            enabled: root.ready
                            onClicked: if (root.ready) root.facade.selectSound(index - 1)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.soundLabels.length <= 1
                    text: qsTr("No sounds are offered yet.")
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // Per-chat overrides for the chat the parent loaded through
    // facade.loadChat().
    DsSection {
        Layout.fillWidth: true
        title: qsTr("This chat")
        description: qsTr("Overrides for the chat this screen loaded. They are sent when you save.")

        content: DsCard {
            Layout.fillWidth: true

            content: ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Mute notifications")
                    subtitle: qsTr("No alerts for this chat.")

                    trailing: DsSwitch {
                        id: chatMuteBox

                        enabled: root.ready
                        Accessible.name: qsTr("Mute notifications")

                        onToggled: if (root.ready) root.facade.setChatMuted(chatMuteBox.checked)
                    }
                }

                DsDivider {
                    Layout.fillWidth: true
                    inset: Design.space3
                }

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Play notification sound")
                    subtitle: qsTr("Overrides the global sound for this chat.")

                    trailing: DsSwitch {
                        id: chatSoundBox

                        enabled: root.ready && !Boolean(root.facade.chatMuted)
                        Accessible.name: qsTr("Play notification sound for this chat")

                        onToggled: if (root.ready) root.facade.setChatSound(chatSoundBox.checked)
                    }
                }

                DsDivider {
                    Layout.fillWidth: true
                    inset: Design.space3
                }

                DsListRow {
                    Layout.fillWidth: true
                    title: qsTr("Show message preview")
                    subtitle: qsTr("Overrides the global preview for this chat.")

                    trailing: DsSwitch {
                        id: chatPreviewBox

                        enabled: root.ready && !Boolean(root.facade.chatMuted)
                        Accessible.name: qsTr("Show message preview for this chat")

                        onToggled: if (root.ready) root.facade.setChatPreview(chatPreviewBox.checked)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Design.space3
                    spacing: Design.space2

                    Item { Layout.fillWidth: true }

                    // Always enabled while a facade is present: saveChat()
                    // itself is a no-op when the chat is clean, so this cannot
                    // send duplicates.
                    DsButton {
                        text: qsTr("Save")
                        tone: "primary"
                        size: "sm"
                        enabled: root.ready
                        onClicked: if (root.ready) root.facade.saveChat()
                    }
                }
            }
        }
    }
}
