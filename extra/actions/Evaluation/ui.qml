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
          value: app.frame
          from: 0
          to: 3600
          onMoved: { app.frame = value }
        }
        
        Label {
          id: frame
          text: app.frame
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
          to: 60
          onMoved: { app.time = value }
        }

        Label {
          id: time
          text: Math.round(app.time * 1000) / 1000
        }
      }

      Label {
        text: "Time Delta: " + Math.round(app.timeDelta * 10000) / 10000
      }
      
      Text {
          text: "UI Refresh Rate: " + frameAnimation.fps.toFixed(0)
      }

      FrameAnimation {
          id: frameAnimation
          property real fps: smoothFrameTime > 0 ? (1.0 / smoothFrameTime) : 0
          running: true
      }

      Button {
        text: "Steady"
        onClicked: app.evaluation = "Steady"
      }      
      
      Button {
        text: "Automatic"
        onClicked: app.evaluation = "Automatic"
      }
      
      Button {
        text: "Pause"
        onClicked: app.evaluation = "Paused"
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
