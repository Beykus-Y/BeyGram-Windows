import QtQuick
import BeyGram.Theme 1.0

// Indeterminate 24 px arc spinner. The Canvas draws the ring once; a
// RotationAnimator spins the whole item, which keeps the animation cheap.
Item {
    id: root

    property bool running: true
    property color color: BeyGramTheme.accent
    property real lineWidth: 2

    readonly property int diameter: 24

    implicitWidth: diameter
    implicitHeight: diameter
    width: implicitWidth
    height: implicitHeight

    Accessible.role: Accessible.ProgressBar
    Accessible.name: "Loading"

    onColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            const size = Math.min(width, height)
            const lw = root.lineWidth
            const r = Math.max(0, size / 2 - lw / 2)
            const cx = width / 2
            const cy = height / 2

            ctx.clearRect(0, 0, width, height)
            ctx.lineWidth = lw
            ctx.lineCap = "round"

            // Faint track so the 90° gap reads as an arc, not a broken ring.
            ctx.beginPath()
            ctx.strokeStyle = Qt.rgba(root.color.r, root.color.g, root.color.b, 0.16)
            ctx.arc(cx, cy, r, 0, Math.PI * 2, false)
            ctx.stroke()

            // 270° arc, rotating with the item.
            ctx.beginPath()
            ctx.strokeStyle = root.color
            ctx.arc(cx, cy, r, -Math.PI / 2, Math.PI, false)
            ctx.stroke()
        }

        RotationAnimator on rotation {
            running: root.running
            from: 0
            to: 360
            duration: 900
            loops: Animation.Infinite
        }
    }
}
