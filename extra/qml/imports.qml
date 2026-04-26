import QtQuick
import QtQuick.Shapes
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.WorkerScript

ScrollView { // QtQuick.Controls

  RowLayout { // QtQuick.Layouts
    Text { id: myText } // QtQuick

    Shape { } // QtQuick.Shapes
  }

  WorkerScript { // QtQml.WorkerScript
    source: "script.mjs"
    onMessage: (message)=> myText.text = message
  }
}

