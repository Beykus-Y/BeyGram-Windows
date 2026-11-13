// Message actions context menu.
//
// Presentation only. `facade` is a beygram::ui::MessageActionsFacade, which owns
// the availability decision: the owner calls facade.configure() when it opens
// the menu for a message, this menu renders facade.actions, and a click calls
// facade.trigger(id), which emits the typed request signal. The facade refuses
// actions that are no longer offered, so a menu left open across a message
// update cannot send a stale request.
//
// Rows are generated from the facade list, keeping their order in sync with
// MessageAction declaration order. Labels are translated here because the
// facade deliberately exposes stable, untranslated identifiers. Delete and
// report use the DS danger tone.
//
// The menu owns no message state and must not be given static rows: every row
// comes from the Instantiator below so that it always matches facade.actions.
// Nothing in this file touches TDLib, the network, the message store, or
// logging. A null facade leaves the menu empty and inert.

import QtQuick
import "../ds"

DsMenu {
    id: menu

    /// beygram::ui::MessageActionsFacade instance. May be null while the owner
    /// wires the menu up; every binding below tolerates that.
    property var facade: null

    readonly property bool ready: menu.facade !== null && menu.facade !== undefined

    /// Identifiers of the actions the facade currently offers.
    readonly property var actionIds: menu.ready ? menu.facade.actions : []

    /// True when the facade offers at least one action. Callers should not open
    /// an empty menu.
    readonly property bool hasActions: menu.actionIds.length > 0

    // A menu that lost every action while open would be an empty surface.
    onActionIdsChanged: if (!menu.hasActions) menu.close()

    /// Translated label for an action identifier. The fallback keeps an
    /// identifier the facade adds before this file knows it visible instead of
    /// silently blank.
    function labelFor(action) {
        switch (action) {
        case "reply": return qsTr("Reply")
        case "forward": return qsTr("Forward")
        case "copy": return qsTr("Copy")
        case "edit": return qsTr("Edit")
        case "delete": return qsTr("Delete")
        case "pin": return qsTr("Pin")
        case "save": return qsTr("Save")
        case "react": return qsTr("React")
        case "translate": return qsTr("Translate")
        case "report": return qsTr("Report")
        }
        return action
    }

    /// Decorative glyph for an action identifier. The label carries the
    /// meaning, so an unknown identifier simply gets no glyph.
    function glyphFor(action) {
        switch (action) {
        case "reply": return "\u21A9"
        case "forward": return "\u21AA"
        case "copy": return "\u29C9"
        case "edit": return "\u270E"
        case "delete": return "\u2715"
        case "pin": return "\u2691"
        case "save": return "\u2606"
        case "react": return "\u263A"
        case "translate": return "\u21C4"
        case "report": return "\u26A0"
        }
        return ""
    }

    /// Destructive actions are tinted with the DS danger tone.
    function isDanger(action) {
        return action === "delete" || action === "report"
    }

    Instantiator {
        model: menu.actionIds

        delegate: DsMenuItem {
            property string action: modelData

            text: menu.labelFor(action)
            glyph: menu.glyphFor(action)
            tone: menu.isDanger(action) ? "danger" : "normal"
            onClicked: if (menu.facade) menu.facade.trigger(action)
        }

        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
