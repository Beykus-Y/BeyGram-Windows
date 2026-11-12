.pragma library

// BeyGram design tokens shared by every Ds* component.
//
// Colours deliberately live in BeyGramTheme (they follow the Warm/Graphite
// preset and the Light/Dark/AMOLED mode). This file owns the geometry,
// typography, Android DesignStyle/BubbleStyle adaptation, and motion scale
// so every component agrees on the same numbers.

// --- Spacing: a strict 4 px grid -------------------------------------------
var space0 = 0;
var space1 = 4;
var space2 = 8;
var space3 = 12;
var space4 = 16;
var space5 = 20;
var space6 = 24;
var space8 = 32;
var space10 = 40;

// --- Radii ------------------------------------------------------------------
var radiusXs = 6;
var radiusSm = 10;
var radiusMd = 12;
var radiusLg = 16;
var radiusXl = 20;
var radiusPill = 999;

// --- Typography (pixel sizes; weights use Font.* in QML) --------------------
var fontCaption = 11;
var fontSmall = 12;
var fontBody = 14;
var fontBodyLarge = 15;
var fontTitle = 18;
var fontHeadline = 22;
var fontDisplay = 28;

var lineTight = 1.15;
var lineNormal = 1.35;
var lineRelaxed = 1.5;

// --- Controls ---------------------------------------------------------------
var controlHeight = 40;
var controlHeightSmall = 32;
var controlHeightLarge = 48;
var iconSize = 18;
var iconSizeLarge = 22;
var avatarSmall = 34;
var avatarMedium = 46;
var avatarLarge = 56;
var avatarXLarge = 76;

// --- Chat -------------------------------------------------------------------
var bubbleRadius = 16;
var bubbleRadiusTail = 4;
var bubbleMaxWidthRatio = 0.72;
var composerRadius = 22;
var rowHeight = 70;
var rowHeightCompact = 58;
var headerHeight = 58;
var railWidthCollapsed = 72;
var railWidthExpanded = 220;

// --- Elevation --------------------------------------------------------------
// QML has no shadow token, so elevation is expressed as border + surface step.
var elevation0 = 0;
var elevation1 = 1;
var elevation2 = 2;

// --- Motion -----------------------------------------------------------------
var motionFast = 120;
var motionNormal = 180;
var motionSlow = 260;

// --- Opacity ----------------------------------------------------------------
var opacityDisabled = 0.45;
var opacitySecondary = 0.72;
var opacityHover = 0.08;
var opacityPressed = 0.14;
var opacityScrim = 0.55;

// --- DesignStyle & BubbleStyle helpers (mirroring Android DesignSystem.kt) --
function cardRadius(style) {
    if (style === "Material2")
        return 10;
    if (style === "Material3")
        return 22;
    if (style === "LiquidGlass" || style === "Signature")
        return 18;
    return radiusLg;
}

function rowRadius(style) {
    if (style === "Material2")
        return 8;
    if (style === "Material3")
        return 16;
    if (style === "LiquidGlass" || style === "Signature")
        return 14;
    return radiusMd;
}

function buttonRadius(style) {
    if (style === "Material2")
        return 8;
    if (style === "Material3")
        return 20;
    if (style === "LiquidGlass" || style === "Signature")
        return 14;
    return radiusSm;
}

function bubbleRadiusFor(bubbleStyle) {
    if (bubbleStyle === "Square")
        return 7;
    if (bubbleStyle === "Soft")
        return 13;
    return 18;
}

function bubbleTailRadiusFor(bubbleStyle) {
    if (bubbleStyle === "Square")
        return 2;
    if (bubbleStyle === "Soft")
        return 4;
    return 5;
}

function motion(ms, reduceMotion) {
    return reduceMotion ? 0 : ms;
}
