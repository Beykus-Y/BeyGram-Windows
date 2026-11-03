import QtQuick
import QtQuick.Layouts
import BeyGram.Theme 1.0

Rectangle {
    id: card
    default property alias contentData: column.data
    property int padding: 20
    property int spacing: 12

    radius: 18
    color: Qt.alpha(BeyGramTheme.surface, BeyGramTheme.darkMode ? 0.78 : 0.92)
    border.width: 1
    border.color: Qt.alpha(BeyGramTheme.line, 0.95)
    implicitHeight: column.implicitHeight + padding * 2
    implicitWidth: column.implicitWidth + padding * 2

    Behavior on color { ColorAnimation { duration: 180 } }

    // Top specular edge from the Liquid Glass material, kept subtle so it also
    // reads as a normal card in the other design styles.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        height: 1
        radius: 1
        color: Qt.alpha(BeyGramTheme.text, BeyGramTheme.darkMode ? 0.10 : 0.06)
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: card.padding
        spacing: card.spacing
    }
}
