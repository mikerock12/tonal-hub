import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: panel
    objectName: "audioEffectsPanel"
    property int instantiatedBandCount: 0
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(790, parent ? parent.width - 32 : 790)
    height: Math.min(740, parent ? parent.height - 32 : 740)
    modal: true
    focus: true
    padding: 24
    property var state: App.audio.settings
    background: Rectangle { color: Theme.elevated; radius: 12; border.color: Theme.divider }
    Overlay.modal: Rectangle { color: Qt.rgba(0,0,0,0.6) }

    FileDialog {
        id: picker
        title: qsTr("Abrir áudio local")
        nameFilters: [qsTr("Áudio (*.mp3 *.flac *.wav *.aac *.m4a *.ogg *.opus *.wma *.aiff *.iamf *.mp4)"), qsTr("Todos os arquivos (*)")]
        onAccepted: App.audio.openFile(selectedFile)
    }
    Dialog {
        id: confirmReset
        anchors.centerIn: parent
        title: qsTr("Restaurar todas as configurações de áudio?")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: App.audio.reset()
    }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Áudio e efeitos"); color: Theme.textPrimary; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
            Button { text: qsTr("Fechar"); onClicked: panel.close() }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 14
                Label { text: qsTr("Efeitos aplicados apenas ao áudio reproduzido neste computador."); wrapMode: Text.WordWrap; color: Theme.textSecondary; Layout.fillWidth: true }
                CheckBox { text: qsTr("Motor de áudio avançado"); checked: panel.state.enabled; onToggled: App.audio.setSetting("enabled", checked) }
                RowLayout {
                    Button { text: qsTr("Abrir arquivo…"); onClicked: picker.open() }
                    Button { text: qsTr("Voltar ao Spotify"); visible: App.audio.localActive; onClicked: App.audio.returnToSpotify() }
                    Label { text: App.audio.localActive ? App.audio.localName : qsTr("Spotify / librespot"); color: Theme.textPrimary; elide: Text.ElideMiddle; Layout.fillWidth: true }
                }
                Label { text: App.audio.diagnostics; wrapMode: Text.WordWrap; color: Theme.accent; Layout.fillWidth: true; font.pixelSize: 12 }
                Label {
                    visible: App.audio.metrics.sampleRate !== undefined
                    text: qsTr("Pico pré-volume: %1 dBFS · DSP: %2 µs · inválidas: %3").arg(Number(App.audio.metrics.peakDbfs !== undefined ? App.audio.metrics.peakDbfs : -120).toFixed(1)).arg(Number(App.audio.metrics.averageDspUs || 0).toFixed(0)).arg(App.audio.metrics.invalidSamples || 0)
                    color: Theme.textMuted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Rectangle { height: 1; color: Theme.divider; Layout.fillWidth: true }
                RowLayout {
                    CheckBox { text: qsTr("Equalizador"); checked: panel.state.eqEnabled; enabled: panel.state.enabled; onToggled: App.audio.setSetting("eqEnabled", checked) }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        id: presets
                        model: ["Flat", "Rock", "Pop", "Clássica", "Eletrônica", "Vocal", "Personalizada"]
                        currentIndex: Math.max(0, model.indexOf(panel.state.preset))
                        enabled: panel.state.enabled
                        onActivated: if (currentText !== "Personalizada") App.audio.applyPreset(currentText)
                    }
                    Button { text: qsTr("Zerar EQ"); onClicked: App.audio.resetEqualizer() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        onItemAdded: panel.instantiatedBandCount++
                        onItemRemoved: panel.instantiatedBandCount--
                        model: [31,62,125,250,500,1000,2000,4000,8000,16000]
                        delegate: ColumnLayout {
                            required property int index
                            required property int modelData
                            Layout.fillWidth: true
                            spacing: 4
                            Label { text: Number(panel.state.eq[index]).toFixed(1); color: Theme.textSecondary; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
                            Slider {
                                objectName: "eqBand" + index
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 130
                                orientation: Qt.Vertical
                                from: -12; to: 12; stepSize: 0.5
                                value: panel.state.eq[index]
                                enabled: panel.state.enabled && panel.state.eqEnabled
                                onMoved: App.audio.setEqBand(index, value)
                                TapHandler { onDoubleTapped: App.audio.setEqBand(index, 0) }
                            }
                            Label { text: modelData < 1000 ? modelData : modelData / 1000 + "k"; color: Theme.textSecondary; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
                        }
                    }
                }
                Label { text: qsTr("−12 a +12 dB · Duplo clique para zerar uma banda"); color: Theme.textMuted; font.pixelSize: 11 }
                RowLayout {
                    Label { text: qsTr("Graves"); color: Theme.textPrimary }
                    Slider { Layout.fillWidth: true; from: 0; to: 12; stepSize: 0.5; value: panel.state.bass; enabled: panel.state.enabled; onMoved: App.audio.setSetting("bass", value) }
                    Label { text: Number(panel.state.bass).toFixed(1) + " dB"; color: Theme.textSecondary; Layout.preferredWidth: 62 }
                    Button { text: qsTr("Zerar"); onClicked: App.audio.setSetting("bass", 0) }
                }
                CheckBox { text: qsTr("Compressão dinâmica (suave)"); checked: panel.state.compressor; enabled: panel.state.enabled; onToggled: App.audio.setSetting("compressor", checked) }
                CheckBox { text: qsTr("Normalizar volume entre faixas"); checked: panel.state.normalization; onToggled: App.audio.setSetting("normalization", checked) }
                Label {
                    text: qsTr("Arquivos locais: ReplayGain ou análise EBU R128 com cache. Spotify: normalização do librespot; a troca reinicia o motor.")
                    wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 11; Layout.fillWidth: true
                }
                RowLayout {
                    Label { text: qsTr("Alvo local"); color: Theme.textSecondary }
                    SpinBox { from: -24; to: -10; value: panel.state.targetLufs; enabled: panel.state.enabled; onValueModified: App.audio.setSetting("targetLufs", value) }
                    Label { text: "LUFS"; color: Theme.textSecondary }
                }
                Label { text: qsTr("Áudio espacial"); color: Theme.textPrimary; font.bold: true }
                ButtonGroup { id: modes }
                RowLayout {
                    RadioButton { text: qsTr("Original"); checked: !App.audio.sourceIamf && !App.audio.hrtfActive; enabled: !App.audio.sourceIamf; ButtonGroup.group: modes; onClicked: App.audio.setSetting("spatial", "original") }
                    RadioButton { objectName: "hrtfMode"; text: qsTr("HRTF para fones"); checked: !App.audio.sourceIamf && App.audio.hrtfActive; enabled: panel.state.enabled && App.audio.hrtfAvailable && !App.audio.sourceIamf; ButtonGroup.group: modes; onClicked: App.audio.setSetting("spatial", "hrtf") }
                    RadioButton { text: qsTr("IAMF binaural"); checked: App.audio.sourceIamf; enabled: App.audio.iamfAvailable && App.audio.sourceIamf; ButtonGroup.group: modes }
                }
                Label {
                    text: App.audio.sourceIamf ? qsTr("Renderização OAR binaural: HRTF adicional desativado.")
                        : !panel.state.enabled ? qsTr("Ative o motor de áudio para usar HRTF.")
                        : !App.audio.hrtfAvailable ? qsTr("HRTF aguarda uma saída OpenAL compatível. Inicie a reprodução neste dispositivo.")
                        : qsTr("HRTF: caixas virtuais para fones, inclusive em músicas estéreo. IAMF exige um arquivo .iamf compatível; não está disponível no streaming do Spotify.")
                    wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 11; Layout.fillWidth: true
                }
                Label { text: qsTr("Os efeitos usam limitador de picos de amostra. Ganhos altos podem reduzir a dinâmica; não há garantia de teto true peak."); wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 11; Layout.fillWidth: true }
                Button { text: qsTr("Restaurar padrão…"); onClicked: confirmReset.open() }
            }
        }
    }
}
