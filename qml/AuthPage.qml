import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BeyGram.Theme 1.0
import BeyGram.App 1.0
import "ds/design.js" as Design

// Sign-in and onboarding surface matching Android BeyGram's warm, calm visual
// DNA while offering pre-login Language and Theme quick switchers.
Item {
    id: page

    property string lastStage: ""
    property bool credentialsOpen: !BeyGramApp.credentialsConfigured
    property int resendSeconds: 0
    property bool uiReady: false

    readonly property bool wideLayout: page.width >= 900

    readonly property bool transientStage: page.stageIs("idle") || page.stageIs("loading")
        || page.stageIs("wait_tdlib_parameters") || page.stageIs("closing")
        || page.stageIs("closed") || page.stageIs("error")
        || page.stageIs("locked") || page.stageIs("unavailable")
        || page.stageIs("not_configured")

    readonly property bool connecting: page.stageIs("loading")
        || page.stageIs("wait_tdlib_parameters") || page.stageIs("closing")

    readonly property bool authReady: BeyGramApp.startupError === ""
        && BeyGramApp.tdlibAvailable && BeyGramApp.credentialsConfigured

    readonly property var langOptions: [
        { code: "auto", label: qsTr("Auto") },
        { code: "ru", label: "RU" },
        { code: "en", label: "EN" }
    ]

    readonly property var themeOptions: [
        { mode: "Light", label: qsTr("Light") },
        { mode: "Dark", label: qsTr("Dark") },
        { mode: "Amoled", label: qsTr("AMOLED") }
    ]

    function stageIs(name) {
        return BeyGramApp.stageName === name
    }

    function langIndex() {
        const cur = BeyGramLang.language
        for (let i = 0; i < page.langOptions.length; ++i) {
            if (page.langOptions[i].code === cur)
                return i
        }
        return 0
    }

    function themeIndex() {
        const cur = BeyGramAppearance.themeMode
        for (let i = 0; i < page.themeOptions.length; ++i) {
            if (page.themeOptions[i].mode === cur)
                return i
        }
        return BeyGramTheme.darkMode ? 1 : 0
    }

    function localizedStageTitle() {
        switch (BeyGramApp.stageName) {
        case "wait_qr_code":
            return qsTr("Scan QR code")
        case "wait_phone_number":
            return qsTr("Enter your phone number")
        case "wait_code":
            return qsTr("Enter login code")
        case "wait_password":
            return qsTr("Two-step verification")
        case "wait_email_address":
            return qsTr("Verify email address")
        case "wait_email_code":
            return qsTr("Enter email code")
        case "wait_registration":
            return qsTr("Create your profile")
        case "loading":
        case "wait_tdlib_parameters":
            return qsTr("Connecting to Telegram…")
        case "closing":
            return qsTr("Closing session…")
        case "closed":
            return qsTr("Session closed")
        case "locked":
            return qsTr("Account is open elsewhere")
        case "unavailable":
            return qsTr("Telegram core unavailable")
        case "not_configured":
            return qsTr("API configuration needed")
        default:
            return BeyGramApp.stageTitle.length > 0
                   ? BeyGramApp.stageTitle
                   : qsTr("Preparing BeyGram…")
        }
    }

    function localizedStageDetail() {
        switch (BeyGramApp.stageName) {
        case "wait_qr_code":
            return qsTr("Use your phone to link this Windows device quickly and securely.")
        case "wait_phone_number":
            return qsTr("Choose your country code and enter your phone number.")
        case "wait_code":
            return BeyGramApp.codePhoneNumber !== ""
                   ? qsTr("We sent a confirmation code to %1.").arg(BeyGramApp.codePhoneNumber)
                   : qsTr("Telegram sent a login code to your other active devices.")
        case "wait_password":
            return qsTr("Enter your cloud password to finish signing in.")
        case "wait_email_address":
            return qsTr("Telegram asked for an email address to protect this account.")
        case "wait_email_code":
            return BeyGramApp.emailPattern !== ""
                   ? qsTr("Enter the code sent to %1.").arg(BeyGramApp.emailPattern)
                   : qsTr("Enter the verification code sent to your email.")
        case "wait_registration":
            return qsTr("Set up your display name to join Telegram.")
        default:
            return BeyGramApp.stageDetail
        }
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
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    function syncStageUi() {
        methodToggle.currentIndex = page.stageIs("wait_qr_code") ? 0 : 1
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

    Connections {
        target: BeyGramLang
        function onLanguageChanged() {
            langSwitch.currentIndex = page.langIndex()
        }
    }

    Connections {
        target: BeyGramAppearance
        function onChanged() {
            themeSwitch.currentIndex = page.themeIndex()
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

    // Subtle warm ambient accent glow matching Android BeyGram.
    Rectangle {
        anchors.fill: parent
        visible: BeyGramTheme.modeName !== "Amoled"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.alpha(BeyGramTheme.accent, BeyGramTheme.darkMode ? 0.12 : 0.08) }
            GradientStop { position: 0.5; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.alpha(BeyGramTheme.accent, BeyGramTheme.darkMode ? 0.06 : 0.05) }
        }
    }

    // Top-right Language & Theme quick switcher (available BEFORE login).
    RowLayout {
        id: topQuickBar
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.topMargin: Design.space4
        anchors.leftMargin: Design.space6
        anchors.rightMargin: Design.space6
        spacing: Design.space3
        z: 10

        RowLayout {
            visible: !page.wideLayout
            spacing: Design.space2

            Rectangle {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                radius: Design.radiusSm
                color: BeyGramTheme.accent

                Text {
                    anchors.centerIn: parent
                    text: "B"
                    color: BeyGramTheme.onAccent
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }
            }

            Text {
                text: "BeyGram"
                color: BeyGramTheme.text
                font.pixelSize: Design.fontBodyLarge
                font.weight: Font.DemiBold
            }
        }

        Item { Layout.fillWidth: true }

        DsSegmented {
            id: langSwitch
            Layout.preferredWidth: 156
            model: page.langOptions.map(function (o) { return o.label })
            currentIndex: page.langIndex()
            Accessible.name: qsTr("Language")
            onActivated: function (index) {
                if (index >= 0 && index < page.langOptions.length)
                    BeyGramLang.setLanguage(page.langOptions[index].code)
            }
        }

        DsSegmented {
            id: themeSwitch
            Layout.preferredWidth: 196
            model: page.themeOptions.map(function (o) { return o.label })
            currentIndex: page.themeIndex()
            Accessible.name: qsTr("Theme mode")
            onActivated: function (index) {
                if (index >= 0 && index < page.themeOptions.length) {
                    const mode = page.themeOptions[index].mode
                    BeyGramAppearance.setThemeMode(mode)
                    BeyGramTheme.modeName = mode
                }
            }
        }
    }

    Flickable {
        id: viewport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topQuickBar.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: Design.space2
        clip: true
        contentWidth: width
        contentHeight: Math.max(height, centerContainer.implicitHeight + Design.space8 * 2)
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        RowLayout {
            id: centerContainer

            width: Math.min(page.wideLayout ? 900 : 460, Math.max(320, viewport.width - Design.space8 * 2))
            x: Math.round((viewport.width - width) / 2)
            y: Math.max(Design.space6, Math.round((viewport.height - implicitHeight) / 2))
            spacing: Design.space8

            // Left welcoming column on wide desktop windows.
            ColumnLayout {
                visible: page.wideLayout
                Layout.fillWidth: true
                Layout.preferredWidth: 360
                Layout.alignment: Qt.AlignVCenter
                spacing: Design.space5

                RowLayout {
                    spacing: Design.space3

                    Rectangle {
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 60
                        radius: Design.radiusXl
                        color: BeyGramTheme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "B"
                            color: BeyGramTheme.onAccent
                            font.pixelSize: 32
                            font.weight: Font.Bold
                        }
                    }

                    ColumnLayout {
                        spacing: 2

                        Text {
                            text: "BeyGram"
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontDisplay
                            font.weight: Font.Bold
                        }

                        Text {
                            text: qsTr("For Windows")
                            color: BeyGramTheme.accentInk
                            font.pixelSize: Design.fontSmall
                            font.weight: Font.DemiBold
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space2

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Fast, calm Telegram messaging on your desktop.")
                        color: BeyGramTheme.text
                        font.pixelSize: Design.fontHeadline
                        font.weight: Font.DemiBold
                        lineHeight: Design.lineTight
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Sign in with a quick QR scan or your phone number. Choose your language and theme above before getting started.")
                        color: BeyGramTheme.muted
                        font.pixelSize: Design.fontBody
                        lineHeight: Design.lineNormal
                        wrapMode: Text.WordWrap
                    }
                }

                DsCard {
                    Layout.fillWidth: true
                    padding: Design.space4
                    spacing: Design.space3

                    content: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            DsIcon {
                                name: "\uE8BD"
                                size: 18
                                color: BeyGramTheme.accentInk
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Real-time cloud sync with your chats and folders")
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            DsIcon {
                                name: "\uE790"
                                size: 18
                                color: BeyGramTheme.accentInk
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Warm, Graphite, Liquid Glass & AMOLED appearances")
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3

                            DsIcon {
                                name: "\uE72E"
                                size: 18
                                color: BeyGramTheme.accentInk
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Session keys encrypted locally with Windows DPAPI")
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            // Right (or centered) sign-in card.
            DsCard {
                id: authCard

                Layout.fillWidth: !page.wideLayout
                Layout.preferredWidth: page.wideLayout ? 440 : -1
                Layout.alignment: Qt.AlignVCenter
                elevated: true
                padding: Design.space6
                spacing: Design.space4

                content: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Design.space4

                    // Compact header on narrow viewports
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space1
                        visible: !page.wideLayout

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Welcome to BeyGram")
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontHeadline
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Sign in to your Telegram account")
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                        }
                    }

                    // Startup / runtime failure
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space2
                        visible: BeyGramApp.startupError !== "" || !BeyGramApp.tdlibAvailable

                        DsBadge {
                            text: BeyGramApp.startupError !== ""
                                  ? qsTr("Startup error")
                                  : qsTr("TDLib unavailable")
                            tone: "danger"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.startupError !== ""
                                  ? qsTr("The local account could not be prepared")
                                  : qsTr("The Telegram core library could not be loaded")
                            color: BeyGramTheme.text
                            font.pixelSize: Design.fontBodyLarge
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: BeyGramApp.startupError !== ""
                                  ? BeyGramApp.startupError
                                  : BeyGramApp.tdlibDetail
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WrapAnywhere
                            lineHeight: Design.lineNormal
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2

                            DsButton {
                                text: qsTr("Retry")
                                onClicked: BeyGramApp.retrySession()
                            }
                            DsButton {
                                text: qsTr("Open data folder")
                                tone: "secondary"
                                onClicked: BeyGramApp.openDataFolder()
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }

                    // Authorization flow
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Design.space3
                        visible: page.authReady

                        // QR Code / Phone toggle (QR primary first)
                        DsSegmented {
                            id: methodToggle
                            Layout.fillWidth: true
                            visible: page.stageIs("wait_phone_number") || page.stageIs("wait_qr_code")
                            model: [qsTr("QR Code"), qsTr("Phone number")]
                            currentIndex: page.stageIs("wait_qr_code") ? 0 : 1
                            onActivated: function(index) {
                                if (index === 0)
                                    BeyGramApp.requestQrLogin()
                                else
                                    BeyGramApp.usePhoneInstead()
                            }
                        }

                        // Stage heading
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: !page.transientStage

                            Text {
                                Layout.fillWidth: true
                                text: page.localizedStageTitle()
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
                            visible: !page.transientStage && page.localizedStageDetail() !== ""
                            text: page.localizedStageDetail()
                            color: BeyGramTheme.muted
                            font.pixelSize: Design.fontSmall
                            wrapMode: Text.WordWrap
                            lineHeight: Design.lineNormal
                        }

                        // Error banner
                        Rectangle {
                            Layout.fillWidth: true
                            visible: BeyGramApp.authError !== ""
                            implicitHeight: authErrorCol.implicitHeight + Design.space3 * 2
                            radius: Design.radiusMd
                            color: Qt.alpha(BeyGramTheme.danger, 0.10)
                            border.width: 1
                            border.color: Qt.alpha(BeyGramTheme.danger, 0.35)

                            ColumnLayout {
                                id: authErrorCol
                                anchors.fill: parent
                                anchors.margins: Design.space3
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Could not sign in")
                                    color: BeyGramTheme.danger
                                    font.pixelSize: Design.fontCaption
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: BeyGramApp.authError
                                    color: BeyGramTheme.danger
                                    font.pixelSize: Design.fontSmall
                                    wrapMode: Text.WordWrap
                                    lineHeight: Design.lineNormal
                                }
                            }
                        }

                        // --- QR login ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space3
                            visible: page.stageIs("wait_qr_code")

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 204
                                Layout.preferredHeight: 204
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
                                      ? qsTr("Open Telegram on your phone → Settings → Devices → Link Desktop Device, then scan this code.")
                                      : qsTr("Requesting a QR code from Telegram…")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontSmall
                                wrapMode: Text.WordWrap
                                lineHeight: Design.lineNormal
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: qsTr("Open in Telegram")
                                tone: "secondary"
                                enabled: BeyGramApp.qrRevision > 0
                                onClicked: BeyGramApp.openQrLoginInTelegram()
                            }
                        }

                        // --- Phone number ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_phone_number")

                            DsTextField {
                                id: phoneField
                                Layout.fillWidth: true
                                label: qsTr("Phone number")
                                placeholderText: "+7 999 123 45 67"
                                inputMethodHints: Qt.ImhDialableCharactersOnly
                                maximumLength: 24
                                clearable: true
                                onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPhone(text) }
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Use the full international format, including your country code.")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                wrapMode: Text.WordWrap
                            }

                            DsButton {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                text: qsTr("Continue")
                                enabled: phoneField.text.trim().length > 0
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitPhone(phoneField.text)
                            }
                        }

                        // --- Login code ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_code")

                            DsTextField {
                                id: codeField
                                Layout.fillWidth: true
                                label: qsTr("Login code")
                                placeholderText: BeyGramApp.codeLength > 0
                                                 ? qsTr("%n digit(s)", "", BeyGramApp.codeLength)
                                                 : qsTr("5-digit code")
                                inputMethodHints: Qt.ImhDigitsOnly
                                maximumLength: BeyGramApp.codeLength > 0 ? BeyGramApp.codeLength : 5
                                font.pixelSize: 18
                                font.letterSpacing: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: TextInput.AlignHCenter
                                onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitCode(text) }
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            Text {
                                Layout.fillWidth: true
                                text: page.resendSeconds > 0
                                      ? qsTr("Resend available in %1").arg(page.formatCountdown(page.resendSeconds))
                                      : qsTr("You can request a new code now.")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                            }

                            DsButton {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                text: qsTr("Sign in")
                                enabled: codeField.text.length > 0
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitCode(codeField.text)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Design.space2

                                DsButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Resend code")
                                    tone: "secondary"
                                    enabled: page.resendSeconds === 0 && !BeyGramApp.busy
                                    onClicked: {
                                        BeyGramApp.resendCode()
                                        page.startResendCountdown()
                                    }
                                }

                                DsButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Back")
                                    tone: "ghost"
                                    onClicked: BeyGramApp.usePhoneInstead()
                                }
                            }
                        }

                        // --- Two-step password ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_password")

                            DsTextField {
                                id: passwordField
                                Layout.fillWidth: true
                                label: qsTr("Two-step verification password")
                                placeholderText: qsTr("Cloud password")
                                echoMode: TextInput.Password
                                onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitPassword(text) }
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: BeyGramApp.passwordHint !== ""
                                text: qsTr("Hint: %1").arg(BeyGramApp.passwordHint)
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: !BeyGramApp.passwordHasRecoveryEmail
                                text: qsTr("This account has no recovery email configured.")
                                color: BeyGramTheme.warnInk
                                font.pixelSize: Design.fontCaption
                                wrapMode: Text.WordWrap
                            }

                            DsButton {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                text: qsTr("Continue")
                                enabled: passwordField.text.length > 0
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitPassword(passwordField.text)
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: qsTr("Back")
                                tone: "ghost"
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }

                        // --- Email address ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_email_address")

                            DsTextField {
                                id: emailField
                                Layout.fillWidth: true
                                label: qsTr("Email address")
                                placeholderText: "name@example.com"
                                inputMethodHints: Qt.ImhEmailCharactersOnly
                                maximumLength: 254
                                clearable: true
                                onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailAddress(text) }
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            DsButton {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                text: qsTr("Continue")
                                enabled: emailField.text.includes("@")
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitEmailAddress(emailField.text)
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: qsTr("Back")
                                tone: "ghost"
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }

                        // --- Email code ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_email_code")

                            DsTextField {
                                id: emailCodeField
                                Layout.fillWidth: true
                                label: qsTr("Email code")
                                placeholderText: BeyGramApp.emailCodeLength > 0
                                                 ? qsTr("%n digit(s)", "", BeyGramApp.emailCodeLength)
                                                 : qsTr("Email code")
                                inputMethodHints: Qt.ImhDigitsOnly
                                maximumLength: BeyGramApp.emailCodeLength > 0
                                               ? BeyGramApp.emailCodeLength : 12
                                font.pixelSize: 18
                                font.letterSpacing: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: TextInput.AlignHCenter
                                onAccepted: { if (!BeyGramApp.busy) BeyGramApp.submitEmailCode(text) }
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            Text {
                                Layout.fillWidth: true
                                text: page.resendSeconds > 0
                                      ? qsTr("Resend available in %1").arg(page.formatCountdown(page.resendSeconds))
                                      : qsTr("You can request a new code now.")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                            }

                            DsButton {
                                Layout.fillWidth: true
                                Layout.topMargin: Design.space1
                                text: qsTr("Sign in")
                                enabled: emailCodeField.text.length > 0
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitEmailCode(emailCodeField.text)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Design.space2

                                DsButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Resend code")
                                    tone: "secondary"
                                    enabled: page.resendSeconds === 0 && !BeyGramApp.busy
                                    onClicked: {
                                        BeyGramApp.resendCode()
                                        page.startResendCountdown()
                                    }
                                }

                                DsButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Back")
                                    tone: "ghost"
                                    onClicked: BeyGramApp.usePhoneInstead()
                                }
                            }
                        }

                        // --- Registration ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Design.space2
                            visible: page.stageIs("wait_registration")

                            DsTextField {
                                id: firstNameField
                                Layout.fillWidth: true
                                label: qsTr("First name")
                                maximumLength: 64
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            DsTextField {
                                id: lastNameField
                                Layout.fillWidth: true
                                label: qsTr("Last name (optional)")
                                maximumLength: 64
                                onTextEdited: BeyGramApp.clearAuthError()
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("By creating an account you confirm you are at least %1 years old and accept Telegram's Terms of Service.")
                                      .arg(BeyGramApp.registrationMinUserAge)
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
                                text: qsTr("Create account")
                                enabled: firstNameField.text.trim().length > 0
                                loading: BeyGramApp.busy
                                onClicked: BeyGramApp.submitRegistration(firstNameField.text,
                                                                        lastNameField.text)
                            }

                            DsButton {
                                Layout.fillWidth: true
                                text: qsTr("Back")
                                tone: "ghost"
                                onClicked: BeyGramApp.usePhoneInstead()
                            }
                        }

                        // --- Loading / connecting / terminal states ---
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
                                text: page.localizedStageTitle()
                                color: BeyGramTheme.text
                                font.pixelSize: Design.fontTitle
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                visible: page.localizedStageDetail() !== ""
                                text: page.localizedStageDetail()
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
                                    text: page.stageIs("closing") ? qsTr("Please wait") : qsTr("Retry")
                                    enabled: !page.stageIs("closing")
                                    onClicked: BeyGramApp.retrySession()
                                }

                                DsButton {
                                    Layout.fillWidth: true
                                    text: qsTr("Restart session")
                                    tone: "secondary"
                                    visible: !page.stageIs("loading")
                                    onClicked: BeyGramApp.usePhoneInstead()
                                }
                            }
                        }
                    }

                    // Collapsible Telegram API credentials (shown prominently only if missing)
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
                                text: qsTr("API configuration")
                                color: BeyGramTheme.muted
                                font.pixelSize: Design.fontCaption
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            DsBadge {
                                text: BeyGramApp.credentialsConfigured
                                      ? qsTr("Ready")
                                      : qsTr("Required")
                                tone: BeyGramApp.credentialsConfigured ? "ok" : "muted"
                            }

                            DsButton {
                                text: page.credentialsOpen ? qsTr("Hide") : qsTr("Configure")
                                tone: "ghost"
                                size: "sm"
                                visible: BeyGramApp.credentialsConfigured
                                onClicked: page.credentialsOpen = !page.credentialsOpen
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: page.credentialsOpen || !BeyGramApp.credentialsConfigured
                            text: BeyGramApp.credentialsConfigured
                                  ? BeyGramApp.credentialsHint
                                  : qsTr("Enter your Telegram API ID and API hash from my.telegram.org to connect.")
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
                                label: qsTr("API ID")
                                placeholderText: "123456"
                                inputMethodHints: Qt.ImhDigitsOnly
                                maximumLength: 12
                            }

                            DsTextField {
                                id: apiHashField
                                Layout.fillWidth: true
                                label: qsTr("API Hash")
                                placeholderText: qsTr("32 hexadecimal characters")
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
                                    text: qsTr("Save credentials")
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
                                    text: qsTr("Forget")
                                    tone: "ghost"
                                    visible: BeyGramApp.credentialsFromStore
                                    accessibleName: qsTr("Forget saved credentials")
                                    onClicked: forgetDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    DsDialog {
        id: forgetDialog
        title: qsTr("Forget saved API credentials?")
        description: qsTr("The DPAPI-protected API ID and API hash will be removed from this Windows account. Your Telegram session stays intact until restart.")
        primaryText: qsTr("Forget")
        secondaryText: qsTr("Cancel")
        tone: "danger"
        onAccepted: {
            BeyGramApp.clearStoredCredentials()
            page.credentialsOpen = true
        }
    }
}
