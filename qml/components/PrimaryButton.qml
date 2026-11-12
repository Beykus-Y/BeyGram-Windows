import QtQuick

// PrimaryButton is the legacy name for the accent-filled DsButton. Pages keep
// their old API (text, tone, enabled, clicked); the design-system button owns
// the visuals.
DsButton {
    id: control

    tone: "primary"

    // Legacy pages also use neutral, ok, warn and accent; the DS has no such
    // button tones, so they fall back to primary. "danger" is forwarded
    // unchanged.
    onToneChanged: {
        if (control.tone !== "primary" && control.tone !== "secondary"
                && control.tone !== "ghost" && control.tone !== "danger")
            control.tone = "primary"
    }

    // Pages set Accessible.name on the old Button root. DsButton exposes the
    // spoken label as accessibleName for the inner native button.
    accessibleName: Accessible.name
}
