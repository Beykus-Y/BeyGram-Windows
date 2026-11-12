// DsSlider.qml — horizontal slider with an optional label/value header.
//
// The track is 4 px, the filled portion and the 16 px handle use the accent
// colour, and the handle has a 2 px surface ring so it separates cleanly from
// the fill. `from`, `to`, `value` and `stepSize` are inherited Slider API;
// set `label` and/or `valueText` to show the small header above the track.

import QtQuick
import QtQuick.Controls
import BeyGram.Theme 1.0
import "design.js" as Design

Slider {
    id: root

    property string label: ""
    property string valueText: ""
    property int trackThickness: 4
    property int handleSize: 16

    readonly property bool hasHeader: root.label.length > 0 || root.valueText.length > 0
    readonly property real headerHeight: root.hasHeader ? 16 + Design.space2 : 0

    hoverEnabled: true
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitWidth: 240
    implicitHeight: root.headerHeight + root.handleSize + 6
    opacity: root.enabled ? 1.0 : Design.opacityDisabled
    activeFocusOnTab: true
    Accessible.name: root.label.length > 0 ? root.label : "Slider"

    Item {
        id: header

        visible: root.hasHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 16

        Text {
            id: labelText

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.width - valueLabel.implicitWidth
                            - (valueLabel.visible ? Design.space2 : 0))
            text: root.label
            visible: root.label.length > 0
            color: BeyGramTheme.muted
            font.pixelSize: Design.fontSmall
            elide: Text.ElideRight
        }

        Text {
            id: valueLabel

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.valueText
            visible: root.valueText.length > 0
            color: BeyGramTheme.text
            font.pixelSize: Design.fontSmall
            font.weight: Font.DemiBold
        }
    }

    background: Item {
        x: 0
        y: root.headerHeight
        width: root.width
        height: Math.max(0, root.height - root.headerHeight)

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: root.trackThickness
            radius: height / 2
            color: BeyGramTheme.surface2
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, root.visualPosition * parent.width)
            height: root.trackThickness
            radius: height / 2
            color: BeyGramTheme.accent
        }
    }

    handle: Rectangle {
        width: root.handleSize
        height: root.handleSize
        radius: width / 2
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.headerHeight + (root.height - root.headerHeight) / 2 - height / 2
        color: BeyGramTheme.accent
        border.width: 2
        border.color: BeyGramTheme.surface

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 6
            height: parent.height + 6
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: BeyGramTheme.accent
            visible: root.visualFocus
        }
    }
}
