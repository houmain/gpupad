pragma ComponentBehavior: Bound
// qmllint disable unqualified

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
  id: root

  implicitWidth: 400
  implicitHeight: 400
  focus: true

  readonly property var keyCodes: [
    Qt.Key_A, Qt.Key_W, Qt.Key_S, Qt.Key_E, Qt.Key_D, Qt.Key_F, Qt.Key_T,
    Qt.Key_G, Qt.Key_Y, Qt.Key_H, Qt.Key_U, Qt.Key_J, Qt.Key_K
  ]
  readonly property var keyLabels: [
    "A", "W", "S", "E", "D", "F", "T", "G", "Y", "H", "U", "J", "K"
  ]
  readonly property var noteLabels: [
    "C4", "C♯4", "D4", "D♯4", "E4", "F4", "F♯4",
    "G4", "G♯4", "A4", "A♯4", "B4", "C5"
  ]
  readonly property var whiteNotes: [0, 2, 4, 5, 7, 9, 11, 12]
  readonly property var blackNotes: [1, 3, 6, 8, 10]
  readonly property var blackCenters: [1, 2, 4, 5, 6]
  property var noteStateValues: []
  property double previousTime: 0

  function initialNoteStateValues() {
    const values = []
    for (let note = 0; note < keyCodes.length; ++note)
      values.push(-1000, -1000, 0, 60 + note)
    return values
  }

  function isPressed(note) {
    return noteStateValues[note * 4 + 2] > 0.5
  }

  function publishNoteStateValues() {
    const binding = app.findItem("noteState")
    if (binding)
      binding.values = noteStateValues.map(value => value.toString())
  }

  function resetNoteStateValues() {
    noteStateValues = initialNoteStateValues()
    publishNoteStateValues()
  }

  function setNotePressed(note, pressed) {
    if (note < 0 || note >= keyCodes.length
        || isPressed(note) === pressed)
      return

    const next = noteStateValues.slice()
    const offset = note * 4
    const eventTime = app.soundTime
    if (pressed) {
      next[offset] = eventTime
      next[offset + 1] = -1
    } else {
      next[offset + 1] = eventTime
    }
    next[offset + 2] = pressed ? 1 : 0
    noteStateValues = next
    publishNoteStateValues()
  }

  function releaseAllNotes() {
    const next = noteStateValues.slice()
    const eventTime = app.soundTime
    let changed = false
    for (let note = 0; note < keyCodes.length; ++note) {
      const offset = note * 4
      if (next[offset + 2] > 0.5) {
        next[offset + 1] = eventTime
        next[offset + 2] = 0
        changed = true
      }
    }
    if (changed) {
      noteStateValues = next
      publishNoteStateValues()
    }
  }

  Keys.priority: Keys.BeforeItem
  Keys.onPressed: function(event) {
    const note = keyCodes.indexOf(event.key)
    if (note >= 0) {
      if (!event.isAutoRepeat)
        setNotePressed(note, true)
      event.accepted = true
    }
  }
  Keys.onReleased: function(event) {
    const note = keyCodes.indexOf(event.key)
    if (note >= 0) {
      if (!event.isAutoRepeat)
        setNotePressed(note, false)
      event.accepted = true
    }
  }

  onActiveFocusChanged: {
    if (!activeFocus)
      releaseAllNotes()
  }

  Connections {
    target: app

    function onTimeChanged() {
      const timelineReset = app.frame === 0 || app.time < root.previousTime
      root.previousTime = app.time
      if (timelineReset)
        root.resetNoteStateValues()
    }
  }

  Component.onCompleted: {
    previousTime = app.time
    resetNoteStateValues()
    forceActiveFocus()
    
    app.evaluation = "Steady"
  }
  Component.onDestruction: releaseAllNotes()

  Rectangle {
    anchors.fill: parent
    color: "#111629"

    gradient: Gradient {
      GradientStop { position: 0; color: "#171d35" }
      GradientStop { position: 1; color: "#29182f" }
    }
  }

  ColumnLayout {
    anchors.fill: parent
    anchors.margins: 24
    spacing: 14

    RowLayout {
      Layout.fillWidth: true
      spacing: 16

      ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
          text: qsTr("Shader Keyboard")
          color: "#f3f5ff"
          font.pixelSize: 25
          font.weight: Font.DemiBold
        }
      }
    }

    Item {
      id: piano

      Layout.fillWidth: true
      Layout.fillHeight: true
      Layout.minimumHeight: 240

      TapHandler {
        onTapped: root.forceActiveFocus()
      }

      Repeater {
        model: root.whiteNotes.length

        delegate: Rectangle {
          id: whiteKey

          required property int index
          readonly property int note: root.whiteNotes[index]

          x: index * piano.width / root.whiteNotes.length
          y: 4
          width: piano.width / root.whiteNotes.length
          height: piano.height - 8
          radius: 6
          color: root.isPressed(note)
            ? "#35cfc3" : (whiteMouse.containsMouse ? "#ffffff" : "#e4e8f2")
          border.width: root.isPressed(note) ? 3 : 1
          border.color: root.isPressed(note) ? "#9cfff7" : "#555d73"

          Behavior on color { ColorAnimation { duration: 70 } }

          Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            text: root.keyLabels[whiteKey.note]
            color: "#23283a"
            font.pixelSize: 22
            font.weight: Font.Bold
          }

          Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 52
            text: root.noteLabels[whiteKey.note]
            color: "#687087"
            font.pixelSize: 12
          }

          MouseArea {
            id: whiteMouse
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onPressed: {
              root.forceActiveFocus()
              root.setNotePressed(whiteKey.note, true)
            }
            onReleased: root.setNotePressed(whiteKey.note, false)
            onCanceled: root.setNotePressed(whiteKey.note, false)
          }
        }
      }

      Repeater {
        model: root.blackNotes.length

        delegate: Rectangle {
          id: blackKey

          required property int index
          readonly property int note: root.blackNotes[index]

          z: 2
          x: root.blackCenters[index] * piano.width / root.whiteNotes.length
            - width / 2
          y: 4
          width: piano.width / 13
          height: piano.height * 0.62
          radius: 6
          color: root.isPressed(note)
            ? "#ff7146" : (blackMouse.containsMouse ? "#303852" : "#111729")
          border.width: root.isPressed(note) ? 3 : 1
          border.color: root.isPressed(note) ? "#ffc1a7" : "#4a526a"

          Behavior on color { ColorAnimation { duration: 70 } }

          Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            text: root.keyLabels[blackKey.note]
            color: "#f4f6ff"
            font.pixelSize: 18
            font.weight: Font.Bold
          }

          Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 42
            text: root.noteLabels[blackKey.note]
            color: "#aeb6cc"
            font.pixelSize: 11
          }

          MouseArea {
            id: blackMouse
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            onPressed: {
              root.forceActiveFocus()
              root.setNotePressed(blackKey.note, true)
            }
            onReleased: root.setNotePressed(blackKey.note, false)
            onCanceled: root.setNotePressed(blackKey.note, false)
          }
        }
      }
    }
  }
}
