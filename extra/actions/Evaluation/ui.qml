import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.0

ScrollView {
  id: root

  Component.onCompleted: {
    script.initializeUi(root)
  }
  
  RowLayout {
    spacing: 10

    ColumnLayout {
      spacing: 10
      
      Layout.topMargin: 20
      Layout.leftMargin: 20
      
      Label {
        id: evaluation
        text: "Evaluation: " + app.evaluation
      }
      
      RowLayout {
        spacing: 10
        
        Label {
          text: "Frame: "
        }
        
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
        
        Label {
          text: "Time: "
        }        
        
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
        text: "Steady"
        onClicked: app.evaluation = "Steady"
      }      
      
      Button {
        text: "Pause"
        onClicked: app.evaluation = "Paused"
      }
  
      Button {
        text: "Automatic"
        onClicked: app.evaluation = "Automatic"
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
}
