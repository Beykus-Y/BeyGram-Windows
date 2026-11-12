import QtQuick
import BeyGram.Theme 1.0
import "design.js" as Design

// DsTheme is the semantic layer between the raw theme singleton and the Ds*
// components. It is deliberately a plain QtObject and not a singleton, so a
// preview, a test or a sub-tree can own an isolated instance:
//
//     DsTheme { id: ds }
//     Rectangle { color: ds.color.surface; radius: ds.radius.md }
//
// Colours come from BeyGramTheme (preset + Light/Dark/AMOLED); geometry, type
// and motion come from design.js. Components must not hard-code a hex value, a
// corner radius or a duration.
QtObject {
    id: ds

    // Convenience for the few places that branch on brightness (for example a
    // specular edge that is lighter in dark mode).
    readonly property bool dark: BeyGramTheme.darkMode

    // --- Colour roles -------------------------------------------------------
    // The names match BeyGramTokens in ThemeController.h and Color.kt, so a
    // screen ported from Android can keep the same vocabulary.
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
    // 12-16 px is the house range for containers; xs/sm are for controls and
    // pills are fully rounded.
    readonly property QtObject radius: QtObject {
        readonly property int xs: Design.radiusXs
        readonly property int sm: Design.radiusSm
        readonly property int md: Design.radiusMd
        readonly property int lg: Design.radiusLg
        readonly property int xl: Design.radiusXl
        readonly property int pill: Design.radiusPill
    }

    // --- Spacing: a strict 4 px grid ---------------------------------------
    // s0..s10 are the grid steps. design.js names the steps the components
    // actually use (0, 1, 2, 3, 4, 5, 6, 8, 10); s7 and s9 are derived from
    // the same grid so a layout never has to invent 28 or 36 by hand.
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
    // Sizes are logical pixels; compare them with Android sp at the default
    // scale. Line heights are multipliers because QML Text has no line token.
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
    // Durations in milliseconds. Components bind these to Behavior/Animation;
    // the easing values are Qt enum integers, ready for Easing curves.
    readonly property QtObject motion: QtObject {
        readonly property int fast: Design.motionFast
        readonly property int normal: Design.motionNormal
        readonly property int slow: Design.motionSlow

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
    // 32 is the smallest accepted target; 40 is the default desktop control.
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
        readonly property int bubbleRadius: Design.bubbleRadius
        readonly property int tailRadius: Design.bubbleRadiusTail
        readonly property real maxWidthRatio: Design.bubbleMaxWidthRatio
        readonly property int composerRadius: Design.composerRadius
        readonly property int rowHeight: Design.rowHeight
        readonly property int headerHeight: Design.headerHeight
    }

    // --- Elevation ----------------------------------------------------------
    // QML has no shadow token: depth is a surface step plus a hairline, never a
    // large blur. These values select the step, not a pixel offset.
    readonly property QtObject elevation: QtObject {
        readonly property int flat: Design.elevation0
        readonly property int raised: Design.elevation1
        readonly property int overlay: Design.elevation2
    }

    // --- Helpers ------------------------------------------------------------

    /// Returns the surface for a depth level: 0 canvas, 1 card, 2 raised
    /// container, 3 overlay. Values outside the range are clamped.
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

    /// Returns a text colour for an emphasis level: 0 primary, 1 secondary,
    /// 2 disabled. Values outside the range are clamped. Accent text is
    /// intentionally not part of the ladder; use color.accent directly.
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

    /// Applies an alpha channel to any colour, clamping value to 0..1. Use it
    /// for hover washes, dividers and status fills instead of new hex values.
    function alpha(base, value) {
        var a = Number(value)
        if (isNaN(a))
            a = 1
        return Qt.alpha(base, Math.max(0, Math.min(1, a)))
    }
}
