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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

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

    spacing: 16

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
    Card {
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Notifications")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        CheckBox {
            id: enabledBox
            Layout.fillWidth: true
            text: qsTr("Desktop notifications")
            enabled: root.ready
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.enabled = checked
        }

        CheckBox {
            id: soundBox
            Layout.fillWidth: true
            text: qsTr("Play notification sound")
            enabled: root.notificationsOn
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.sound = checked
        }

        CheckBox {
            id: previewBox
            Layout.fillWidth: true
            text: qsTr("Show message preview")
            enabled: root.notificationsOn
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.preview = checked
        }

        CheckBox {
            id: dndBox
            Layout.fillWidth: true
            text: qsTr("Do not disturb")
            enabled: root.notificationsOn
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.dnd = checked
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Do not disturb keeps everything silent until you turn it off.")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    // Quiet hours.
    Card {
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Quiet hours")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Notifications stay silent between these times. Equal times turn the window off.")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: quietStartField
                Layout.preferredWidth: 88
                maximumLength: 5
                horizontalAlignment: TextInput.AlignHCenter
                placeholderText: "22:00"
                enabled: root.notificationsOn
                font.pixelSize: 13
                palette.text: BeyGramTheme.text
                palette.windowText: BeyGramTheme.text
                palette.base: BeyGramTheme.surface2
                palette.highlight: BeyGramTheme.accentSoft
                palette.highlightedText: BeyGramTheme.accentInk
                Accessible.name: qsTr("Quiet hours start")
                onTextEdited: root.quietInvalid = false
            }

            Text {
                text: "\u2013"
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }

            TextField {
                id: quietEndField
                Layout.preferredWidth: 88
                maximumLength: 5
                horizontalAlignment: TextInput.AlignHCenter
                placeholderText: "08:00"
                enabled: root.notificationsOn
                font.pixelSize: 13
                palette.text: BeyGramTheme.text
                palette.windowText: BeyGramTheme.text
                palette.base: BeyGramTheme.surface2
                palette.highlight: BeyGramTheme.accentSoft
                palette.highlightedText: BeyGramTheme.accentInk
                Accessible.name: qsTr("Quiet hours end")
                onTextEdited: root.quietInvalid = false
            }

            SecondaryButton {
                text: qsTr("Apply")
                enabled: root.notificationsOn
                onClicked: root.applyQuietHours()
            }

            SecondaryButton {
                text: qsTr("Off")
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
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            Accessible.name: text
        }
    }

    // Sound picker: "Silent" plus every sound the facade offers.
    Card {
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Notification sound")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("The sound plays for chats that are not muted.")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.soundLabels

                delegate: ChoiceChip {
                    required property int index
                    required property string modelData

                    text: modelData
                    selected: root.soundRow === index
                    enabled: root.ready
                    Accessible.name: modelData
                    onClicked: if (root.ready) root.facade.selectSound(index - 1)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.soundLabels.length <= 1
            text: qsTr("No sounds are offered yet.")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    // Per-chat overrides for the chat the parent loaded through
    // facade.loadChat().
    Card {
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("This chat")
            color: BeyGramTheme.text
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Overrides for the chat this screen loaded. They are sent when you save.")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            lineHeight: 1.3
        }

        CheckBox {
            id: chatMuteBox
            Layout.fillWidth: true
            text: qsTr("Mute notifications")
            enabled: root.ready
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.setChatMuted(checked)
        }

        CheckBox {
            id: chatSoundBox
            Layout.fillWidth: true
            text: qsTr("Play notification sound")
            enabled: root.ready && !Boolean(root.facade.chatMuted)
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.setChatSound(checked)
        }

        CheckBox {
            id: chatPreviewBox
            Layout.fillWidth: true
            text: qsTr("Show message preview")
            enabled: root.ready && !Boolean(root.facade.chatMuted)
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            Accessible.name: text
            onToggled: if (root.ready) root.facade.setChatPreview(checked)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            // Always enabled while a facade is present: saveChat() itself is a
            // no-op when the chat is clean, so this cannot send duplicates.
            PrimaryButton {
                text: qsTr("Save")
                enabled: root.ready
                onClicked: if (root.ready) root.facade.saveChat()
            }
        }
    }
}
