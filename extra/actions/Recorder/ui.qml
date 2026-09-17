pragma ComponentBehavior: Bound

import App

ThemedScrollView {
  id: root

  property var textureModel: []
  property bool hasAudio: false
  property bool running: false
  property int progress: 0
  property int frameCount: 0
  property string status: ""
  property bool initializing: true
  readonly property bool hasVideo: Number(comboValue(texture)) != 0

  implicitWidth: 620
  implicitHeight: Math.min(620, form.implicitHeight + 32)
  contentWidth: availableWidth
  contentHeight: form.implicitHeight + 32

  function valueIndex(combo, value) {
    for (let index = 0; index < combo.count; ++index) {
      if (String(combo.valueAt(index)) == String(value))
        return index
    }
    return -1
  }

  function defaultIndex(model) {
    for (let index = 0; index < model.length; ++index) {
      if (model[index].default)
        return index
    }
    return model.length > 0 ? 0 : -1
  }

  function comboValue(combo) {
    return combo.currentIndex >= 0 ? combo.currentValue : ""
  }

  function refreshAudioCodecs() {
    const previous = comboValue(audioCodec)
    const model = script.audioCodecEntries(comboValue(fileFormat),
      comboValue(videoCodec), hasVideo)
    audioCodec.model = model
    const previousIndex = valueIndex(audioCodec, previous)
    audioCodec.currentIndex = previousIndex >= 0
      ? previousIndex : defaultIndex(model)
  }

  function refreshCodecs() {
    const previous = comboValue(videoCodec)
    const model = hasVideo
      ? script.videoCodecEntries(comboValue(fileFormat)) : []
    videoCodec.model = model
    const previousIndex = valueIndex(videoCodec, previous)
    videoCodec.currentIndex = previousIndex >= 0
      ? previousIndex : defaultIndex(model)
    refreshAudioCodecs()
  }

  function refreshFormats(replaceSuffix) {
    const previous = comboValue(fileFormat)
    const model = script.formatEntries(hasVideo)
    fileFormat.model = model
    const previousIndex = valueIndex(fileFormat, previous)
    fileFormat.currentIndex = previousIndex >= 0
      ? previousIndex : defaultIndex(model)
    refreshCodecs()
    if (replaceSuffix && !initializing) {
      fileName.text = script.replaceFileSuffix(fileName.text,
        comboValue(fileFormat))
    }
  }

  function canRecord() {
    const hasEncodedAudio = hasAudio && audioCodec.currentIndex >= 0
    return fileName.text.trim().length > 0
      && endTime.value > startTime.value
      && frameRate.value > 0
      && fileFormat.currentIndex >= 0
      && (!hasVideo || videoCodec.currentIndex >= 0)
      && (!hasAudio || hasEncodedAudio)
      && (hasVideo || hasEncodedAudio)
  }

  Component.onCompleted: {
    script.initializeUi(root)
    texture.currentIndex = script.defaultTextureIndex()
    refreshFormats(false)
    fileName.text = script.defaultFileName(hasVideo)
    initializing = false
  }

  ColumnLayout {
    id: form
    x: 16
    y: 16
    width: Math.max(0, root.availableWidth - 32)
    spacing: 12

    GridLayout {
      Layout.fillWidth: true
      columns: 2
      columnSpacing: 12
      rowSpacing: 10
      enabled: !root.running

      Label { text: qsTr("Texture") }
      ComboBox {
        id: texture
        Layout.fillWidth: true
        model: root.textureModel
        textRole: "text"
        valueRole: "key"
        onActivated: root.refreshFormats(true)
      }

      Label { text: qsTr("Resolution") }
      Label {
        Layout.fillWidth: true
        text: script.textureResolution(root.comboValue(texture))
      }

      Label { text: qsTr("File") }
      RowLayout {
        Layout.fillWidth: true
        TextField {
          id: fileName
          Layout.fillWidth: true
        }
        Button {
          text: qsTr("Browse...")
          onClicked: {
            const selected = app.saveFileDialog(fileName.text)
            if (selected) {
              fileName.text = selected
            }
          }
        }
      }

      Label { text: qsTr("Container") }
      ComboBox {
        id: fileFormat
        Layout.fillWidth: true
        textRole: "text"
        valueRole: "key"
        onActivated: {
          root.refreshCodecs()
          if (!root.initializing) {
            fileName.text = script.replaceFileSuffix(fileName.text,
              root.comboValue(fileFormat))
          }
        }
      }

      Label {
        text: qsTr("Video codec")
        enabled: root.hasVideo
      }
      ComboBox {
        id: videoCodec
        Layout.fillWidth: true
        enabled: root.hasVideo
        textRole: "text"
        valueRole: "key"
        onActivated: root.refreshAudioCodecs()
      }

      Label {
        text: qsTr("Video bitrate")
        enabled: root.hasVideo
      }
      RowLayout {
        enabled: root.hasVideo
        SpinBox {
          id: videoBitRate
          from: 1
          to: 1000000
          value: 12000
          editable: true
        }
        Label { text: qsTr("kbit/s") }
      }

      Label {
        text: qsTr("Audio codec")
        enabled: root.hasAudio
      }
      ComboBox {
        id: audioCodec
        Layout.fillWidth: true
        enabled: root.hasAudio
        textRole: "text"
        valueRole: "key"
      }

      Label {
        text: qsTr("Audio bitrate")
        enabled: root.hasAudio
      }
      RowLayout {
        enabled: root.hasAudio
        SpinBox {
          id: audioBitRate
          from: 1
          to: 10000
          value: 192
          editable: true
        }
        Label { text: qsTr("kbit/s") }
      }

      Label { text: qsTr("Time range") }
      RowLayout {
        DoubleSpinBox {
          id: startTime
          Layout.preferredWidth: 120
          from: 0
          to: 1000000
          value: 0
          decimals: 3
          editable: true
        }
        Label { text: qsTr("to") }
        DoubleSpinBox {
          id: endTime
          Layout.preferredWidth: 120
          from: 0.001
          to: 1000000
          value: 10
          decimals: 3
          editable: true
        }
        Label { text: qsTr("s") }
      }

      Label { text: qsTr("Frame rate") }
      RowLayout {
        DoubleSpinBox {
          id: frameRate
          Layout.preferredWidth: 120
          from: 1
          to: 240
          value: 60
          decimals: 3
          editable: true
        }
        Label { text: qsTr("fps") }
      }
    }

    ProgressBar {
      Layout.fillWidth: true
      visible: root.running || root.progress > 0
      from: 0
      to: Math.max(1, root.frameCount)
      value: root.progress
    }

    Label {
      Layout.fillWidth: true
      visible: text.length > 0
      text: root.status
      wrapMode: Text.WordWrap
    }

    Button {
      Layout.alignment: Qt.AlignRight
      text: root.running ? qsTr("Cancel") : qsTr("Record")
      enabled: root.running || root.canRecord()
      onClicked: {
        if (root.running) {
          script.cancel()
        } else {
          script.record({
            textureId: root.comboValue(texture),
            outputFile: fileName.text,
            fileFormat: root.comboValue(fileFormat),
            videoCodec: root.comboValue(videoCodec),
            audioCodec: root.comboValue(audioCodec),
            startTime: startTime.value,
            endTime: endTime.value,
            frameRate: frameRate.value,
            videoBitRate: videoBitRate.value,
            audioBitRate: audioBitRate.value,
          })
        }
      }
    }
  }
}
