pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../../components"

ColumnLayout {
    id: root

    required property AppUi ui
    required property var viewModel
    required property var editor

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        ColumnLayout {
            Layout.preferredWidth: 330
            spacing: 6

            Label {
                text: qsTr("Processor name")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppTextField {
                id: nameField

                ui: root.ui
                Layout.fillWidth: true
                text: root.editor.name
                placeholderText: qsTr("Processor name")
                onTextEdited: root.editor.name = text
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Description")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                text: root.editor.description
                placeholderText: qsTr("Device protocol or payload structure")
                onTextEdited: root.editor.description = text
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        ColumnLayout {
            Layout.preferredWidth: 190
            spacing: 6

            Label {
                text: qsTr("Language")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppComboBox {
                ui: root.ui
                Layout.fillWidth: true
                model: root.editor.languageOptionNames
                currentIndex: root.editor.languageIndex
                enabled: !root.editor.archived
                onActivated: root.editor.languageIndex = currentIndex
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 230
            spacing: 6

            Label {
                text: qsTr("Runtime")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                text: root.editor.runtimeName
                readOnly: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: qsTr("Entry file")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                text: root.editor.entryFile
                enabled: !root.editor.archived
                onTextEdited: root.editor.entryFile = text
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 180
            spacing: 6

            Label {
                text: qsTr("Entry symbol")
                color: root.ui.textMuted
                font.pixelSize: 12
            }

            AppTextField {
                ui: root.ui
                Layout.fillWidth: true
                text: root.editor.entrySymbol
                enabled: !root.editor.archived
                onTextEdited: root.editor.entrySymbol = text
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("%1 source").arg(root.editor.languageId === "javascript"
                                                ? "JavaScript"
                                                : "Lua")
                    color: root.ui.textMuted
                    font.pixelSize: 12
                }

                AppBadge {
                    ui: root.ui
                    label: root.editor.selectedRevisionId === root.editor.currentRevisionId
                           ? qsTr("Current revision")
                           : qsTr("Historical revision")
                    visible: root.editor.currentProcessorId.length > 0
                    horizontalPadding: 8
                    verticalPadding: 3
                }
            }

            AppTextArea {
                ui: root.ui
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 310
                text: root.editor.source
                enabled: !root.editor.archived
                clip: true
                showLineNumbers: true
                wrapMode: TextEdit.NoWrap
                placeholderText: root.editor.languageId === "javascript"
                                 ? qsTr("function process(context)")
                                 : qsTr("function process(context)")
                onTextChanged: {
                    if (text !== root.editor.source) {
                        root.editor.source = text
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: diagnosticsText.implicitHeight + 18
                visible: root.editor.diagnostics.length > 0
                radius: 8
                color: root.ui.themePalette.innerPanelBg
                border.color: root.editor.validationOk
                              ? root.ui.stateColor("completed")
                              : root.ui.themePalette.innerPanelBorder

                Label {
                    id: diagnosticsText

                    anchors.fill: parent
                    anchors.margins: 9
                    text: root.editor.diagnostics
                    color: root.ui.textMuted
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 270
            Layout.fillHeight: true
            radius: 10
            color: root.ui.themePalette.innerPanelBg
            border.color: root.ui.themePalette.innerPanelBorder

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    text: qsTr("Revision history")
                    color: root.ui.textStrong
                    font.pixelSize: 13
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0

                    ListView {
                        anchors.fill: parent
                        clip: true
                        spacing: 6
                        model: root.editor.revisions
                        reuseItems: true

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: ItemDelegate {
                            id: revisionDelegate

                            required property int index
                            required property string id
                            required property int revisionNumber
                            required property string languageName
                            required property string runtimeId
                            required property string createdAt
                            required property bool current
                            required property string readinessState

                            width: ListView.view.width
                            implicitHeight: 58
                            hoverEnabled: true
                            leftPadding: 9
                            rightPadding: 9
                            topPadding: 6
                            bottomPadding: 6
                            Accessible.name: qsTr("Processor Revision %1").arg(revisionDelegate.revisionNumber)
                            onClicked: root.viewModel.selectRevisionAt(revisionDelegate.index)

                            contentItem: ColumnLayout {
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Revision %1 · %2").arg(
                                                  revisionDelegate.revisionNumber).arg(
                                                  revisionDelegate.languageName)
                                        color: root.ui.textStrong
                                        font.pixelSize: 11
                                        font.bold: revisionDelegate.current
                                        elide: Label.ElideRight
                                    }

                                    Label {
                                        text: revisionDelegate.current
                                              ? qsTr("Current")
                                              : (revisionDelegate.readinessState === "ready"
                                                 ? qsTr("Ready")
                                                 : qsTr("Unavailable"))
                                        color: revisionDelegate.readinessState === "ready"
                                               ? root.ui.stateColor("completed")
                                               : root.ui.textMuted
                                        font.pixelSize: 9
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: `${revisionDelegate.runtimeId} · ${revisionDelegate.createdAt}`
                                    color: root.ui.themePalette.textSubtle
                                    font.pixelSize: 10
                                    elide: Label.ElideRight
                                }
                            }

                            background: Rectangle {
                                radius: 7
                                color: revisionDelegate.id === root.editor.selectedRevisionId
                                       ? root.ui.themePalette.selectedBg
                                       : (revisionDelegate.hovered
                                          ? root.ui.themePalette.actionHoverBg
                                          : "transparent")
                                border.color: revisionDelegate.id === root.editor.selectedRevisionId
                                              ? root.ui.themePalette.selectedBorder
                                              : "transparent"
                            }
                        }
                    }
                }
            }
        }
    }
}
