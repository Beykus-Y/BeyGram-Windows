.pragma library

// BeyGram design tokens shared by every Ds* component.
//
// Colours deliberately live in BeyGramTheme (they follow the Warm/Graphite
// preset and the Light/Dark/AMOLED mode). This file owns the geometry,
// typography and motion scale so every component agrees on the same numbers.

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
var fontTitle = 17;
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
var avatarSmall = 32;
var avatarMedium = 44;
var avatarLarge = 56;
var avatarXLarge = 72;

// --- Chat -------------------------------------------------------------------
var bubbleRadius = 16;
var bubbleRadiusTail = 4;
var bubbleMaxWidthRatio = 0.72;
var composerRadius = 20;
var rowHeight = 72;
var headerHeight = 56;

// --- Elevation --------------------------------------------------------------
// QML has no shadow token, so elevation is expressed as border + surface step.
var elevation0 = 0;
var elevation1 = 1;
var elevation2 = 2;

// --- Motion -----------------------------------------------------------------
var motionFast = 120;
var motionNormal = 180;
var motionSlow = 260;
// Components use Easing.OutCubic / Easing.OutQuint directly; a .pragma library
// file cannot reference QML enum types.

// --- Opacity ----------------------------------------------------------------
var opacityDisabled = 0.45;
var opacitySecondary = 0.72;
var opacityHover = 0.08;
var opacityPressed = 0.14;
var opacityScrim = 0.55;
