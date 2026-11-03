// Static chat wallpaper painted with QML Canvas primitives only.
//
// The Android app is the source reference: the palettes below are copied from
// `data/appearance/WallpaperPresets.kt` and the sparse motif geometry follows
// `ui/screens/chat/PatternWallpaper.kt`. Everything is resolution independent
// (the painting is redone per item size), self-contained (no BeyGram.Theme
// import, no image assets, no network, no timers) and drawn exactly once per
// size/preset/mode change.
//
// AMOLED stays true black: all three gradient stops collapse to #000000 and the
// motif is skipped, so black pixels remain black instead of picking up a tint.
//
// Motifs use only waves, arches and dots. Android presets whose motif shares a
// primitive reuse it: mint-air (Leaves) draws arches, sunset-peach (Doodles)
// and soft-rose (Orbits) draw dots.

import QtQuick

Item {
    id: root

    property bool darkMode: false
    property bool amoled: false
    property string preset: "lavender-flow"

    anchors.fill: parent

    // Palette table mirrored 1:1 from Android's WallpaperPresets so the Windows
    // build needs no theme singleton and can be previewed on its own.
    readonly property var palette: ({
        "lavender-flow": {
            light: ["#F9F4FF", "#F1E7FF", "#E8E4FF"],
            dark: ["#18132B", "#272040", "#19172F"],
            lightInk: "#6A4FBB",
            darkInk: "#C5ACFF",
            motif: "waves"
        },
        "arctic-blue": {
            light: ["#F3FAFF", "#DFEFFF", "#CFE7F8"],
            dark: ["#0E1B2C", "#122C46", "#172033"],
            lightInk: "#4476AB",
            darkInk: "#9CCEF9",
            motif: "arches"
        },
        "mint-air": {
            light: ["#F1FFF8", "#DDF6E8", "#D0ECE2"],
            dark: ["#102821", "#173A33", "#102B27"],
            lightInk: "#3A9376",
            darkInk: "#94E2BB",
            motif: "arches"
        },
        "sunset-peach": {
            light: ["#FFF5EB", "#FFDDCD", "#F7D4E3"],
            dark: ["#32212A", "#482D3B", "#2C2134"],
            lightInk: "#CE7B75",
            darkInk: "#FFB6AB",
            motif: "dots"
        },
        "soft-rose": {
            light: ["#FFF5FA", "#F8E0ED", "#EADFF7"],
            dark: ["#2B1728", "#3C203B", "#271F39"],
            lightInk: "#AE5D8E",
            darkInk: "#FFADD4",
            motif: "dots"
        },
        "graphite-wave": {
            light: ["#F5F6F8", "#E6E9F0", "#DDE1EA"],
            dark: ["#15171E", "#20242E", "#191C26"],
            lightInk: "#69748B",
            darkInk: "#AAB8CF",
            motif: "waves"
        }
    })

    // Unknown or missing ids fall back to lavender-flow instead of failing.
    readonly property var spec: {
        var candidate = palette[preset]
        return candidate !== undefined && candidate.light !== undefined
                ? candidate
                : palette["lavender-flow"]
    }

    readonly property var stops: amoled
                                  ? ["#000000", "#000000", "#000000"]
                                  : (darkMode ? spec.dark : spec.light)

    readonly property color ink: darkMode ? spec.darkInk : spec.lightInk

    // Android limits the ornament to 0.33 alpha; a wallpaper behind message
    // bubbles reads best at the quiet end of that range.
    readonly property real motifAlpha: 0.08

    // Repaint is explicit: nothing animates, so the canvas content stays cached.
    onPresetChanged: canvas.requestPaint()
    onDarkModeChanged: canvas.requestPaint()
    onAmoledChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas

        // Same spacing as Android's 112.dp motif grid.
        readonly property real cellSize: 112

        anchors.fill: parent
        clip: true

        Component.onCompleted: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            paint(ctx, width, height)
        }

        function paint(ctx, w, h) {
            if (w <= 0 || h <= 0)
                return

            var gradient = ctx.createLinearGradient(0, 0, 0, h)
            gradient.addColorStop(0.0, root.stops[0])
            gradient.addColorStop(0.5, root.stops[1])
            gradient.addColorStop(1.0, root.stops[2])
            ctx.fillStyle = gradient
            ctx.fillRect(0, 0, w, h)

            if (root.amoled)
                return

            var ink = root.ink
            var inkStyle = "rgba(" + Math.round(ink.r * 255) + ", "
                         + Math.round(ink.g * 255) + ", "
                         + Math.round(ink.b * 255) + ", "
                         + root.motifAlpha + ")"

            ctx.save()
            ctx.strokeStyle = inkStyle
            ctx.fillStyle = inkStyle
            ctx.lineWidth = 1.35
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (root.spec.motif === "waves")
                paintWaves(ctx, w, h)
            else if (root.spec.motif === "arches")
                paintArches(ctx, w, h)
            else
                paintDots(ctx, w, h)

            ctx.restore()
        }

        function paintWaves(ctx, w, h) {
            var cell = cellSize
            var rows = Math.ceil(h / cell) + 1

            for (var row = 0; row <= rows * 2; ++row) {
                var y = row * cell * 0.57
                ctx.beginPath()
                ctx.moveTo(-cell, y)
                for (var x = -cell; x < w + cell; x += cell) {
                    ctx.bezierCurveTo(x + cell * 0.27, y - cell * 0.30,
                                      x + cell * 0.73, y + cell * 0.30,
                                      x + cell, y)
                }
                ctx.stroke()
            }
        }

        function paintArches(ctx, w, h) {
            var cell = cellSize
            var columns = Math.ceil(w / cell) + 1
            var rows = Math.ceil(h / cell) + 1

            for (var row = 0; row <= rows; ++row) {
                for (var col = 0; col <= columns; ++col) {
                    // Every other row is offset by half a cell, as on Android.
                    var x = col * cell + (row % 2 === 0 ? 0 : cell * 0.5)
                    var y = row * cell

                    ctx.beginPath()
                    ctx.ellipse(x + cell * 0.48, y + cell * 0.54,
                                cell * 0.35, cell * 0.36,
                                0, Math.PI, Math.PI * 2)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.ellipse(x + cell * 0.485, y + cell * 0.535,
                                cell * 0.185, cell * 0.175,
                                0, Math.PI, Math.PI * 2)
                    ctx.stroke()
                }
            }
        }

        function paintDots(ctx, w, h) {
            var cell = cellSize
            var columns = Math.ceil(w / cell) + 1
            var rows = Math.ceil(h / cell) + 1

            for (var row = 0; row <= rows; ++row) {
                for (var col = 0; col <= columns; ++col) {
                    var x = col * cell + (row % 2 === 0 ? 0 : cell * 0.5)
                    var y = row * cell

                    ctx.beginPath()
                    ctx.arc(x + cell * 0.32, y + cell * 0.30, cell * 0.065, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.beginPath()
                    ctx.arc(x + cell * 0.78, y + cell * 0.70, cell * 0.017, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.beginPath()
                    ctx.arc(x + cell * 0.17, y + cell * 0.80, cell * 0.025, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
    }
}
