// AppearancePanel - the appearance section of the settings screen.
//
// The parent owns the facade: it assigns `facade` (an AppearanceFacade) and this
// file only reads and writes its properties. The facade validates every write,
// so the panel never clamps, guesses or persists anything itself. There is no
// network, storage or logging here.
//
// All controls are guarded against a null facade, so the panel can be
// instantiated before the facade exists.
//
// Layout comes from the design system: a DsSection per group, a DsCard for each
// group, DsListRow with a DsSwitch for the toggles, DsSlider for the numeric
// settings, DsTextField for the background and DsDialog for the reset.

import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0
import "../ds/design.js" as Design

Item {
    id: root

    property var facade: null

    readonly property int minimumScale: 85
    readonly property int maximumScale: 150

    // Set once the controls exist. A facade assigned during creation would
    // otherwise make the sync handler touch ids that are not built yet.
    property bool ready: false

    implicitWidth: 440
    implicitHeight: content.implicitHeight

    Component.onCompleted: {
        root.ready = true
        root.syncFromFacade()
    }

    /// Rewrites the controls from the facade. Called whenever the facade
    /// announces a real change, so a reset or an external edit is visible
    /// without the panel keeping its own copy of the state. A control the user
    /// is currently using is left alone, because rewriting it would fight the
    /// edit in progress.
    function syncFromFacade() {
        if (!root.ready || !root.facade)
            return
        // The switches are assigned explicitly: a user toggle replaces their
        // `checked` binding, so a reset must write the switch back too.
        reduceMotionBox.checked = root.facade.reduceMotion
        highContrastBox.checked = root.facade.highContrast
        largeTextBox.checked = root.facade.largeText
        if (!scaleSlider.pressed)
            scaleSlider.value = root.facade.textScale
        if (!dimmerSlider.pressed)
            dimmerSlider.value = root.facade.dimmer
        if (!blurSlider.pressed)
            blurSlider.value = root.facade.blur
        if (!kindField.activeFocus)
            kindField.text = root.facade.backgroundKind
        if (!valueField.activeFocus)
            valueField.text = root.facade.backgroundValue
    }

    onFacadeChanged: root.syncFromFacade()

    /// Applies the two background fields. The value goes first: a value written
    /// while the kind is "none" selects a solid background, which is then
    /// narrowed to the requested kind. A blank or unknown kind falls back to
    /// "none", the only kind that needs no value.
    function commitBackground() {
        if (!root.facade)
            return

        const kind = kindField.text.trim().toLowerCase()
        const value = valueField.text.trim()

        if (kind.length === 0 || kind === "none") {
            root.facade.backgroundKind = "none"
            root.syncFromFacade()
            return
        }

        root.facade.backgroundValue = value
        root.facade.backgroundKind = kind
        root.syncFromFacade()
    }

    Connections {
        target: root.facade
        enabled: root.facade !== null
        function onChanged() {
            root.syncFromFacade()
        }
    }

    ColumnLayout {
        id: content
        width: root.width
        spacing: Design.space4

        // Accessibility ------------------------------------------------------

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Accessibility")
            description: qsTr("Adjust motion, contrast and text size for this device.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Reduce motion")
                        subtitle: qsTr("Trim animations across the interface.")

                        trailing: DsSwitch {
                            id: reduceMotionBox

                            checked: root.facade ? root.facade.reduceMotion : false
                            Accessible.name: qsTr("Reduce motion")

                            onToggled: {
                                if (root.facade)
                                    root.facade.reduceMotion = reduceMotionBox.checked
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("High contrast")
                        subtitle: qsTr("Stronger borders and separators.")

                        trailing: DsSwitch {
                            id: highContrastBox

                            checked: root.facade ? root.facade.highContrast : false
                            Accessible.name: qsTr("High contrast")

                            onToggled: {
                                if (root.facade)
                                    root.facade.highContrast = highContrastBox.checked
                            }
                        }
                    }

                    DsDivider {
                        Layout.fillWidth: true
                        inset: Design.space3
                    }

                    DsListRow {
                        Layout.fillWidth: true
                        title: qsTr("Large text")
                        subtitle: qsTr("Use a larger base size for message text.")

                        trailing: DsSwitch {
                            id: largeTextBox

                            checked: root.facade ? root.facade.largeText : false
                            Accessible.name: qsTr("Large text")

                            onToggled: {
                                if (root.facade)
                                    root.facade.largeText = largeTextBox.checked
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

                        DsSlider {
                            id: scaleSlider

                            Layout.fillWidth: true
                            label: qsTr("Text scale")
                            valueText: (root.facade ? root.facade.textScale : 100) + "%"
                            from: root.minimumScale
                            to: root.maximumScale
                            stepSize: 1
                            value: root.facade ? root.facade.textScale : 100

                            onMoved: {
                                if (root.facade)
                                    root.facade.textScale = Math.round(value)
                            }
                        }
                    }
                }
            }
        }

        // Chat background ----------------------------------------------------

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Chat background")
            description: qsTr("Kinds: none, solid, gradient, pattern, wallpaper. A background needs a value.")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsTextField {
                            id: kindField

                            Layout.preferredWidth: 150
                            label: qsTr("Kind")
                            placeholderText: "none"
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                            Accessible.name: qsTr("Background kind")

                            onEditingFinished: root.commitBackground()
                        }

                        DsTextField {
                            id: valueField

                            Layout.fillWidth: true
                            label: qsTr("Value")
                            placeholderText: qsTr("#1b2836 or an image path")
                            Accessible.name: qsTr("Background value")

                            onEditingFinished: root.commitBackground()
                        }
                    }

                    DsSlider {
                        id: dimmerSlider

                        Layout.fillWidth: true
                        label: qsTr("Dimmer")
                        valueText: (root.facade ? root.facade.dimmer : 0) + "%"
                        from: 0
                        to: 100
                        stepSize: 1
                        value: root.facade ? root.facade.dimmer : 0

                        onMoved: {
                            if (root.facade)
                                root.facade.dimmer = Math.round(value)
                        }
                    }

                    DsSlider {
                        id: blurSlider

                        Layout.fillWidth: true
                        label: qsTr("Blur")
                        valueText: (root.facade ? root.facade.blur : 0) + "%"
                        from: 0
                        to: 100
                        stepSize: 1
                        value: root.facade ? root.facade.blur : 0

                        onMoved: {
                            if (root.facade)
                                root.facade.blur = Math.round(value)
                        }
                    }
                }
            }
        }

        // Language -----------------------------------------------------------

        DsSection {
            Layout.fillWidth: true
            title: qsTr("Language")

            content: DsCard {
                Layout.fillWidth: true

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        Layout.margins: Design.space3
                        visible: languageList.count === 0
                        text: qsTr("No language packs are offered yet.")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: languageList

                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, 240)
                        visible: count > 0
                        clip: true
                        spacing: 0
                        model: root.facade ? root.facade.languageNames : []

                        delegate: DsListRow {
                            id: languageRow

                            required property int index
                            required property string modelData

                            readonly property bool isCurrent: root.facade !== null
                                                              && root.facade.languageIndex === index

                            width: languageList.width
                            title: modelData
                            selected: isCurrent
                            hoverEnabled: true

                            trailing: DsBadge {
                                visible: languageRow.isCurrent
                                text: qsTr("Current")
                                tone: "accent"
                            }

                            onClicked: {
                                if (root.facade)
                                    root.facade.selectLanguage(index)
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.space2

            Item { Layout.fillWidth: true }

            DsButton {
                text: qsTr("Reset appearance")
                tone: "danger"
                enabled: root.facade !== null
                onClicked: resetDialog.open()
            }
        }
    }

    // A reset discards the custom scale, contrast, background and language, so
    // it is confirmed before the facade is asked to do it.
    DsDialog {
        id: resetDialog

        title: qsTr("Reset appearance?")
        description: qsTr("Text scale, contrast, the chat background and the language go back to their defaults.")
        primaryText: qsTr("Reset")
        secondaryText: qsTr("Cancel")
        tone: "danger"

        onAccepted: {
            if (root.facade)
                root.facade.reset()
        }
    }
}
