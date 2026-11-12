import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

// DsTheme is the semantic layer between the raw theme/appearance singletons and
// the Ds* components. Colours follow BeyGramTheme (preset + Light/Dark/AMOLED);
// visual surface treatment follows BeyGramAppearance (DesignStyle, BubbleStyle,
// density, message font scale, and reduceMotion).
QtObject {
    id: ds

    readonly property bool dark: BeyGramTheme.darkMode
    readonly property bool amoled: BeyGramTheme.modeName === "Amoled"
    readonly property string designStyle: BeyGramAppearance.designStyle
    readonly property string bubbleStyle: BeyGramAppearance.bubbleStyle
    readonly property string listDensity: BeyGramAppearance.listDensity
    readonly property int messageTextSize: BeyGramAppearance.messageTextSize
    readonly property bool reduceMotion: BeyGramAppearance.reduceMotion
    readonly property bool isLiquidGlass: designStyle === "LiquidGlass"
    readonly property bool isSignature: designStyle === "Signature"
    readonly property bool isCompact: listDensity === "Compact"

    readonly property int styleCardRadius: Design.cardRadius(designStyle)
    readonly property int styleRowRadius: Design.rowRadius(designStyle)
    readonly property int styleButtonRadius: Design.buttonRadius(designStyle)

    readonly property color surfaceBorderColor: {
        if (isLiquidGlass)
            return Qt.alpha(dark ? "#FFFFFF" : BeyGramTheme.line, dark ? 0.14 : 0.85)
        if (isSignature)
            return Qt.alpha(BeyGramTheme.accent, dark ? 0.24 : 0.20)
        if (amoled)
            return Qt.lighter(BeyGramTheme.line, 1.35)
        return BeyGramTheme.line
    }

    function motionMs(ms) {
        return Design.motion(ms, reduceMotion)
    }

    // --- Colour roles -------------------------------------------------------
    readonly property QtObject color: QtObject {
        readonly property color bg: BeyGramTheme.bg
        readonly property color surface: BeyGramTheme.surface
        readonly property color surface2: BeyGramTheme.surface2
        readonly property color line: BeyGramTheme.line
        readonly property color text: BeyGramTheme.text
        readonly property color muted: BeyGramTheme.muted
        readonly property color accent: BeyGramTheme.accent
        readonly property color accentSoft: BeyGramTheme.accentSoft
        readonly property color accentInk: BeyGramTheme.accentInk
        readonly property color onAccent: BeyGramTheme.onAccent
        readonly property color bubbleOut: BeyGramTheme.bubbleOut
        readonly property color ok: BeyGramTheme.ok
        readonly property color okInk: BeyGramTheme.okInk
        readonly property color warn: BeyGramTheme.warn
        readonly property color warnInk: BeyGramTheme.warnInk
        readonly property color danger: BeyGramTheme.danger
    }

    // --- Corner radii -------------------------------------------------------
    readonly property QtObject radius: QtObject {
        readonly property int xs: Design.radiusXs
        readonly property int sm: Design.radiusSm
        readonly property int md: Design.radiusMd
        readonly property int lg: Design.radiusLg
        readonly property int xl: Design.radiusXl
        readonly property int pill: Design.radiusPill
    }

    // --- Spacing: a strict 4 px grid ---------------------------------------
    readonly property QtObject space: QtObject {
        readonly property int grid: 4
        readonly property int s0: Design.space0
        readonly property int s1: Design.space1
        readonly property int s2: Design.space2
        readonly property int s3: Design.space3
        readonly property int s4: Design.space4
        readonly property int s5: Design.space5
        readonly property int s6: Design.space6
        readonly property int s7: Design.space6 + Design.space1
        readonly property int s8: Design.space8
        readonly property int s9: Design.space8 + Design.space1
        readonly property int s10: Design.space10
    }

    // --- Typography ---------------------------------------------------------
    readonly property QtObject font: QtObject {
        readonly property int caption: Design.fontCaption
        readonly property int small: Design.fontSmall
        readonly property int body: Design.fontBody
        readonly property int bodyLarge: Design.fontBodyLarge
        readonly property int title: Design.fontTitle
        readonly property int headline: Design.fontHeadline
        readonly property int display: Design.fontDisplay

        readonly property real lineTight: Design.lineTight
        readonly property real lineNormal: Design.lineNormal
        readonly property real lineRelaxed: Design.lineRelaxed

        readonly property int weightRegular: Font.Normal
        readonly property int weightMedium: Font.Medium
        readonly property int weightDemiBold: Font.DemiBold
        readonly property int weightBold: Font.Bold
    }

    // --- Motion -------------------------------------------------------------
    readonly property QtObject motion: QtObject {
        readonly property int fast: ds.motionMs(Design.motionFast)
        readonly property int normal: ds.motionMs(Design.motionNormal)
        readonly property int slow: ds.motionMs(Design.motionSlow)

        readonly property int easeOut: Easing.OutCubic
        readonly property int easeEmphasized: Easing.OutQuint
        readonly property int easeInOut: Easing.InOutCubic
    }

    // --- State opacities ----------------------------------------------------
    readonly property QtObject opacity: QtObject {
        readonly property real disabled: Design.opacityDisabled
        readonly property real secondary: Design.opacitySecondary
        readonly property real hover: Design.opacityHover
        readonly property real pressed: Design.opacityPressed
        readonly property real scrim: Design.opacityScrim
    }

    // --- Control sizes ------------------------------------------------------
    readonly property QtObject control: QtObject {
        readonly property int height: Design.controlHeight
        readonly property int heightSmall: Design.controlHeightSmall
        readonly property int heightLarge: Design.controlHeightLarge
        readonly property int icon: Design.iconSize
        readonly property int iconLarge: Design.iconSizeLarge
        readonly property int avatarSmall: Design.avatarSmall
        readonly property int avatarMedium: Design.avatarMedium
        readonly property int avatarLarge: Design.avatarLarge
        readonly property int avatarXLarge: Design.avatarXLarge
    }

    // --- Chat metrics -------------------------------------------------------
    readonly property QtObject chat: QtObject {
        readonly property int bubbleRadius: Design.bubbleRadiusFor(ds.bubbleStyle)
        readonly property int tailRadius: Design.bubbleTailRadiusFor(ds.bubbleStyle)
        readonly property real maxWidthRatio: Design.bubbleMaxWidthRatio
        readonly property int composerRadius: Design.composerRadius
        readonly property int rowHeight: ds.isCompact ? Design.rowHeightCompact : Design.rowHeight
        readonly property int headerHeight: Design.headerHeight
    }

    // --- Elevation ----------------------------------------------------------
    readonly property QtObject elevation: QtObject {
        readonly property int flat: Design.elevation0
        readonly property int raised: Design.elevation1
        readonly property int overlay: Design.elevation2
    }

    // --- Helpers ------------------------------------------------------------
    function surfaceFor(level) {
        var l = Math.round(Number(level))
        if (isNaN(l))
            l = 1
        if (l <= 0)
            return color.bg
        if (l === 1)
            return color.surface
        if (l === 2)
            return color.surface2
        return dark ? Qt.lighter(color.surface2, 1.12)
                    : Qt.darker(color.surface2, 1.05)
    }

    function textFor(level) {
        var l = Math.round(Number(level))
        if (isNaN(l))
            l = 0
        if (l <= 0)
            return color.text
        if (l === 1)
            return color.muted
        return alpha(color.text, opacity.disabled)
    }

    function alpha(base, value) {
        var a = Number(value)
        if (isNaN(a))
            a = 1
        return Qt.alpha(base, Math.max(0, Math.min(1, a)))
    }
}
