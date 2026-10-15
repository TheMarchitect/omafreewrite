import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Where Polish sends the text: any OpenAI-compatible chat server (Ollama on
// this machine by default) or the Anthropic API, plus the model to ask for.
Popup {
    id: settings

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color highlightColor: "#428bca"
    property string defaultFont: "IBM Plex Mono"

    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * textScale));
    }

    modal: false
    focus: true
    padding: 14
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Material.elevation: 8
    Material.accent: highlightColor

    onAboutToShow: {
        urlField.text = polish.baseUrl;
        keyField.text = polish.apiKey;
        modelBox.editText = polish.model;
        polish.refreshModels();
    }

    onClosed: commit()

    function commit() {
        polish.baseUrl = urlField.text;
        polish.apiKey = keyField.text;
        polish.model = modelBox.editText;
    }

    background: Rectangle {
        radius: 9
        color: settings.darkMode ? "#22221f" : "#fffef2"
    }

    contentItem: Column {
        spacing: 8
        width: settings.width - settings.leftPadding - settings.rightPadding

        Row {
            width: parent.width
            spacing: 8

            Label {
                text: "Polish"
                color: settings.textColor
                font.family: settings.defaultFont
                font.pixelSize: settings.scaledSize(15)
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Item { width: parent.width - polishSwitch.width - 60; height: 1 }

            Switch {
                id: polishSwitch
                objectName: "polishSwitch"
                checked: polish.enabled
                onToggled: polish.enabled = checked
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Label {
            width: parent.width
            text: "On Escape, the words from this focus session get their spelling, "
                  + "punctuation and paragraphs fixed. The words themselves never change. "
                  + "Ctrl+Z undoes a pass. Shift+Escape skips it."
            color: settings.mutedColor
            wrapMode: Text.Wrap
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(11)
        }

        Label {
            text: "Server"
            color: settings.mutedColor
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(11)
        }

        TextField {
            id: urlField
            objectName: "polishUrlField"
            width: parent.width
            color: settings.textColor
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(13)
            placeholderText: "http://127.0.0.1:11434"
            selectByMouse: true
            onEditingFinished: {
                polish.baseUrl = text;
                polish.refreshModels();
            }
        }

        Label {
            text: "Model"
            color: settings.mutedColor
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(11)
        }

        Row {
            width: parent.width
            spacing: 8

            ComboBox {
                id: modelBox
                objectName: "polishModelBox"
                width: parent.width - refreshButton.width - 8
                editable: true
                model: polish.availableModels
                font.family: settings.defaultFont
                font.pixelSize: settings.scaledSize(13)
                onAccepted: polish.model = editText
                onActivated: polish.model = currentText
                Connections {
                    target: polish
                    function onAvailableModelsChanged() {
                        var index = polish.availableModels.indexOf(polish.model);
                        modelBox.editText = polish.model;
                        if (index >= 0)
                            modelBox.currentIndex = index;
                    }
                }
            }

            Button {
                id: refreshButton
                text: "Refresh"
                flat: true
                font.family: settings.defaultFont
                font.pixelSize: settings.scaledSize(11)
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    settings.commit();
                    polish.refreshModels();
                }
            }
        }

        Label {
            text: "API key (blank for Ollama)"
            color: settings.mutedColor
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(11)
        }

        TextField {
            id: keyField
            objectName: "polishKeyField"
            width: parent.width
            color: settings.textColor
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(13)
            echoMode: TextInput.Password
            selectByMouse: true
            onEditingFinished: polish.apiKey = text
        }

        Label {
            width: parent.width
            text: polish.modelsStatus
            color: settings.mutedColor
            elide: Text.ElideRight
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(11)
        }

        Label {
            width: parent.width
            text: "Anthropic: https://api.anthropic.com with a key and claude-haiku-5-5. "
                  + "Any OpenAI-style server works the same way."
            color: settings.mutedColor
            wrapMode: Text.Wrap
            font.family: settings.defaultFont
            font.pixelSize: settings.scaledSize(10)
        }
    }
}
