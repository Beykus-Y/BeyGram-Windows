import QtQuick
import "../ds/design.js" as Design

// Card is the legacy surface container. DsCard owns the fill, hairline and
// radius; this wrapper keeps the old content alias and metrics so pages do not
// have to change.
DsCard {
    id: card

    // Pages put their children directly inside Card. Route them into DsCard's
    // content column instead of the Rectangle's visual data.
    default property alias contentData: card.content

    // Legacy card inset (20 px). DsCard's own default is Design.space4 (16).
    padding: Design.space5

    // Legacy gap between children; the same value as DsCard's default (12 px).
    spacing: Design.space3
}
