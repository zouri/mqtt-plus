pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components"

Dialog {
    id: root

    required property AppUi ui
    required property var appController

    property string mode: "create"
    property int targetIndex: -1
    property string dialogTitle: qsTr("Connection")
    property string validationError: ""

    component FormLabel : Label {
        Layout.preferredWidth: 148
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        color: root.ui.textMuted
        font.pixelSize: 12
        horizontalAlignment: Text.AlignRight
        wrapMode: Text.WordWrap
    }

    component SectionTitle : Label {
        Layout.fillWidth: true
        color: root.ui.textStrong
        font.pixelSize: 13
        font.bold: true
    }

    component FormSection : Rectangle {
        default property alias content: sectionLayout.data

        Layout.fillWidth: true
        implicitHeight: sectionLayout.implicitHeight + 28
        radius: root.ui.innerRadius
        color: root.ui.themePalette.innerPanelBg
        border.color: root.ui.themePalette.innerPanelBorder

        GridLayout {
            id: sectionLayout
            anchors.fill: parent
            anchors.margins: 14
            columns: 2
            columnSpacing: 14
            rowSpacing: 12
        }
    }

    component BrowseField : RowLayout {
        id: browseField

        property alias text: field.text
        property string dialogTitle: qsTr("Select file")

        Layout.fillWidth: true
        spacing: 8

        AppTextField {
            id: field
            ui: root.ui
            Layout.fillWidth: true
            placeholderText: qsTr("Optional file path")
        }

        AppButton {
            ui: root.ui
            text: qsTr("Browse")
            minimumWidth: 76
            onClicked: {
                certificateFileDialog.title = browseField.dialogTitle
                certificateFileDialog.targetField = field
                certificateFileDialog.open()
            }
        }
    }

    function filePathFromUrl(url) {
        return decodeURIComponent(String(url).replace("file://", ""))
    }

    function optionalNumberText(field) {
        return field.text.trim()
    }

    function generateClientId() {
        return `mqtt-plus-${Math.floor(100000 + Math.random() * 900000)}`
    }

    function syncTransportPort() {
        const tlsSelected = transportField.currentIndex === 1
        if (tlsSelected && portField.text.trim() === "1883") {
            portField.text = "8883"
        } else if (!tlsSelected && portField.text.trim() === "8883") {
            portField.text = "1883"
        }
    }

    function loadConfig(config) {
        sessionNameField.text = config.name !== undefined ? config.name : ""
        hostField.text = config.host !== undefined ? config.host : "broker.emqx.io"
        portField.text = `${config.port !== undefined ? config.port : 1883}`
        transportField.currentIndex = config.transport === "tls" ? 1 : 0
        protocolField.currentIndex = config.protocolVersion === 4 ? 1 : 0
        sslSecureField.checked = config.sslSecure !== undefined ? config.sslSecure : true
        alpnField.text = config.alpn !== undefined ? config.alpn : ""
        caSignedRadio.checked = config.certificateType !== "self"
        selfSignedRadio.checked = config.certificateType === "self"
        caFileField.text = config.caFile !== undefined ? config.caFile : ""
        clientCertificateField.text = config.clientCertificateFile !== undefined ? config.clientCertificateFile : ""
        clientKeyField.text = config.clientKeyFile !== undefined ? config.clientKeyFile : ""
        clientIdField.text = config.clientId !== undefined ? config.clientId : ""
        usernameField.text = config.username !== undefined ? config.username : ""
        passwordField.text = config.password !== undefined ? config.password : ""
        connectTimeoutField.text = `${config.connectTimeoutSeconds !== undefined ? config.connectTimeoutSeconds : 10}`
        keepAliveField.text = `${config.keepAliveSeconds !== undefined ? config.keepAliveSeconds : 30}`
        cleanSessionField.checked = config.cleanSession !== undefined ? config.cleanSession : true
        sessionExpiryField.text = `${config.sessionExpiryInterval !== undefined ? config.sessionExpiryInterval : 0}`
        receiveMaximumField.text = config.receiveMaximum !== undefined ? config.receiveMaximum : ""
        maximumPacketSizeField.text = config.maximumPacketSize !== undefined ? config.maximumPacketSize : ""
        topicAliasMaximumField.text = config.topicAliasMaximum !== undefined ? config.topicAliasMaximum : ""
        requestResponseInformationField.checked = config.requestResponseInformation !== undefined
                ? config.requestResponseInformation : false
        requestProblemInformationField.checked = config.requestProblemInformation !== undefined
                ? config.requestProblemInformation : false
        authenticationMethodField.text = config.authenticationMethod !== undefined ? config.authenticationMethod : ""
        authenticationDataField.text = config.authenticationData !== undefined ? config.authenticationData : ""
    }

    function collectedConfig() {
        return {
            "name": sessionNameField.text,
            "host": hostField.text,
            "port": Number(portField.text.trim()),
            "transport": transportField.currentIndex === 1 ? "tls" : "tcp",
            "protocolVersion": protocolField.currentIndex === 1 ? 4 : 5,
            "sslSecure": sslSecureField.checked,
            "alpn": alpnField.text,
            "certificateType": selfSignedRadio.checked ? "self" : "ca",
            "caFile": caFileField.text,
            "clientCertificateFile": clientCertificateField.text,
            "clientKeyFile": clientKeyField.text,
            "clientId": clientIdField.text,
            "username": usernameField.text,
            "password": passwordField.text,
            "connectTimeoutSeconds": Number(connectTimeoutField.text.trim()),
            "keepAliveSeconds": Number(keepAliveField.text.trim()),
            "cleanSession": cleanSessionField.checked,
            "sessionExpiryInterval": optionalNumberText(sessionExpiryField),
            "receiveMaximum": optionalNumberText(receiveMaximumField),
            "maximumPacketSize": optionalNumberText(maximumPacketSizeField),
            "topicAliasMaximum": optionalNumberText(topicAliasMaximumField),
            "requestResponseInformation": requestResponseInformationField.checked,
            "requestProblemInformation": requestProblemInformationField.checked,
            "authenticationMethod": authenticationMethodField.text,
            "authenticationData": authenticationDataField.text
        }
    }

    function validateInteger(text, label, minimum, maximum, required) {
        const trimmed = text.trim()
        if (trimmed.length === 0) {
            return required ? qsTr("%1 is required.").arg(label) : ""
        }
        if (!/^\d+$/.test(trimmed)) {
            return qsTr("%1 must be an integer.").arg(label)
        }
        const value = Number(trimmed)
        if (value < minimum || value > maximum) {
            return qsTr("%1 must be between %2 and %3.").arg(label).arg(minimum).arg(maximum)
        }
        return ""
    }

    function validationErrorMessage() {
        if (sessionNameField.text.trim().length === 0) {
            return qsTr("Name is required.")
        }
        if (hostField.text.trim().length === 0) {
            return qsTr("Server address is required.")
        }

        const checks = [
            validateInteger(portField.text, qsTr("Port"), 1, 65535, true),
            validateInteger(connectTimeoutField.text, qsTr("Connection timeout"), 1, 300, true),
            validateInteger(keepAliveField.text, qsTr("Keep Alive"), 5, 1200, true),
            validateInteger(sessionExpiryField.text, qsTr("Session expiry interval"), 0, 4294967295, false),
            validateInteger(receiveMaximumField.text, qsTr("Receive maximum"), 1, 65535, false),
            validateInteger(maximumPacketSizeField.text, qsTr("Maximum packet size"), 1, 4294967295, false),
            validateInteger(topicAliasMaximumField.text, qsTr("Topic alias maximum"), 1, 65535, false)
        ]

        for (const message of checks) {
            if (message.length > 0) {
                return message
            }
        }
        return ""
    }

    function openForCreate() {
        mode = "create"
        targetIndex = -1
        dialogTitle = qsTr("New Connection")
        validationError = ""
        loadConfig(appController.defaultSessionConfig())
        open()
    }

    function openForEdit(index) {
        if (index < 0 || index >= appController.sessionsModel.length) {
            return
        }

        mode = "edit"
        targetIndex = index
        dialogTitle = qsTr("Edit Connection")
        validationError = ""
        loadConfig(appController.sessionConfigAt(index))
        open()
    }

    function submit() {
        validationError = validationErrorMessage()
        if (validationError.length > 0) {
            return
        }

        const config = collectedConfig()
        if (mode === "create") {
            appController.addSessionWithConfig(config)
            close()
            return
        }

        if (targetIndex >= 0 && appController.updateSessionConfigAt(targetIndex, config)) {
            close()
        }
    }

    modal: true
    focus: true
    width: Math.min(900, Overlay.overlay ? Overlay.overlay.width - 44 : 900)
    height: Math.min(760, Overlay.overlay ? Overlay.overlay.height - 44 : 760)
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.NoButton

    Overlay.modal: Rectangle {
        color: root.ui.themePalette.dialogOverlay
    }

    header: Item {
        implicitHeight: 0
        visible: false
    }

    background: Rectangle {
        radius: 18
        color: root.ui.themePalette.dialogBg
        border.color: root.ui.themePalette.dialogBorder
    }

    FileDialog {
        id: certificateFileDialog
        property var targetField: null

        onAccepted: {
            if (targetField) {
                targetField.text = root.filePathFromUrl(selectedFile)
            }
        }
    }

    ButtonGroup {
        id: certificateTypeGroup
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: root.dialogTitle
                color: root.ui.textStrong
                font.pixelSize: 18
                font.bold: true
            }

            AppIconButton {
                ui: root.ui
                iconSource: root.ui.materialIcon("xmark")
                iconSize: 15
                implicitWidth: 34
                implicitHeight: 34
                toolTipText: qsTr("Cancel")
                onClicked: root.close()
            }
        }

        Flickable {
            id: formFlick

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: formContent.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ScrollBar.vertical: ScrollBar { }

            ColumnLayout {
                id: formContent

                width: formFlick.width
                spacing: 14

                SectionTitle { text: qsTr("Basic") }
                FormSection {
                    FormLabel { text: qsTr("* Name") }
                    AppTextField {
                        id: sessionNameField
                        ui: root.ui
                        Layout.fillWidth: true
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("* Server address") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        AppComboBox {
                            id: transportField
                            ui: root.ui
                            Layout.preferredWidth: 132
                            model: ["mqtt://", "mqtts://"]
                            onCurrentIndexChanged: root.syncTransportPort()
                        }

                        AppTextField {
                            id: hostField
                            ui: root.ui
                            Layout.fillWidth: true
                            placeholderText: qsTr("broker.emqx.io")
                            onTextChanged: root.validationError = ""
                        }
                    }

                    FormLabel { text: qsTr("* Port") }
                    AppTextField {
                        id: portField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 65535 }
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Client ID") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        AppTextField {
                            id: clientIdField
                            ui: root.ui
                            Layout.fillWidth: true
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("Generate")
                            minimumWidth: 92
                            onClicked: clientIdField.text = root.generateClientId()
                        }
                    }

                    FormLabel { text: qsTr("Username") }
                    AppTextField {
                        id: usernameField
                        ui: root.ui
                        Layout.fillWidth: true
                    }

                    FormLabel { text: qsTr("Password") }
                    AppTextField {
                        id: passwordField
                        ui: root.ui
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                    }
                }

                SectionTitle { text: qsTr("Certificates") }
                FormSection {
                    FormLabel { text: qsTr("SSL secure") }
                    AppCheckBox {
                        id: sslSecureField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Verify server certificate")
                    }

                    FormLabel { text: qsTr("ALPN") }
                    AppTextField {
                        id: alpnField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("Optional, comma separated")
                    }

                    FormLabel { text: qsTr("Certificate type") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 18

                        RadioButton {
                            id: caSignedRadio
                            ButtonGroup.group: certificateTypeGroup
                            text: qsTr("CA signed server certificate")
                            checked: true
                            font.pixelSize: root.ui.compactFontSize
                            palette.windowText: root.ui.textStrong
                        }

                        RadioButton {
                            id: selfSignedRadio
                            ButtonGroup.group: certificateTypeGroup
                            text: qsTr("CA or self signed certificates")
                            font.pixelSize: root.ui.compactFontSize
                            palette.windowText: root.ui.textStrong
                        }
                    }

                    FormLabel {
                        text: qsTr("CA file")
                        visible: selfSignedRadio.checked
                    }
                    BrowseField {
                        id: caFileField
                        visible: selfSignedRadio.checked
                        dialogTitle: qsTr("Select CA file")
                    }

                    FormLabel {
                        text: qsTr("Client certificate")
                        visible: selfSignedRadio.checked
                    }
                    BrowseField {
                        id: clientCertificateField
                        visible: selfSignedRadio.checked
                        dialogTitle: qsTr("Select client certificate")
                    }

                    FormLabel {
                        text: qsTr("Client key file")
                        visible: selfSignedRadio.checked
                    }
                    BrowseField {
                        id: clientKeyField
                        visible: selfSignedRadio.checked
                        dialogTitle: qsTr("Select client key file")
                    }
                }

                SectionTitle { text: qsTr("Advanced") }
                FormSection {
                    FormLabel { text: qsTr("MQTT version") }
                    AppComboBox {
                        id: protocolField
                        ui: root.ui
                        Layout.fillWidth: true
                        model: ["5.0", "3.1.1"]
                    }

                    FormLabel { text: qsTr("Connection timeout") }
                    AppTextField {
                        id: connectTimeoutField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 300 }
                        placeholderText: qsTr("Seconds")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Keep Alive") }
                    AppTextField {
                        id: keepAliveField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 5; top: 1200 }
                        placeholderText: qsTr("Seconds")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Clean Start") }
                    AppCheckBox {
                        id: cleanSessionField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Start with a clean broker session")
                    }

                    FormLabel { text: qsTr("Session expiry interval") }
                    AppTextField {
                        id: sessionExpiryField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Seconds")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Receive maximum") }
                    AppTextField {
                        id: receiveMaximumField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Maximum packet size") }
                    AppTextField {
                        id: maximumPacketSizeField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Topic alias maximum") }
                    AppTextField {
                        id: topicAliasMaximumField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        onTextChanged: root.validationError = ""
                    }

                    FormLabel { text: qsTr("Request response information") }
                    AppCheckBox {
                        id: requestResponseInformationField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Ask broker for response information")
                    }

                    FormLabel { text: qsTr("Request problem information") }
                    AppCheckBox {
                        id: requestProblemInformationField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Ask broker for problem details")
                    }

                    FormLabel { text: qsTr("Auth method") }
                    AppTextField {
                        id: authenticationMethodField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("MQTT 5 enhanced auth method")
                    }

                    FormLabel { text: qsTr("Auth data") }
                    AppTextField {
                        id: authenticationDataField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("MQTT 5 enhanced auth data")
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                visible: root.validationError.length > 0
                text: root.validationError
                color: root.ui.themePalette.errorText
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Item {
                visible: root.validationError.length === 0
                Layout.fillWidth: true
            }

            AppButton {
                ui: root.ui
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            AppButton {
                ui: root.ui
                primary: true
                text: root.mode === "create" ? qsTr("Create") : qsTr("Save")
                onClicked: root.submit()
            }
        }
    }
}
