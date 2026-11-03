import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0

Item {
    id: page

    property string lastStage: ""
    property bool credentialsOpen: !BeyGramApp.credentialsConfigured

    function stageIs(name) {
        return BeyGramApp.stageName === name
    }

    function focusForStage() {
        if (stageIs("wait_phone_number")) phoneField.forceInputFocus()
        else if (stageIs("wait_code")) codeField.forceInputFocus()
        else if (stageIs("wait_password")) passwordField.forceInputFocus()
        else if (stageIs("wait_email_address")) emailField.forceInputFocus()
        else if (stageIs("wait_email_code")) emailCodeField.forceInputFocus()
        else if (stageIs("wait_registration")) firstNameField.forceInputFocus()
    }

    Connections {
        target: BeyGramApp
        function onStateChanged() {
            if (BeyGramApp.stageName !== page.lastStage) {
                page.lastStage = BeyGramApp.stageName
                page.focusForStage()
            }
        }
    }

    Component.onCompleted: {
        lastStage = BeyGramApp.stageName
        focusForStage()
    }

    Flickable {
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight + 64
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: Math.min(620, page.width - 64)
            x: (page.width - width) / 2
            y: 30
            spacing: 18

            // ------------------------------------------------------------------
            // Brand header
            // ------------------------------------------------------------------
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 14

                Rectangle {
                    width: 52
                    height: 52
                    radius: 17
                    color: BeyGramTheme.accent
                    Text {
                        anchors.centerIn: parent
                        text: "B"
                        color: BeyGramTheme.onAccent
                        font.pixelSize: 28
                        font.weight: Font.DemiBold
                    }
                }
                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "BeyGram Windows"
                        color: BeyGramTheme.text
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Native Telegram client · Phase 1 authentication"
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                    }
                }
            }

            // ------------------------------------------------------------------
            // Runtime / startup failures
            // ------------------------------------------------------------------
            Card {
                Layout.fillWidth: true
                visible: BeyGramApp.startupError !== "" || !BeyGramApp.tdlibAvailable
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Pill {
                        text: BeyGramApp.startupError !== "" ? "STARTUP ERROR" : "TDLIB MISSING"
                        tone: "danger"
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.startupError !== "" ? "The local account could not be prepared"
                                                         : "The TDLib runtime is not available"
                    color: BeyGramTheme.text
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.startupError !== "" ? BeyGramApp.startupError
                                                         : BeyGramApp.tdlibDetail
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    lineHeight: 1.3
                }
                Text {
                    Layout.fillWidth: true
                    visible: !BeyGramApp.tdlibAvailable
                    text: "Build tdjson.dll for Windows x64 and place it next to beygram.exe, or set BEYGRAM_TDLIB_DLL to an absolute path. See docs/WINDOWS_TDLIB.md."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }
                RowLayout {
                    spacing: 8
                    PrimaryButton {
                        text: "Retry"
                        onClicked: BeyGramApp.retrySession()
                    }
                    SecondaryButton {
                        text: "Open data folder"
                        onClicked: BeyGramApp.openDataFolder()
                    }
                }
            }

            // ------------------------------------------------------------------
            // API credentials
            // ------------------------------------------------------------------
            Card {
                Layout.fillWidth: true
                spacing: 12
                visible: BeyGramApp.startupError === ""

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Telegram API credentials"
                        color: BeyGramTheme.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    Pill {
                        text: BeyGramApp.credentialsConfigured
                              ? BeyGramApp.credentialsSource.toUpperCase()
                              : "REQUIRED"
                        tone: BeyGramApp.credentialsConfigured ? "ok" : "warn"
                    }
                    SecondaryButton {
                        text: page.credentialsOpen ? "Hide" : "Manage"
                        visible: BeyGramApp.credentialsConfigured
                        onClicked: page.credentialsOpen = !page.credentialsOpen
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.credentialsHint
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.credentialsOpen

                    AuthField {
                        id: apiIdField
                        Layout.fillWidth: true
                        label: "API id"
                        placeholderText: "123456"
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: 12
                    }
                    AuthField {
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
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Save securely"
                            enabled: !BeyGramApp.busy
                            onClicked: {
                                if (BeyGramApp.saveCredentials(apiIdField.text, apiHashField.text)) {
                                    apiIdField.text = ""
                                    apiHashField.text = ""
                                    page.credentialsOpen = false
                                }
                            }
                        }
                        SecondaryButton {
                            text: "Forget saved credentials"
                            visible: BeyGramApp.credentialsFromStore
                            onClicked: forgetDialog.open()
                        }
                    }
                }
            }

            // ------------------------------------------------------------------
            // Authorization flow
            // ------------------------------------------------------------------
            Card {
                Layout.fillWidth: true
                spacing: 14
                visible: BeyGramApp.startupError === "" && BeyGramApp.tdlibAvailable
                         && BeyGramApp.credentialsConfigured

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Pill { text: "TELEGRAM"; tone: "accent" }
                    Item { Layout.fillWidth: true }
                    BusyIndicator {
                        running: BeyGramApp.busy
                        visible: BeyGramApp.busy
                        implicitWidth: 22
                        implicitHeight: 22
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.stageTitle
                    color: BeyGramTheme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: BeyGramApp.stageDetail !== ""
                    text: BeyGramApp.stageDetail
                    color: BeyGramTheme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: BeyGramApp.authError !== ""
                    implicitHeight: errorText.implicitHeight + 20
                    radius: 10
                    color: Qt.alpha(BeyGramTheme.danger, 0.12)
                    border.width: 1
                    border.color: Qt.alpha(BeyGramTheme.danger, 0.35)
                    Text {
                        id: errorText
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 10
                        text: BeyGramApp.authError
                        color: BeyGramTheme.danger
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                    }
                }

                // Phone number
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_phone_number")

                    AuthField {
                        id: phoneField
                        Layout.fillWidth: true
                        label: "Phone number"
                        placeholderText: "+1 415 555 0123"
                        inputMethodHints: Qt.ImhDialableCharactersOnly
                        maximumLength: 24
                        onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPhone(text) }
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Continue"
                            enabled: !BeyGramApp.busy && phoneField.text.trim().length > 0
                            onClicked: BeyGramApp.submitPhone(phoneField.text)
                        }
                        SecondaryButton {
                            text: "Log in with QR"
                            enabled: !BeyGramApp.busy
                            onClicked: BeyGramApp.requestQrLogin()
                        }
                    }
                }

                // Login code
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_code")

                    AuthField {
                        id: codeField
                        Layout.fillWidth: true
                        label: "Login code"
                        placeholderText: BeyGramApp.codeLength > 0
                                         ? BeyGramApp.codeLength + " digits"
                                         : "Code from Telegram"
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: BeyGramApp.codeLength > 0 ? BeyGramApp.codeLength : 12
                        onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitCode(text) }
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Sign in"
                            enabled: !BeyGramApp.busy && codeField.text.length > 0
                            onClicked: BeyGramApp.submitCode(codeField.text)
                        }
                        SecondaryButton {
                            text: "Resend code"
                            enabled: !BeyGramApp.busy
                            onClicked: BeyGramApp.resendCode()
                        }
                        SecondaryButton {
                            text: "Use another number"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }

                // Two-step password
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_password")

                    AuthField {
                        id: passwordField
                        Layout.fillWidth: true
                        label: "Two-step verification password"
                        placeholderText: "Cloud password"
                        echoMode: TextInput.Password
                        onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPassword(text) }
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: BeyGramApp.passwordHint !== ""
                        text: "Hint: " + BeyGramApp.passwordHint
                        color: BeyGramTheme.muted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: !BeyGramApp.passwordHasRecoveryEmail
                        text: "This account has no recovery email configured. A forgotten password cannot be recovered from this client."
                        color: BeyGramTheme.warnInk
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Sign in"
                            enabled: !BeyGramApp.busy && passwordField.text.length > 0
                            onClicked: BeyGramApp.submitPassword(passwordField.text)
                        }
                        SecondaryButton {
                            text: "Use another number"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }

                // Email address
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_email_address")

                    AuthField {
                        id: emailField
                        Layout.fillWidth: true
                        label: "Email address"
                        placeholderText: "name@example.com"
                        inputMethodHints: Qt.ImhEmailCharactersOnly
                        maximumLength: 254
                        onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailAddress(text) }
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Continue"
                            enabled: !BeyGramApp.busy && emailField.text.includes("@")
                            onClicked: BeyGramApp.submitEmailAddress(emailField.text)
                        }
                        SecondaryButton {
                            text: "Cancel"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }

                // Email code
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_email_code")

                    AuthField {
                        id: emailCodeField
                        Layout.fillWidth: true
                        label: "Email code"
                        placeholderText: BeyGramApp.emailCodeLength > 0
                                         ? BeyGramApp.emailCodeLength + " digits"
                                         : "Code from the email"
                        inputMethodHints: Qt.ImhDigitsOnly
                        maximumLength: BeyGramApp.emailCodeLength > 0
                                       ? BeyGramApp.emailCodeLength : 12
                        onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailCode(text) }
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Sign in"
                            enabled: !BeyGramApp.busy && emailCodeField.text.length > 0
                            onClicked: BeyGramApp.submitEmailCode(emailCodeField.text)
                        }
                        SecondaryButton {
                            text: "Resend code"
                            enabled: !BeyGramApp.busy
                            onClicked: BeyGramApp.resendCode()
                        }
                        SecondaryButton {
                            text: "Cancel"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }

                // Registration
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("wait_registration")

                    AuthField {
                        id: firstNameField
                        Layout.fillWidth: true
                        label: "First name"
                        maximumLength: 64
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    AuthField {
                        id: lastNameField
                        Layout.fillWidth: true
                        label: "Last name (optional)"
                        maximumLength: 64
                        onEdited: BeyGramApp.clearAuthError()
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "By creating an account you must be at least "
                              + BeyGramApp.registrationMinUserAge + " years old and accept "
                              + "Telegram's Terms of Service."
                        color: BeyGramTheme.muted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        lineHeight: 1.3
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(140, termsText.implicitHeight + 20)
                        radius: 10
                        color: Qt.alpha(BeyGramTheme.surface2, 0.6)
                        border.width: 1
                        border.color: BeyGramTheme.line
                        visible: BeyGramApp.registrationTerms !== ""

                        Flickable {
                            anchors.fill: parent
                            anchors.margins: 10
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
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: "Create account"
                            enabled: !BeyGramApp.busy && firstNameField.text.trim().length > 0
                            onClicked: BeyGramApp.submitRegistration(firstNameField.text,
                                                                    lastNameField.text)
                        }
                        SecondaryButton {
                            text: "Cancel"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }

                // QR login
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: page.stageIs("wait_qr_code")

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 264
                        height: 264
                        radius: 16
                        color: "white"
                        border.width: 1
                        border.color: Qt.alpha(BeyGramTheme.line, 0.8)

                        Image {
                            anchors.fill: parent
                            anchors.margins: 12
                            source: BeyGramApp.qrRevision > 0
                                    ? "image://beygramqr/current?v=" + BeyGramApp.qrRevision
                                    : ""
                            cache: false
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            visible: BeyGramApp.qrRevision > 0
                        }
                        BusyIndicator {
                            anchors.centerIn: parent
                            running: BeyGramApp.qrRevision === 0
                            visible: BeyGramApp.qrRevision === 0
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: "Settings → Devices → Link Desktop Device,\nthen scan this code."
                        color: BeyGramTheme.muted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        lineHeight: 1.35
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        SecondaryButton {
                            text: "Use phone number instead"
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                        SecondaryButton {
                            text: "Open in Telegram"
                            enabled: BeyGramApp.qrRevision > 0
                            onClicked: BeyGramApp.openQrLoginInTelegram()
                        }
                    }
                }

                // Transient / terminal states
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: page.stageIs("idle") || page.stageIs("loading")
                             || page.stageIs("wait_tdlib_parameters") || page.stageIs("closing")
                             || page.stageIs("closed") || page.stageIs("error")
                             || page.stageIs("locked") || page.stageIs("unavailable")
                             || page.stageIs("not_configured")

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        running: page.stageIs("loading") || page.stageIs("wait_tdlib_parameters")
                                 || page.stageIs("closing")
                        visible: running
                    }
                    RowLayout {
                        spacing: 8
                        PrimaryButton {
                            text: page.stageIs("closing") ? "Please wait" : "Retry"
                            enabled: !page.stageIs("closing")
                            onClicked: BeyGramApp.retrySession()
                        }
                        SecondaryButton {
                            text: "Restart session"
                            visible: !page.stageIs("loading")
                            onClicked: BeyGramApp.usePhoneInstead()
                        }
                    }
                }
            }

            // ------------------------------------------------------------------
            // Security note
            // ------------------------------------------------------------------
            Card {
                Layout.fillWidth: true
                spacing: 8
                visible: BeyGramApp.startupError === ""

                Text {
                    text: "Session security"
                    color: BeyGramTheme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: BeyGramApp.credentialsFromBuild
                          ? "This local test build embeds the Telegram api_id/api_hash from secrets.properties. They are not logged, but they are extractable from this executable: do not distribute it. The TDLib database key is still protected with Windows DPAPI, and the account database is locked against a second BeyGram process."
                          : "The TDLib database key and API hash are protected with Windows DPAPI for this user. The account database is locked against a second BeyGram process. Login codes, passwords and QR links are never written to the log."
                    color: BeyGramTheme.muted
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    lineHeight: 1.35
                }
            }

            Item { Layout.fillWidth: true; height: 8 }
        }
    }

    Dialog {
        id: forgetDialog
        title: "Forget saved API credentials?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        width: 420

        ColumnLayout {
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: "The DPAPI-protected api_id/api_hash will be removed. The Telegram session itself is not affected until the app restarts."
                color: BeyGramTheme.text
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8
                SecondaryButton {
                    text: "Cancel"
                    onClicked: forgetDialog.close()
                }
                PrimaryButton {
                    text: "Forget"
                    tone: "danger"
                    onClicked: {
                        forgetDialog.close()
                        BeyGramApp.clearStoredCredentials()
                        page.credentialsOpen = true
                    }
                }
            }
        }
    }
}
