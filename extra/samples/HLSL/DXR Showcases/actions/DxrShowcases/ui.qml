import App

ThemedScrollView {
  id: root
  implicitWidth: 320
  implicitHeight: 200
  contentWidth: Math.max(300, availableWidth)
  contentHeight: controls.implicitHeight + 24

  ColumnLayout {
    id: controls
    x: 12
    y: 12
    width: root.contentWidth - 24
    spacing: 8

    RowLayout {
      Layout.fillWidth: true
      Label { text: "Showcase" }
      ComboBox {
        Layout.fillWidth: true
        model: ["Infinity Lattice", "Living Geometry", "Adaptive Reflection Field"]
        currentIndex: script.showcase()
        onActivated: script.selectShowcase(currentIndex)
      }
    }

    Label {
      text: app.evaluation === "Steady" && app.timeDelta > 0
        ? "Frame FPS: " + (app.timeDelta * 1000).toFixed(2) + " ms | "
          + (1 / app.timeDelta).toFixed(1) + " FPS"
        : "Frame FPS: paused"
    }
    Item {
      id: frameGraph
      Layout.fillWidth: true
      Layout.minimumWidth: 0
      Layout.preferredHeight: 120

      property var samples: []
      property int sampleCapacity: 120
      property real maximumMilliseconds: 60

      function appendSample() {
        const milliseconds = app.timeDelta * 1000
        if (!(milliseconds > 0) || !isFinite(milliseconds))
          return

        const history = samples.slice(
          Math.max(0, samples.length - sampleCapacity + 1))
        history.push(milliseconds)
        samples = history
        framePlot.requestPaint()
      }

      Timer {
        interval: 100
        repeat: true
        running: app.evaluation === "Steady"
        onTriggered: frameGraph.appendSample()
      }

      Canvas {
        id: framePlot
        anchors.fill: parent

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
          const context = getContext("2d")
          const width = framePlot.width
          const height = framePlot.height
          const maximum = frameGraph.maximumMilliseconds

          context.clearRect(0, 0, width, height)
          context.fillStyle = app.palette.base
          context.fillRect(0, 0, width, height)

          const guideTimes = [10, 20, 30, 40, 50, 60]
          context.lineWidth = 1
          context.strokeStyle = app.palette.button
          context.fillStyle = app.palette.text
          context.font = "10px sans-serif"

          for (let guideIndex = 0; guideIndex < guideTimes.length; ++guideIndex) {
            const milliseconds = guideTimes[guideIndex]
            const y = height - milliseconds / maximum * height
            context.beginPath()
            context.moveTo(0, y)
            context.lineTo(width, y)
            context.stroke()
            const labelY = y < 12 ? y + 11 : y - 3
            context.fillText(milliseconds.toFixed(0) + " ms", 4, labelY)
          }

          if (frameGraph.samples.length < 2)
            return

          const firstSlot = frameGraph.sampleCapacity - frameGraph.samples.length
          context.beginPath()
          for (let sampleIndex = 0;
             sampleIndex < frameGraph.samples.length;
             ++sampleIndex) {
            const x = (firstSlot + sampleIndex)
              / (frameGraph.sampleCapacity - 1) * width
            const milliseconds = Math.min(frameGraph.samples[sampleIndex], maximum)
            const y = height - milliseconds / maximum * height
            if (sampleIndex === 0)
              context.moveTo(x, y)
            else
              context.lineTo(x, y)
          }
          context.lineWidth = 2
          context.strokeStyle = "#4ea1ff"
          context.stroke()
        }
      }
    }
  }
}
