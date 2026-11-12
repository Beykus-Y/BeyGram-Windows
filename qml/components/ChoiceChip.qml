// ChoiceChip.qml — single-choice chip used by settings, pickers and calls.
//
// DsChip already exposes exactly the API this component needs (`text`,
// `selected`, `clicked()`), so ChoiceChip is a thin specialisation that keeps
// the old radio-style accessibility role. Selection stays with the parent:
// the chip only reports clicks.

import QtQuick

DsChip {
    Accessible.role: Accessible.RadioButton
}
