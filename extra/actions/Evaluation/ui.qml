import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.0

ScrollView {
  id: root

  Component.onCompleted: {
    script.initializeUi(root)
  }

  ColumnLayout {
    spacing: 10
    
    Label {
      id: evaluation
      text: app.evaluation
    }
    
    RowLayout {
      spacing: 10
      
      Slider {
        value: app.frameIndex
        from: 0
        to: 1000
        onMoved: { app.frameIndex = value }
      }
      
      Label {
        id: frameIndex
        text: app.frameIndex
      }      
    }
        
    RowLayout {
      spacing: 10
      
      Slider {
        value: app.time
        from: 0
        to: 1000/60
        onMoved: { app.time = value }
      }
      
      Label {
        id: time
        text: app.time
      }
    }
    
    Button {
      text: "Paused"
      onClicked: app.evaluation = "Paused"
    }

    Button {
      text: "Automatic"
      onClicked: app.evaluation = "Automatic"
    }

    Button {
      text: "Steady"
      onClicked: app.evaluation = "Steady"
    }

    Button {
      text: "Manual"
      onClicked: app.evaluation = "Manual"
    }
    
    Button {
      text: "Reset"
      onClicked: app.evaluation = "Reset"
    }
  }
}
