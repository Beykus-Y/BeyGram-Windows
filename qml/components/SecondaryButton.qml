import QtQuick

// SecondaryButton is the legacy outlined button. DsButton owns the fill,
// hairline, hover and focus treatment.
DsButton {
    id: control

    tone: "secondary"

    // Pages set Accessible.name on the old Button root. DsButton exposes the
    // spoken label as accessibleName for the inner native button.
    accessibleName: Accessible.name
}
