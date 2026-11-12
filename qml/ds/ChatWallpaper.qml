import QtQuick
import BeyGram.Theme 1.0

// Deterministic vector wallpaper matching Android's `PatternWallpaper.kt` and
// `WallpaperPresets.kt`. Renders soft theme-aware gradients + subtle pools +
// sparse vector motifs without per-frame allocations or external image assets.
Item {
    id: root

    property string presetId: "none"
    property int intensity: 24
    property bool amoled: BeyGramTheme.modeName === "Amoled"
    property bool dark: BeyGramTheme.darkMode

    readonly property var presets: [
        { id: "none", motif: "none", light: [], dark: [], inkLight: "#6A4FDC", inkDark: "#A894FF" },
        { id: "lavender-flow", motif: "waves", light: ["#F9F4FF", "#F1E7FF", "#E8E4FF"], dark: ["#18132B", "#272040", "#19172F"], inkLight: "#6A4FBB", inkDark: "#C5ACFF" },
        { id: "arctic-blue", motif: "arches", light: ["#F3FAFF", "#DFEFFF", "#CFE7F8"], dark: ["#0E1B2C", "#122C46", "#172033"], inkLight: "#4476AB", inkDark: "#9CCEF9" },
        { id: "mint-air", motif: "leaves", light: ["#F1FFF8", "#DDF6E8", "#D0ECE2"], dark: ["#102821", "#173A33", "#102B27"], inkLight: "#3A9376", inkDark: "#94E2BB" },
        { id: "sunset-peach", motif: "doodles", light: ["#FFF5EB", "#FFDDCD", "#F7D4E3"], dark: ["#32212A", "#482D3B", "#2C2134"], inkLight: "#CE7B75", inkDark: "#FFB6AB" },
        { id: "soft-rose", motif: "orbits", light: ["#FFF5FA", "#F8E0ED", "#EADFF7"], dark: ["#2B1728", "#3C203B", "#271F39"], inkLight: "#AE5D8E", inkDark: "#FFADD4" },
        { id: "blue-notes", motif: "doodles", light: ["#F0F5FF", "#DDE9FF", "#E2E1FA"], dark: ["#15223E", "#202C55", "#181D3C"], inkLight: "#516DB5", inkDark: "#ABC0FF" },
        { id: "graphite-wave", motif: "waves", light: ["#F5F6F8", "#E6E9F0", "#DDE1EA"], dark: ["#15171E", "#20242E", "#191C26"], inkLight: "#69748B", inkDark: "#AAB8CF" },
        { id: "forest-whisper", motif: "leaves", light: ["#F1F7EB", "#D7E8D2", "#BBDAC9"], dark: ["#12251F", "#213D31", "#142C28"], inkLight: "#527E60", inkDark: "#A3DAB2" },
        { id: "deep-night", motif: "orbits", light: ["#F0EEFF", "#D9DDF7", "#C6D5F0"], dark: ["#101323", "#1E2343", "#141D35"], inkLight: "#6861B4", inkDark: "#ABA7FF" },
        { id: "violet-mesh", motif: "grid", light: ["#F9F2FF", "#E7DDFA", "#DCE3FA"], dark: ["#231830", "#33234C", "#25213A"], inkLight: "#9269B3", inkDark: "#D5AFFF" },
        { id: "ocean-current", motif: "waves", light: ["#F0FDFF", "#D4F1F5", "#BCE4EE"], dark: ["#0C2530", "#103945", "#0E2535"], inkLight: "#378899", inkDark: "#84D8DF" },
        { id: "amber-glow", motif: "confetti", light: ["#FFF8E8", "#FFE6BB", "#F6D6AF"], dark: ["#2F2518", "#453022", "#2D241D"], inkLight: "#BF8548", inkDark: "#FFD287" },
        { id: "sky-paper", motif: "arches", light: ["#F8FCFF", "#EDF4FF", "#E0EDF8"], dark: ["#152A38", "#213B4C", "#172B3C"], inkLight: "#6386A5", inkDark: "#A7D6EC" },
        { id: "terracotta-dust", motif: "dots", light: ["#FFF2E9", "#F4DED1", "#F5DDD3"], dark: ["#2E211E", "#493029", "#2A2322"], inkLight: "#BA7562", inkDark: "#F1AE93" },
        { id: "silver-mist", motif: "grid", light: ["#F9FAFD", "#EAEEF4", "#DDE5EE"], dark: ["#1C202A", "#2C3341", "#1C2534"], inkLight: "#79889B", inkDark: "#B7C5E1" },
        { id: "emerald-tide", motif: "orbits", light: ["#F3FFF6", "#D6F4DE", "#BAE7D2"], dark: ["#0A2B26", "#12463D", "#0A302B"], inkLight: "#3E9675", inkDark: "#91E8C8" }
    ]

    function findSpec(id) {
        for (let i = 0; i < presets.length; ++i) {
            if (presets[i].id === id)
                return presets[i]
        }
        return presets[0]
    }

    readonly property var activeSpec: findSpec(root.presetId)
    readonly property bool hasWallpaper: activeSpec.id !== "none"
    readonly property var activeColors: {
        if (!hasWallpaper)
            return [BeyGramTheme.bg, BeyGramTheme.bg, BeyGramTheme.bg]
        if (root.amoled)
            return ["#000000", "#000000", "#000000"]
        return root.dark ? activeSpec.dark : activeSpec.light
    }
    readonly property color patternInk: root.dark ? activeSpec.inkDark : activeSpec.inkLight
    readonly property real patternAlpha: Math.min(0.30, Math.max(0, root.intensity) / 100.0 * 0.68)

    onPresetIdChanged: motifCanvas.requestPaint()
    onIntensityChanged: motifCanvas.requestPaint()
    onDarkChanged: motifCanvas.requestPaint()
    onAmoledChanged: motifCanvas.requestPaint()

    Rectangle {
        anchors.fill: parent
        color: root.activeColors[0]
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: root.activeColors[0] }
            GradientStop { position: 0.52; color: root.activeColors[1] }
            GradientStop { position: 1.0; color: root.activeColors[2] }
        }
    }

    // Soft ambient accent wash when no custom wallpaper is chosen (or non-AMOLED preset).
    Rectangle {
        anchors.fill: parent
        visible: !root.amoled
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.alpha(root.hasWallpaper ? root.patternInk : BeyGramTheme.accent, root.dark ? 0.07 : 0.05) }
            GradientStop { position: 0.55; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.alpha(BeyGramTheme.accent, root.dark ? 0.04 : 0.03) }
        }
    }

    Canvas {
        id: motifCanvas
        anchors.fill: parent
        visible: root.hasWallpaper && root.patternAlpha > 0.01 && width > 0 && height > 0
        renderStrategy: Canvas.Cooperative

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.hasWallpaper || root.patternAlpha <= 0.01)
                return

            const spec = root.activeSpec
            const c = root.patternInk
            ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, root.patternAlpha)
            ctx.fillStyle = Qt.rgba(c.r, c.g, c.b, root.patternAlpha)
            ctx.lineWidth = 1.3
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const cell = 112
            const cols = Math.ceil(width / cell) + 1
            const rows = Math.ceil(height / cell) + 1

            if (spec.motif === "waves") {
                for (let row = 0; row <= rows * 2; ++row) {
                    const y = row * cell * 0.57
                    ctx.beginPath()
                    ctx.moveTo(-cell, y)
                    for (let x = -cell; x < width + cell; x += cell) {
                        ctx.bezierCurveTo(
                            x + cell * 0.27, y - cell * 0.30,
                            x + cell * 0.73, y + cell * 0.30,
                            x + cell, y
                        )
                    }
                    ctx.stroke()
                }
            } else if (spec.motif === "grid") {
                for (let col = 0; col <= cols; ++col) {
                    const x = col * cell
                    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                }
                for (let row = 0; row <= rows; ++row) {
                    const y = row * cell
                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                }
                for (let r = 0; r <= rows; ++r) {
                    for (let cIdx = 0; cIdx <= cols; ++cIdx) {
                        ctx.beginPath()
                        ctx.arc(cIdx * cell, r * cell, 2.2, 0, Math.PI * 2)
                        ctx.fill()
                    }
                }
            } else {
                for (let row = 0; row <= rows; ++row) {
                    for (let col = 0; col <= cols; ++col) {
                        const x = col * cell + (row % 2 === 0 ? 0 : cell * 0.5)
                        const y = row * cell
                        if (spec.motif === "dots") {
                            ctx.beginPath(); ctx.arc(x + cell * 0.32, y + cell * 0.30, cell * 0.065, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.arc(x + cell * 0.78, y + cell * 0.70, cell * 0.018, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.arc(x + cell * 0.17, y + cell * 0.80, cell * 0.025, 0, Math.PI * 2); ctx.fill()
                        } else if (spec.motif === "orbits") {
                            ctx.beginPath(); ctx.arc(x + cell * 0.5, y + cell * 0.5, cell * 0.32, 0, Math.PI * 2); ctx.stroke()
                            ctx.beginPath(); ctx.arc(x + cell * 0.5, y + cell * 0.5, cell * 0.18, 0, Math.PI * 2); ctx.stroke()
                            ctx.beginPath(); ctx.arc(x + cell * 0.73, y + cell * 0.27, 3, 0, Math.PI * 2); ctx.fill()
                        } else if (spec.motif === "arches") {
                            ctx.beginPath(); ctx.arc(x + cell * 0.5, y + cell * 0.75, cell * 0.34, Math.PI, Math.PI * 2); ctx.stroke()
                            ctx.beginPath(); ctx.arc(x + cell * 0.5, y + cell * 0.75, cell * 0.18, Math.PI, Math.PI * 2); ctx.stroke()
                        } else if (spec.motif === "leaves") {
                            ctx.beginPath()
                            ctx.moveTo(x + cell * 0.30, y + cell * 0.55)
                            ctx.quadraticCurveTo(x + cell * 0.45, y + cell * 0.20, x + cell * 0.72, y + cell * 0.32)
                            ctx.quadraticCurveTo(x + cell * 0.58, y + cell * 0.65, x + cell * 0.30, y + cell * 0.55)
                            ctx.stroke()
                        } else if (spec.motif === "confetti") {
                            ctx.beginPath(); ctx.arc(x + cell * 0.24, y + cell * 0.28, 2.5, 0, Math.PI * 2); ctx.fill()
                            ctx.beginPath(); ctx.moveTo(x + cell * 0.62, y + cell * 0.30); ctx.lineTo(x + cell * 0.74, y + cell * 0.42); ctx.stroke()
                            ctx.beginPath(); ctx.arc(x + cell * 0.48, y + cell * 0.74, 3.2, 0, Math.PI * 2); ctx.stroke()
                        } else {
                            // doodles
                            ctx.beginPath(); ctx.arc(x + cell * 0.30, y + cell * 0.32, 6, 0, Math.PI * 1.4); ctx.stroke()
                            ctx.beginPath(); ctx.moveTo(x + cell * 0.65, y + cell * 0.60); ctx.lineTo(x + cell * 0.78, y + cell * 0.72); ctx.stroke()
                            ctx.beginPath(); ctx.arc(x + cell * 0.22, y + cell * 0.76, 2.5, 0, Math.PI * 2); ctx.fill()
                        }
                    }
                }
            }
        }
    }
}
