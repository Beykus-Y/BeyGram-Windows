// AppearancePanel - the appearance section of the settings screen.
//
// The parent owns the facade: it assigns `facade` (an AppearanceFacade) and this
// file only reads and writes its properties. The facade validates every write,
// so the panel never clamps, guesses or persists anything itself. There is no
// network, storage or logging here.
//
// All controls are guarded against a null facade, so the panel can be
// instantiated before the facade exists.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

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
        // `checked` binding, so a reset must write the box back too.
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
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: qsTr("Accessibility")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        CheckBox {
            id: reduceMotionBox
            Layout.fillWidth: true
            text: qsTr("Reduce motion")
            checked: root.facade ? root.facade.reduceMotion : false
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: {
                if (root.facade)
                    root.facade.reduceMotion = checked
            }
            Accessible.name: text
        }

        CheckBox {
            id: highContrastBox
            Layout.fillWidth: true
            text: qsTr("High contrast")
            checked: root.facade ? root.facade.highContrast : false
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: {
                if (root.facade)
                    root.facade.highContrast = checked
            }
            Accessible.name: text
        }

        CheckBox {
            id: largeTextBox
            Layout.fillWidth: true
            text: qsTr("Large text")
            checked: root.facade ? root.facade.largeText : false
            palette.text: BeyGramTheme.text
            palette.windowText: BeyGramTheme.text
            palette.buttonText: BeyGramTheme.text
            onToggled: {
                if (root.facade)
                    root.facade.largeText = checked
            }
            Accessible.name: text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Text scale")
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }

            Slider {
                id: scaleSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                from: root.minimumScale
                to: root.maximumScale
                stepSize: 1
                value: root.facade ? root.facade.textScale : 100
                onMoved: {
                    if (root.facade)
                        root.facade.textScale = Math.round(value)
                }
                Accessible.name: qsTr("Text scale")

                background: Rectangle {
                    x: scaleSlider.leftPadding
                    y: scaleSlider.topPadding
                       + scaleSlider.availableHeight / 2 - height / 2
                    width: scaleSlider.availableWidth
                    height: 4
                    radius: 2
                    color: BeyGramTheme.line

                    Rectangle {
                        width: scaleSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: BeyGramTheme.accent
                    }
                }

                handle: Rectangle {
                    x: scaleSlider.leftPadding
                       + scaleSlider.visualPosition
                         * (scaleSlider.availableWidth - width)
                    y: scaleSlider.topPadding
                       + scaleSlider.availableHeight / 2 - height / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: BeyGramTheme.accent
                    border.width: 2
                    border.color: BeyGramTheme.surface
                }
            }

            Text {
                Layout.preferredWidth: 40
                text: (root.facade ? root.facade.textScale : 100) + "%"
                color: BeyGramTheme.muted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 2
            implicitHeight: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Chat background")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            TextField {
                id: kindField
                Layout.preferredWidth: 130
                text: root.facade ? root.facade.backgroundKind : "none"
                placeholderText: qsTr("Kind")
                color: BeyGramTheme.text
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: 13
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onEditingFinished: root.commitBackground()
                Accessible.name: qsTr("Background kind")

                background: Rectangle {
                    radius: 10
                    color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                    border.width: kindField.activeFocus ? 2 : 1
                    border.color: kindField.activeFocus
                                  ? BeyGramTheme.accent
                                  : Qt.alpha(BeyGramTheme.line, 1.0)
                }
            }

            TextField {
                id: valueField
                Layout.fillWidth: true
                text: root.facade ? root.facade.backgroundValue : ""
                placeholderText: qsTr("Value, e.g. #1b2836 or an image path")
                color: BeyGramTheme.text
                placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
                selectionColor: BeyGramTheme.accent
                selectedTextColor: BeyGramTheme.onAccent
                font.pixelSize: 13
                onEditingFinished: root.commitBackground()
                Accessible.name: qsTr("Background value")

                background: Rectangle {
                    radius: 10
                    color: Qt.alpha(BeyGramTheme.surface2, 0.75)
                    border.width: valueField.activeFocus ? 2 : 1
                    border.color: valueField.activeFocus
                                  ? BeyGramTheme.accent
                                  : Qt.alpha(BeyGramTheme.line, 1.0)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Kinds: none, solid, gradient, pattern, wallpaper. A background needs a value.")
            color: Qt.alpha(BeyGramTheme.muted, 0.85)
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Dimmer")
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }

            Slider {
                id: dimmerSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                from: 0
                to: 100
                stepSize: 1
                value: root.facade ? root.facade.dimmer : 0
                onMoved: {
                    if (root.facade)
                        root.facade.dimmer = Math.round(value)
                }
                Accessible.name: qsTr("Background dimmer")

                background: Rectangle {
                    x: dimmerSlider.leftPadding
                    y: dimmerSlider.topPadding
                       + dimmerSlider.availableHeight / 2 - height / 2
                    width: dimmerSlider.availableWidth
                    height: 4
                    radius: 2
                    color: BeyGramTheme.line

                    Rectangle {
                        width: dimmerSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: BeyGramTheme.accent
                    }
                }

                handle: Rectangle {
                    x: dimmerSlider.leftPadding
                       + dimmerSlider.visualPosition
                         * (dimmerSlider.availableWidth - width)
                    y: dimmerSlider.topPadding
                       + dimmerSlider.availableHeight / 2 - height / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: BeyGramTheme.accent
                    border.width: 2
                    border.color: BeyGramTheme.surface
                }
            }

            Text {
                Layout.preferredWidth: 40
                text: (root.facade ? root.facade.dimmer : 0) + "%"
                color: BeyGramTheme.muted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: qsTr("Blur")
                color: BeyGramTheme.muted
                font.pixelSize: 11
            }

            Slider {
                id: blurSlider
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                from: 0
                to: 100
                stepSize: 1
                value: root.facade ? root.facade.blur : 0
                onMoved: {
                    if (root.facade)
                        root.facade.blur = Math.round(value)
                }
                Accessible.name: qsTr("Background blur")

                background: Rectangle {
                    x: blurSlider.leftPadding
                    y: blurSlider.topPadding
                       + blurSlider.availableHeight / 2 - height / 2
                    width: blurSlider.availableWidth
                    height: 4
                    radius: 2
                    color: BeyGramTheme.line

                    Rectangle {
                        width: blurSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: BeyGramTheme.accent
                    }
                }

                handle: Rectangle {
                    x: blurSlider.leftPadding
                       + blurSlider.visualPosition
                         * (blurSlider.availableWidth - width)
                    y: blurSlider.topPadding
                       + blurSlider.availableHeight / 2 - height / 2
                    width: 14
                    height: 14
                    radius: 7
                    color: BeyGramTheme.accent
                    border.width: 2
                    border.color: BeyGramTheme.surface
                }
            }

            Text {
                Layout.preferredWidth: 40
                text: (root.facade ? root.facade.blur : 0) + "%"
                color: BeyGramTheme.muted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 2
            implicitHeight: 1
            color: Qt.alpha(BeyGramTheme.line, 0.9)
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Language")
            color: BeyGramTheme.muted
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            visible: languageList.count === 0
            text: qsTr("No language packs are offered yet.")
            color: Qt.alpha(BeyGramTheme.muted, 0.85)
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        ListView {
            id: languageList
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 180)
            visible: count > 0
            clip: true
            spacing: 2
            model: root.facade ? root.facade.languageNames : []

            delegate: Rectangle {
                required property int index
                required property string modelData

                readonly property bool isCurrent: root.facade !== null
                                                  && root.facade.languageIndex === index

                width: languageList.width
                height: 34
                radius: 8
                color: isCurrent ? Qt.alpha(BeyGramTheme.accent, 0.18) : "transparent"
                border.width: isCurrent ? 1 : 0
                border.color: Qt.alpha(BeyGramTheme.accent, 0.55)

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.modelData
                    color: parent.isCurrent ? BeyGramTheme.accentInk : BeyGramTheme.text
                    font.pixelSize: 13
                    font.weight: parent.isCurrent ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: parent.modelData
                    Accessible.checked: parent.isCurrent
                    onClicked: {
                        if (root.facade)
                            root.facade.selectLanguage(index)
                    }
                }
            }
        }

        SecondaryButton {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 4
            text: qsTr("Reset appearance")
            enabled: root.facade !== null
            onClicked: {
                if (root.facade)
                    root.facade.reset()
            }
        }
    }
}
