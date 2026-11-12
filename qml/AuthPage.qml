import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "ds/design.js" as Design

// Sign-in surface. One centred card holds the brand mark, the API credential
// provisioning and whichever authorization stage TDLib reports. The page only
// collects input; every request and every piece of state comes from BeyGramApp.
Item {
    id: page

    property string lastStage: ""
    property bool credentialsOpen: !BeyGramApp.credentialsConfigured
    property int resendSeconds: 0

    // Guards the stage-change handlers until every child exists.
    property bool uiReady: false

    // A stage that waits for the core instead of asking the user for input.
    readonly property bool transientStage: page.stageIs("idle") || page.stageIs("loading")
        || page.stageIs("wait_tdlib_parameters") || page.stageIs("closing")
        || page.stageIs("closed") || page.stageIs("error")
        || page.stageIs("locked") || page.stageIs("unavailable")
        || page.stageIs("not_configured")

    readonly property bool connecting: page.stageIs("loading")
        || page.stageIs("wait_tdlib_parameters") || page.stageIs("closing")

    readonly property bool authReady: BeyGramApp.startupError === ""
        && BeyGramApp.tdlibAvailable && BeyGramApp.credentialsConfigured

    function stageIs(name) {
        return BeyGramApp.stageName === name
    }

    function focusForStage() {
        if (stageIs("wait_phone_number")) phoneField.forceActiveFocus()
        else if (stageIs("wait_code")) codeField.forceActiveFocus()
        else if (stageIs("wait_password")) passwordField.forceActiveFocus()
        else if (stageIs("wait_email_address")) emailField.forceActiveFocus()
        else if (stageIs("wait_email_code")) emailCodeField.forceActiveFocus()
        else if (stageIs("wait_registration")) firstNameField.forceActiveFocus()
    }

    function startResendCountdown() {
        page.resendSeconds = 60
    }

    function formatCountdown(seconds) {
        var m = Math.floor(seconds / 60)
        var s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    // Keeps the phone/QR toggle and the resend cooldown in step with the stage.
    function syncStageUi() {
        methodToggle.currentIndex = page.stageIs("wait_qr_code") ? 1 : 0
        if (page.stageIs("wait_code") || page.stageIs("wait_email_code"))
            page.startResendCountdown()
        else
            page.resendSeconds = 0
    }

    Connections {
        target: BeyGramApp
        function onStateChanged() {
            if (page.uiReady && BeyGramApp.stageName !== page.lastStage) {
                page.lastStage = BeyGramApp.stageName
                page.syncStageUi()
                page.focusForStage()
            }
        }
    }

    Component.onCompleted: {
        uiReady = true
        lastStage = BeyGramApp.stageName
        syncStageUi()
        focusForStage()
    }

    Component.onDestruction: uiReady = false

    Timer {
        id: resendTimer
        interval: 1000
        repeat: true
        running: page.resendSeconds > 0
        onTriggered: page.resendSeconds = Math.max(0, page.resendSeconds - 1)
    }

    Rectangle {
        anchors.fill: parent
        color: BeyGramTheme.bg
    }

    Flickable {
        id: viewport

        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: Math.max(height, authCard.implicitHeight + Design.space8 * 2)
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        DsCard {
            id: authCard

            width: 420
            x: (viewport.width - width) / 2
            y: Math.max(Design.space8, (viewport.height - implicitHeight) / 2)
            padding: Design.space6
            spacing: Design.space4

            Behavior on y {
                NumberAnimation { duration: Design.motionNormal; easing.type: Easing.OutCubic }
            }

            content: ColumnLayout {
                Layout.fillWidth: true
                spacing: Design.space4

                // ----------------------------------------------------------
                // Brand
                // ----------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 56
                        radius: Design.radiusLg + 2
                        color: BeyGramTheme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "B"
                            color: BeyGramTheme.onAccent
                            font.pixelSize: 30
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: "BeyGram Windows"
                        color: BeyGramTheme.text
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: "Sign in to your Telegram account"
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                    }
                }

                // ----------------------------------------------------------
                // Startup / runtime failure
                // ----------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2
                    visible: BeyGramApp.startupError !== "" || !BeyGramApp.tdlibAvailable

                    DsBadge {
                        text: BeyGramApp.startupError !== "" ? "STARTUP ERROR" : "TDLIB MISSING"
                        tone: "danger"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: BeyGramApp.startupError !== ""
                              ? "The local account could not be prepared"
                              : "The TDLib runtime is not available"
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontBodyLarge
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: BeyGramApp.startupError !== "" ? BeyGramApp.startupError
                                                             : BeyGramApp.tdlibDetail
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                        wrapMode: Text.WrapAnywhere
                        lineHeight: Design.lineNormal
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !BeyGramApp.tdlibAvailable
                        text: "Build tdjson.dll for Windows x64 and place it next to beygram.exe, or set BEYGRAM_TDLIB_DLL to an absolute path. See docs/WINDOWS_TDLIB.md."
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        wrapMode: Text.WordWrap
                        lineHeight: Design.lineNormal
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        DsButton {
                            text: "Retry"
                            onClicked: BeyGramApp.retrySession()
                        }
                        DsButton {
                            text: "Open data folder"
                            tone: "secondary"
                            onClicked: BeyGramApp.openDataFolder()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }

                // ----------------------------------------------------------
                // Telegram API credentials
                // ----------------------------------------------------------
                DsDivider {
                    visible: BeyGramApp.startupError === ""
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2
                    visible: BeyGramApp.startupError === ""

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2

                        Text {
                            Layout.fillWidth: true
                            text: "Telegram API credentials"
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        DsBadge {
                            text: BeyGramApp.credentialsConfigured
                                  ? (BeyGramApp.credentialsSource !== ""
                                     ? BeyGramApp.credentialsSource.toUpperCase()
                                     : "CONFIGURED")
                                  : "REQUIRED"
                            tone: BeyGramApp.credentialsConfigured ? "ok" : "muted"
                        }

                        DsButton {
                            text: page.credentialsOpen ? "Hide" : "Manage"
                            tone: "ghost"
                            size: "sm"
                            visible: BeyGramApp.credentialsConfigured
                            onClicked: page.credentialsOpen = !page.credentialsOpen
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.credentialsOpen || !BeyGramApp.credentialsConfigured
                        text: BeyGramApp.credentialsHint
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontCaption
                        wrapMode: Text.WordWrap
                        lineHeight: Design.lineNormal
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.credentialsOpen

                        DsTextField {
                            id: apiIdField
                            Layout.fillWidth: true
                            label: "API id"
                            placeholderText: "123456"
                            inputMethodHints: Qt.ImhDigitsOnly
                            maximumLength: 12
                        }

                        DsTextField {
                            id: apiHashField
                            Layout.fillWidth: true
                            label: "API hash"
                            placeholderText: "32 hexadecimal characters"
                            echoMode: TextInput.Password
                            maximumLength: 32
                            inputMethodHints: Qt.ImhPreferLowercase
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: BeyGramApp.credentialsError !== ""
                            text: BeyGramApp.credentialsError
                            color: BeyGramTheme.danger
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsButton {
                                Layout.fillWidth: true
                                text: "Save securely"
                                loading: BeyGramApp.busy
                                onClicked: {
                                    if (BeyGramApp.saveCredentials(apiIdField.text, apiHashField.text)) {
                                        apiIdField.text = ""
                                        apiHashField.text = ""
                                        page.credentialsOpen = false
                                    }
                                }
                            }

                            DsButton {
                                text: "Forget"
                                tone: "ghost"
                                visible: BeyGramApp.credentialsFromStore
                                accessibleName: "Forget saved credentials"
                                onClicked: forgetDialog.open()
                            }
                        }
                    }
                }

                // ----------------------------------------------------------
                // Authorization flow
                // ----------------------------------------------------------
                DsDivider {
                    visible: page.authReady
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space3
                    visible: page.authReady

                    // Stage heading (the transient block repeats it as status).
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: !page.transientStage

                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.stageTitle
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontTitle
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        DsSpinner {
                            Layout.alignment: Qt.AlignVCenter
                            visible: BeyGramApp.busy
                            running: visible
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !page.transientStage && BeyGramApp.stageDetail !== ""
                        text: BeyGramApp.stageDetail
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontSmall
                        wrapMode: Text.WordWrap
                        lineHeight: Design.lineNormal
                    }

                    // Wrapped error line for the current stage. It sits directly
                    // above the inputs, so a failed password attempt reads next
                    // to the field that produced it.
                    Rectangle {
                        Layout.fillWidth: true
                        visible: BeyGramApp.authError !== ""
                        implicitHeight: authErrorText.implicitHeight + Design.space4
                        radius: Design.radiusMd
                        color: Qt.alpha(BeyGramTheme.danger, 0.10)
                        border.width: 1
                        border.color: Qt.alpha(BeyGramTheme.danger, 0.32)

                        Text {
                            id: authErrorText
                            anchors.fill: parent
                            anchors.margins: Design.space2
                            text: BeyGramApp.authError
                            color: BeyGramTheme.danger
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                            lineHeight: Design.lineNormal
                        }
                    }

                    // --- Phone number ------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_phone_number")

                        DsTextField {
                            id: phoneField
                            Layout.fillWidth: true
                            label: "Phone number"
                            placeholderText: "+1 415 555 0123"
                            inputMethodHints: Qt.ImhDialableCharactersOnly
                            maximumLength: 24
                            clearable: true
                            onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPhone(text) }
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Use the full international number, including the country code."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Continue"
                            enabled: phoneField.text.trim().length > 0
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitPhone(phoneField.text)
                        }
                    }

                    // --- Login code --------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_code")

                        DsTextField {
                            id: codeField
                            Layout.fillWidth: true
                            label: "Login code"
                            placeholderText: BeyGramApp.codeLength > 0
                                             ? BeyGramApp.codeLength + "-digit code"
                                             : "5-digit code"
                            inputMethodHints: Qt.ImhDigitsOnly
                            maximumLength: BeyGramApp.codeLength > 0 ? BeyGramApp.codeLength : 5
                            font.pixelSize: 18
                            font.letterSpacing: 18
                            font.weight: Font.DemiBold
                            horizontalAlignment: TextInput.AlignHCenter
                            onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitCode(text) }
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.codePhoneNumber !== ""
                                  ? "Sent to " + BeyGramApp.codePhoneNumber
                                  : "Telegram sent a login code to your other devices."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: page.resendSeconds > 0
                                  ? "Resend available in " + page.formatCountdown(page.resendSeconds)
                                  : "You can request a new code now."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Sign in"
                            enabled: codeField.text.length > 0
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitCode(codeField.text)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsButton {
                                Layout.fillWidth: true
                                text: "Resend code"
                                tone: "secondary"
                                enabled: page.resendSeconds === 0 && !BeyGramApp.busy
                                onClicked: {
                                    BeyGramApp.resendCode()
                                    page.startResendCountdown()
                                }
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: "Back"
                                tone: "ghost"
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }
                    }

                    // --- Two-step password -------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_password")

                        DsTextField {
                            id: passwordField
                            Layout.fillWidth: true
                            label: "Two-step verification password"
                            placeholderText: "Cloud password"
                            echoMode: TextInput.Password
                            onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPassword(text) }
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: BeyGramApp.passwordHint !== ""
                            text: "Hint: " + BeyGramApp.passwordHint
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !BeyGramApp.passwordHasRecoveryEmail
                            text: "This account has no recovery email configured. A forgotten password cannot be recovered from this client."
                            color: BeyGramTheme.warnInk
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Continue"
                            enabled: passwordField.text.length > 0
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitPassword(passwordField.text)
                        }

                        DsButton {
                            Layout.fillWidth: true
                            text: "Back"
                            tone: "ghost"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }

                    // --- Email address -----------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_email_address")

                        DsTextField {
                            id: emailField
                            Layout.fillWidth: true
                            label: "Email address"
                            placeholderText: "name@example.com"
                            inputMethodHints: Qt.ImhEmailCharactersOnly
                            maximumLength: 254
                            clearable: true
                            onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailAddress(text) }
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Telegram asks for an email address to secure this account."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Continue"
                            enabled: emailField.text.includes("@")
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitEmailAddress(emailField.text)
                        }

                        DsButton {
                            Layout.fillWidth: true
                            text: "Back"
                            tone: "ghost"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }

                    // --- Email code --------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_email_code")

                        DsTextField {
                            id: emailCodeField
                            Layout.fillWidth: true
                            label: "Email code"
                            placeholderText: BeyGramApp.emailCodeLength > 0
                                             ? BeyGramApp.emailCodeLength + "-digit code"
                                             : "Email code"
                            inputMethodHints: Qt.ImhDigitsOnly
                            maximumLength: BeyGramApp.emailCodeLength > 0
                                           ? BeyGramApp.emailCodeLength : 12
                            font.pixelSize: 18
                            font.letterSpacing: 18
                            font.weight: Font.DemiBold
                            horizontalAlignment: TextInput.AlignHCenter
                            onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailCode(text) }
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: page.resendSeconds > 0
                                  ? "Resend available in " + page.formatCountdown(page.resendSeconds)
                                  : "You can request a new code now."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Sign in"
                            enabled: emailCodeField.text.length > 0
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitEmailCode(emailCodeField.text)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsButton {
                                Layout.fillWidth: true
                                text: "Resend code"
                                tone: "secondary"
                                enabled: page.resendSeconds === 0 && !BeyGramApp.busy
                                onClicked: {
                                    BeyGramApp.resendCode()
                                    page.startResendCountdown()
                                }
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: "Back"
                                tone: "ghost"
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }
                    }

                    // --- Registration ------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: page.stageIs("wait_registration")

                        DsTextField {
                            id: firstNameField
                            Layout.fillWidth: true
                            label: "First name"
                            maximumLength: 64
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        DsTextField {
                            id: lastNameField
                            Layout.fillWidth: true
                            label: "Last name (optional)"
                            maximumLength: 64
                            onTextEdited: BeyGramApp.clearAuthError()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "By creating an account you must be at least "
                                  + BeyGramApp.registrationMinUserAge + " years old and accept "
                                  + "Telegram's Terms of Service."
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontCaption
                            wrapMode: Text.WordWrap
                            lineHeight: Design.lineNormal
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(140, termsText.implicitHeight + Design.space4)
                            visible: BeyGramApp.registrationTerms !== ""
                            radius: Design.radiusMd
                            color: Qt.alpha(BeyGramTheme.surface2, 0.6)
                            border.width: 1
                            border.color: BeyGramTheme.line

                            Flickable {
                                anchors.fill: parent
                                anchors.margins: Design.space2
                                clip: true
                                contentWidth: width
                                contentHeight: termsText.implicitHeight

                                Text {
                                    id: termsText
                                    width: parent.width
                                    text: BeyGramApp.registrationTerms
                                    color: BeyGramTheme.muted
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        DsButton {
                            Layout.fillWidth: true
                            Layout.topMargin: Design.space1
                            text: "Create account"
                            enabled: firstNameField.text.trim().length > 0
                            loading: BeyGramApp.busy
                            onClicked: BeyGramApp.submitRegistration(firstNameField.text,
                                                                    lastNameField.text)
                        }

                        DsButton {
                            Layout.fillWidth: true
                            text: "Back"
                            tone: "ghost"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }

                    // --- QR login ----------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3
                        visible: page.stageIs("wait_qr_code")

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 200
                            radius: Design.radiusLg
                            color: "white"
                            border.width: 1
                            border.color: Qt.alpha(BeyGramTheme.line, 0.8)

                            Image {
                                anchors.fill: parent
                                anchors.margins: Design.space2
                                source: BeyGramApp.qrRevision > 0
                                        ? "image://beygramqr/current?v=" + BeyGramApp.qrRevision
                                        : ""
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                smooth: false
                                visible: BeyGramApp.qrRevision > 0
                            }

                            DsSpinner {
                                anchors.centerIn: parent
                                visible: BeyGramApp.qrRevision === 0
                                running: visible
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: BeyGramApp.qrRevision > 0
                                  ? "Settings → Devices → Link Desktop Device,\nthen scan this code."
                                  : "Requesting a QR code from Telegram…"
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                            lineHeight: Design.lineNormal
                        }

                        DsButton {
                            Layout.fillWidth: true
                            text: "Open in Telegram"
                            tone: "secondary"
                            enabled: BeyGramApp.qrRevision > 0
                            onClicked: BeyGramApp.openQrLoginInTelegram()
                        }
                    }

                    // --- Loading / connecting / terminal states ----------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3
                        visible: page.transientStage

                        DsSpinner {
                            Layout.alignment: Qt.AlignHCenter
                            visible: page.connecting
                            running: visible
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: BeyGramApp.stageTitle
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontTitle
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            visible: BeyGramApp.stageDetail !== ""
                            text: BeyGramApp.stageDetail
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                            lineHeight: Design.lineNormal
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsButton {
                                Layout.fillWidth: true
                                text: page.stageIs("closing") ? "Please wait" : "Retry"
                                enabled: !page.stageIs("closing")
                                onClicked: BeyGramApp.retrySession()
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: "Restart session"
                                tone: "secondary"
                                visible: !page.stageIs("loading")
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }
                    }
                }

                // ----------------------------------------------------------
                // Phone / QR method toggle
                // ----------------------------------------------------------
                DsDivider {
                    visible: page.authReady && (page.stageIs("wait_phone_number")
                                                || page.stageIs("wait_qr_code"))
                }

                DsSegmented {
                    id: methodToggle
                    Layout.fillWidth: true
                    visible: page.authReady && (page.stageIs("wait_phone_number")
                                                || page.stageIs("wait_qr_code"))
                    model: ["Phone number", "Sign in with QR"]
                    currentIndex: page.stageIs("wait_qr_code") ? 1 : 0
                    onActivated: function(index) {
                        if (index === 1)
                            BeyGramApp.requestQrLogin()
                        else
                            BeyGramApp.usePhoneInstead()
                    }
                }

                // ----------------------------------------------------------
                // Security note
                // ----------------------------------------------------------
                DsDivider {
                    visible: BeyGramApp.startupError === ""
                }

                Text {
                    Layout.fillWidth: true
                    visible: BeyGramApp.startupError === ""
                    text: BeyGramApp.credentialsFromBuild
                          ? "This local test build embeds the Telegram api_id/api_hash; do not distribute it. Session keys stay protected with Windows DPAPI, and login codes, passwords and QR links are never written to the log."
                          : "Session keys are protected with Windows DPAPI for this user. Login codes, passwords and QR links are never written to the log."
                    color: BeyGramTheme.muted
                    font.pixelSize: Design.fontCaption
                    wrapMode: Text.WordWrap
                    lineHeight: Design.lineNormal
                }
            }
        }
    }

    DsDialog {
        id: forgetDialog
        title: "Forget saved API credentials?"
        description: "The DPAPI-protected api_id/api_hash will be removed. The Telegram session itself is not affected until the app restarts."
        primaryText: "Forget"
        secondaryText: "Cancel"
        tone: "danger"
        onAccepted: {
            BeyGramApp.clearStoredCredentials()
            page.credentialsOpen = true
        }
    }
}
