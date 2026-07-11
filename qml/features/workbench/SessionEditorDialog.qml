pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "../../components"

Dialog {
    id: root

    required property AppUi ui
    required property var viewModel

    readonly property var editor: root.viewModel.sessionEditor
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

    component FormSection : GridLayout {
        id: sectionLayout

        Layout.fillWidth: true
        columns: 2
        columnSpacing: 14
        rowSpacing: 12
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

    function generateClientId() {
        return `mqtt-plus-${Math.floor(100000 + Math.random() * 900000)}`
    }

    function openForCreate() {
        root.viewModel.openSessionEditorForCreate()
        open()
    }

    function openForEdit(index) {
        if (index < 0 || index >= viewModel.sessions.count) {
            return
        }

        root.viewModel.openSessionEditorForEdit(index)
        open()
    }

    function submit() {
        if (root.viewModel.submitSessionEditor()) {
            close()
        }
    }

    modal: true
    dim: true
    focus: true
    width: Math.min(900, Overlay.overlay ? Overlay.overlay.width - 44 : 900)
    height: Math.min(760, Overlay.overlay ? Overlay.overlay.height - 44 : 760)
    anchors.centerIn: Overlay.overlay
    transformOrigin: Popup.Center
    standardButtons: Dialog.NoButton

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            property: "scale"
            from: 0.92
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: 160
            easing.type: Easing.InCubic
        }

        NumberAnimation {
            property: "scale"
            from: 1
            to: 0.96
            duration: 160
            easing.type: Easing.InCubic
        }
    }

    Overlay.modal: AppDialogOverlay {
        ui: root.ui
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
                text: root.editor.title
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
                accessibleName: qsTr("Cancel")
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
                        text: root.editor.name
                        onTextEdited: root.editor.name = text
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
                            currentIndex: root.editor.transport === "tls" ? 1 : 0
                            onActivated: root.editor.transport = currentIndex === 1 ? "tls" : "tcp"
                        }

                        AppTextField {
                            id: hostField
                            ui: root.ui
                            Layout.fillWidth: true
                            text: root.editor.host
                            placeholderText: qsTr("broker.emqx.io")
                            onTextEdited: root.editor.host = text
                        }
                    }

                    FormLabel { text: qsTr("* Port") }
                    AppTextField {
                        id: portField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 65535 }
                        text: root.editor.portText
                        onTextEdited: root.editor.portText = text
                    }

                    FormLabel { text: qsTr("Client ID") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        AppTextField {
                            id: clientIdField
                            ui: root.ui
                            Layout.fillWidth: true
                            text: root.editor.clientId
                            onTextEdited: root.editor.clientId = text
                        }

                        AppButton {
                            ui: root.ui
                            text: qsTr("Generate")
                            minimumWidth: 92
                            onClicked: root.editor.clientId = root.generateClientId()
                        }
                    }

                    FormLabel { text: qsTr("Username") }
                    AppTextField {
                        id: usernameField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: root.editor.username
                        onTextEdited: root.editor.username = text
                    }

                    FormLabel { text: qsTr("Password") }
                    AppTextField {
                        id: passwordField
                        ui: root.ui
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text: root.editor.password
                        onTextEdited: root.editor.password = text
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
                        checked: root.editor.sslSecure
                        onToggled: root.editor.sslSecure = checked
                    }

                    FormLabel { text: qsTr("ALPN") }
                    AppTextField {
                        id: alpnField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("Optional, comma separated")
                        text: root.editor.alpn
                        onTextEdited: root.editor.alpn = text
                    }

                    FormLabel { text: qsTr("Certificate type") }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 18

                        RadioButton {
                            id: caSignedRadio
                            ButtonGroup.group: certificateTypeGroup
                            text: qsTr("CA signed server certificate")
                            checked: root.editor.certificateType !== "self"
                            font.pixelSize: root.ui.compactFontSize
                            palette.windowText: root.ui.textStrong
                            onToggled: {
                                if (checked) {
                                    root.editor.certificateType = "ca"
                                }
                            }

                            HoverHandler {
                                cursorShape: caSignedRadio.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }

                        RadioButton {
                            id: selfSignedRadio
                            ButtonGroup.group: certificateTypeGroup
                            text: qsTr("CA or self signed certificates")
                            checked: root.editor.certificateType === "self"
                            font.pixelSize: root.ui.compactFontSize
                            palette.windowText: root.ui.textStrong
                            onToggled: {
                                if (checked) {
                                    root.editor.certificateType = "self"
                                }
                            }

                            HoverHandler {
                                cursorShape: selfSignedRadio.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
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
                        text: root.editor.caFile
                        onTextChanged: root.editor.caFile = text
                    }

                    FormLabel {
                        text: qsTr("Client certificate")
                        visible: selfSignedRadio.checked
                    }
                    BrowseField {
                        id: clientCertificateField
                        visible: selfSignedRadio.checked
                        dialogTitle: qsTr("Select client certificate")
                        text: root.editor.clientCertificateFile
                        onTextChanged: root.editor.clientCertificateFile = text
                    }

                    FormLabel {
                        text: qsTr("Client key file")
                        visible: selfSignedRadio.checked
                    }
                    BrowseField {
                        id: clientKeyField
                        visible: selfSignedRadio.checked
                        dialogTitle: qsTr("Select client key file")
                        text: root.editor.clientKeyFile
                        onTextChanged: root.editor.clientKeyFile = text
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
                        currentIndex: root.editor.protocolVersion === 4 ? 1 : 0
                        onActivated: root.editor.protocolVersion = currentIndex === 1 ? 4 : 5
                    }

                    FormLabel { text: qsTr("Connection timeout") }
                    AppTextField {
                        id: connectTimeoutField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 300 }
                        placeholderText: qsTr("Seconds")
                        text: root.editor.connectTimeoutText
                        onTextEdited: root.editor.connectTimeoutText = text
                    }

                    FormLabel { text: qsTr("Keep Alive") }
                    AppTextField {
                        id: keepAliveField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 5; top: 1200 }
                        placeholderText: qsTr("Seconds")
                        text: root.editor.keepAliveText
                        onTextEdited: root.editor.keepAliveText = text
                    }

                    FormLabel { text: qsTr("Clean Start") }
                    AppCheckBox {
                        id: cleanSessionField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Start with a clean broker session")
                        checked: root.editor.cleanSession
                        onToggled: root.editor.cleanSession = checked
                    }

                    FormLabel { text: qsTr("Session expiry interval") }
                    AppTextField {
                        id: sessionExpiryField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Seconds")
                        text: root.editor.sessionExpiryText
                        onTextEdited: root.editor.sessionExpiryText = text
                    }

                    FormLabel { text: qsTr("Receive maximum") }
                    AppTextField {
                        id: receiveMaximumField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        text: root.editor.receiveMaximumText
                        onTextEdited: root.editor.receiveMaximumText = text
                    }

                    FormLabel { text: qsTr("Maximum packet size") }
                    AppTextField {
                        id: maximumPacketSizeField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        text: root.editor.maximumPacketSizeText
                        onTextEdited: root.editor.maximumPacketSizeText = text
                    }

                    FormLabel { text: qsTr("Topic alias maximum") }
                    AppTextField {
                        id: topicAliasMaximumField
                        ui: root.ui
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhDigitsOnly
                        placeholderText: qsTr("Optional")
                        text: root.editor.topicAliasMaximumText
                        onTextEdited: root.editor.topicAliasMaximumText = text
                    }

                    FormLabel { text: qsTr("Request response information") }
                    AppCheckBox {
                        id: requestResponseInformationField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Ask broker for response information")
                        checked: root.editor.requestResponseInformation
                        onToggled: root.editor.requestResponseInformation = checked
                    }

                    FormLabel { text: qsTr("Request problem information") }
                    AppCheckBox {
                        id: requestProblemInformationField
                        ui: root.ui
                        Layout.fillWidth: true
                        text: qsTr("Ask broker for problem details")
                        checked: root.editor.requestProblemInformation
                        onToggled: root.editor.requestProblemInformation = checked
                    }

                    FormLabel { text: qsTr("Auth method") }
                    AppTextField {
                        id: authenticationMethodField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("MQTT 5 enhanced auth method")
                        text: root.editor.authenticationMethod
                        onTextEdited: root.editor.authenticationMethod = text
                    }

                    FormLabel { text: qsTr("Auth data") }
                    AppTextField {
                        id: authenticationDataField
                        ui: root.ui
                        Layout.fillWidth: true
                        placeholderText: qsTr("MQTT 5 enhanced auth data")
                        text: root.editor.authenticationData
                        onTextEdited: root.editor.authenticationData = text
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                visible: root.editor.validationError.length > 0
                text: root.editor.validationError
                color: root.ui.themePalette.errorText
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Item {
                visible: root.editor.validationError.length === 0
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
                text: root.editor.editMode ? qsTr("Save") : qsTr("Create")
                onClicked: root.submit()
            }
        }
    }
}
