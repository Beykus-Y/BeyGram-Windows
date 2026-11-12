import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

// Single-line text field for the BeyGram design system.
//
// The input box itself is exactly Design.controlHeight tall. An optional
// caption floats above it and an optional error line sits below, so the
// component's implicitHeight grows by those rows while the box keeps the
// token height. All string inputs are normalised, so a null/undefined label,
// error, icon or trailing text simply renders nothing.
TextField {
    id: control

    // --- Public API ---------------------------------------------------------
    property string label: ""
    property string error: ""
    property string leadingIcon: ""
    property string trailingText: ""
    property bool clearable: false

    /// Emitted when the clear button empties the field.
    signal cleared()

    /// Empties the field, keeps focus and emits `cleared()`.
    function clearText() {
        control.text = ""
        control.forceActiveFocus()
        control.cleared()
    }

    // --- Null-safe normalisation -------------------------------------------
    readonly property string _labelText: control.label ? control.label : ""
    readonly property string _errorText: control.error ? control.error : ""
    readonly property string _leadingIcon: control.leadingIcon ? control.leadingIcon : ""
    readonly property string _trailingText: control.trailingText ? control.trailingText : ""

    readonly property bool _hasLabel: _labelText.length > 0
    readonly property bool _hasError: _errorText.length > 0
    readonly property bool _hasLeading: _leadingIcon.length > 0
    readonly property bool _hasClear: control.clearable === true
                                       && (control.text || "").length > 0
                                       && !control.readOnly

    // Caption / helper rows around the 40 px box.
    readonly property real _labelRowHeight: _hasLabel ? labelText.implicitHeight + Design.space1 : 0
    readonly property real _helperRowHeight: _hasError ? helperText.implicitHeight + Design.space1 : 0

    // Width reserved on the right for the trailing text and clear button.
    // Computed from content (not from an item's effective visibility) so the
    // padding stays correct even while the field is inside a hidden container.
    readonly property real _trailingReserve: {
        let reserve = 0
        if (_trailingText.length > 0)
            reserve += trailingTextItem.implicitWidth
        if (_hasClear) {
            if (reserve > 0)
                reserve += Design.space2
            reserve += 20
        }
        return reserve
    }

    // Keeps a 14 px line vertically centred inside the 40 px box.
    readonly property real _verticalInset: Math.max(0, Math.round((Design.controlHeight - font.pixelSize * 1.45) / 2))

    implicitWidth: 260
    implicitHeight: _labelRowHeight + Design.controlHeight + _helperRowHeight

    leftPadding: Design.space3 + (_hasLeading ? leadingIconText.implicitWidth + Design.space2 : 0)
    rightPadding: Design.space3 + (_trailingReserve > 0 ? _trailingReserve + Design.space2 : 0)
    topPadding: _labelRowHeight + _verticalInset
    bottomPadding: _helperRowHeight + _verticalInset

    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    color: BeyGramTheme.text
    placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
    selectionColor: BeyGramTheme.accent
    selectedTextColor: BeyGramTheme.onAccent
    font.pixelSize: Design.fontBody

    // --- Box -----------------------------------------------------------------
    // The Control resizes the background item to the whole component, so the
    // visible box is a child offset below the caption row.
    background: Item {
        Rectangle {
            y: control._labelRowHeight
            width: parent.width
            height: Design.controlHeight
            radius: Design.radiusSm
            color: BeyGramTheme.surface2
            border.width: 1
            border.color: control._hasError
                          ? BeyGramTheme.danger
                          : control.activeFocus
                            ? BeyGramTheme.accent
                            : BeyGramTheme.line
            Behavior on border.color { ColorAnimation { duration: Design.motionFast } }
        }
    }

    // --- Floating caption ----------------------------------------------------
    Text {
        id: labelText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: control._hasLabel
        text: control._labelText
        color: control._hasError
               ? BeyGramTheme.danger
               : control.activeFocus
                 ? BeyGramTheme.accent
                 : BeyGramTheme.muted
        font.pixelSize: Design.fontCaption
        font.weight: Font.DemiBold
        font.letterSpacing: 0.3
        elide: Text.ElideRight
        Behavior on color { ColorAnimation { duration: Design.motionFast } }
    }

    // --- Red helper line -----------------------------------------------------
    Text {
        id: helperText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: control._labelRowHeight + Design.controlHeight + Design.space1
        visible: control._hasError
        text: control._errorText
        color: BeyGramTheme.danger
        font.pixelSize: Design.fontCaption
        elide: Text.ElideRight
    }

    // --- Leading icon / trailing text / clear button ------------------------
    Item {
        id: fieldOverlay
        z: 2
        x: 0
        y: control._labelRowHeight
        width: control.width
        height: Design.controlHeight

        Text {
            id: leadingIconText
            anchors.left: parent.left
            anchors.leftMargin: Design.space3
            anchors.verticalCenter: parent.verticalCenter
            visible: control._hasLeading
            text: control._leadingIcon
            color: control._hasError
                   ? BeyGramTheme.danger
                   : control.activeFocus
                     ? BeyGramTheme.accent
                     : BeyGramTheme.muted
            font.pixelSize: Design.iconSize
            Behavior on color { ColorAnimation { duration: Design.motionFast } }
        }

        Row {
            id: trailingRow
            anchors.right: parent.right
            anchors.rightMargin: Design.space3
            anchors.verticalCenter: parent.verticalCenter
            height: 20
            spacing: Design.space2
            visible: control._trailingText.length > 0 || control._hasClear

            Item {
                visible: control._trailingText.length > 0
                width: trailingTextItem.implicitWidth
                height: 20

                Text {
                    id: trailingTextItem
                    anchors.verticalCenter: parent.verticalCenter
                    text: control._trailingText
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.2
                }
            }

            Item {
                visible: control._hasClear
                width: 20
                height: 20

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: clearMouse.containsMouse
                           ? Qt.alpha(BeyGramTheme.muted, Design.opacityHover)
                           : "transparent"
                    Behavior on color { ColorAnimation { duration: Design.motionFast } }
                }

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontBody
                }

                MouseArea {
                    id: clearMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: control.clearText()
                }
            }
        }
    }
}
