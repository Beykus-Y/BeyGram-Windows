import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 860
    minimumWidth: 980
    minimumHeight: 680
    title: BeyGramApp.ready ? "BeyGram Windows" : "BeyGram Windows — Sign in"
    color: BeyGramTheme.bg

    Behavior on color { ColorAnimation { duration: 180 } }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: BeyGramApp.refresh()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: BeyGramTheme.toggleDarkMode()
    }

    // Ambient wash from the Android design system: two soft accent fields over
    // the plain screen colour. No backdrop is sampled, so nothing is faked.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.alpha(BeyGramTheme.accent, 0.10) }
            GradientStop { position: 0.45; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.alpha(BeyGramTheme.accent, 0.06) }
        }
        opacity: BeyGramTheme.darkMode ? 0.55 : 0.9
    }

    Loader {
        anchors.fill: parent
        sourceComponent: BeyGramApp.ready ? shellComponent : authComponent

        Component {
            id: shellComponent
            Shell {}
        }

        Component {
            id: authComponent
            AuthPage {}
        }
    }
}
