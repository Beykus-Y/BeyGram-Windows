// ContactsPanel.qml — the contact list with its search field and A-Z strip.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0

Item {
    id: root

    property var facade: null

    implicitWidth: 360
    implicitHeight: 560

    readonly property var contactNames: root.facade ? root.facade.names : []
    readonly property var letters: root.facade ? root.facade.indexLetters : []
    readonly property bool loading: root.facade ? root.facade.loading : false
    readonly property bool filtering: root.facade ? root.facade.query.length > 0 : false

    readonly property var avatarColors: [
        "#C2593F", "#5B4BC4", "#2F8A7B",
        "#A9731D", "#3A78C2", "#B0476E"
    ]

    function initialOf(name) {
        if (!name)
            return ""
        const value = String(name).trim()
        if (value.length === 0)
            return ""
        return value.charAt(0).toUpperCase()
    }

    function applyQuery(text) {
        if (!root.facade)
            return
        if (root.facade.query !== String(text).trim())
            root.facade.query = text
    }

    function activate(index) {
        if (root.facade)
            root.facade.activate(index)
    }

    function scrollToLetter(letter) {
        const items = root.contactNames
        for (let i = 0; i < items.length; ++i) {
            if (root.initialOf(items[i]) === letter) {
                contactList.positionViewAtIndex(i, ListView.Beginning)
                return
            }
        }
    }

    Connections {
        target: root.facade
        ignoreUnknownSignals: true

        function onChanged() {
            if (!root.facade)
                return
            if (searchField.text.trim() !== root.facade.query)
                searchField.text = root.facade.query
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TextField {
            id: searchField

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.preferredHeight: 38
            implicitHeight: 38
            leftPadding: 14
            rightPadding: 14
            placeholderText: qsTr("Search contacts")
            placeholderTextColor: Qt.alpha(BeyGramTheme.muted, 0.75)
            color: BeyGramTheme.text
            font.pixelSize: 13
            selectByMouse: true
            selectionColor: BeyGramTheme.accent
            selectedTextColor: BeyGramTheme.onAccent

            onTextEdited: root.applyQuery(text)

            Keys.onEscapePressed: (event) => {
                if (searchField.text.length > 0) {
                    searchField.text = ""
                    root.applyQuery("")
                    event.accepted = true
                } else {
                    event.accepted = false
                }
            }

            background: Rectangle {
                radius: 19
                color: BeyGramTheme.surface2
                border.width: 1
                border.color: searchField.activeFocus ? BeyGramTheme.accent
                                                      : Qt.alpha(BeyGramTheme.line, 0.9)
                Behavior on border.color { ColorAnimation { duration: 120 } }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6

            ListView {
                id: contactList

                anchors.fill: parent
                anchors.rightMargin: root.letters.length > 0 ? 24 : 0
                clip: true
                model: root.contactNames
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: entry

                    required property var modelData
                    required property int index

                    readonly property string contactName: entry.modelData === undefined
                                                         || entry.modelData === null
                                                         ? "" : String(entry.modelData)
                    readonly property string letter: root.initialOf(entry.contactName)
                    readonly property bool showsHeader: entry.index === 0
                        || root.initialOf(root.contactNames[entry.index - 1]) !== entry.letter

                    width: contactList.width
                    height: header.height + body.height

                    Text {
                        id: header

                        width: parent.width
                        height: entry.showsHeader ? 26 : 0
                        visible: entry.showsHeader
                        leftPadding: 18
                        verticalAlignment: Text.AlignBottom
                        text: entry.letter
                        color: BeyGramTheme.muted
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                    Item {
                        id: body

                        anchors.top: header.bottom
                        width: parent.width
                        height: 44

                        Accessible.role: Accessible.Button
                        Accessible.name: entry.contactName.length > 0
                                         ? entry.contactName : qsTr("Unnamed contact")

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            radius: 10
                            color: entryMouse.containsMouse ? BeyGramTheme.surface2
                                                            : "transparent"
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 15
                                color: root.avatarColors[Math.abs(entry.index)
                                                        % root.avatarColors.length]

                                Text {
                                    anchors.centerIn: parent
                                    text: entry.letter
                                    color: "white"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: entry.contactName.length > 0
                                      ? entry.contactName : qsTr("Unnamed contact")
                                color: entry.contactName.length > 0 ? BeyGramTheme.text
                                                                    : BeyGramTheme.muted
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        MouseArea {
                            id: entryMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activate(entry.index)
                        }
                    }
                }
            }

            Flickable {
                id: strip

                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: Math.min(stripColumn.height, Math.max(0, parent.height - 16))
                contentWidth: width
                contentHeight: stripColumn.height
                clip: true
                interactive: contentHeight > height
                boundsBehavior: Flickable.StopAtBounds
                visible: root.letters.length > 0

                Column {
                    id: stripColumn

                    width: strip.width

                    Repeater {
                        model: root.letters

                        delegate: Item {
                            id: letterCell

                            required property var modelData

                            readonly property string letter: letterCell.modelData === undefined
                                                             || letterCell.modelData === null
                                                             ? "" : String(letterCell.modelData)

                            width: strip.width
                            height: 18

                            Accessible.role: Accessible.Button
                            Accessible.name: qsTr("Jump to %1").arg(letterCell.letter)

                            Text {
                                anchors.centerIn: parent
                                text: letterCell.letter
                                color: letterMouse.containsMouse ? BeyGramTheme.accent
                                                                 : BeyGramTheme.muted
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }

                            MouseArea {
                                id: letterMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.scrollToLetter(letterCell.letter)
                            }
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.loading && contactList.count === 0
                visible: running
                implicitWidth: 32
                implicitHeight: 32
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(280, parent.width - 48))
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: !root.loading && contactList.count === 0 && root.filtering
                text: qsTr("No contacts match your search")
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }

            Text {
                anchors.centerIn: parent
                width: Math.max(0, Math.min(280, parent.width - 48))
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: !root.loading && contactList.count === 0 && !root.filtering
                text: qsTr("No contacts yet")
                color: BeyGramTheme.muted
                font.pixelSize: 13
            }
        }
    }
}
