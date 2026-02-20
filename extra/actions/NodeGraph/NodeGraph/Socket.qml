import QtQuick 2.12

Rectangle{
  id: socket
  property Item attribute: parent
  property Item node: parent.parent
  property Item graph: parent.parent.parent
  property bool isInput: false

  color: "#FF3"
  border.color: "#222"
  radius: width

  DropArea {
    id: dropArea
    anchors.fill: parent
    anchors.margins: -4
    keys: [ 100 ]
    onEntered: {
      let fromSocket = drag.source.parent
      if (isInput == fromSocket.isInput)
        return
      if (!graph.canLink(isInput ? fromSocket.attribute : attribute,
                         isInput ? attribute : fromSocket.attribute))
        return

      drag.source.target = socket
      graph.draggedCable.update()
    }
    onExited: {
      if (drag.source.target == socket) {
        drag.source.target = null;
        graph.draggedCable.update()
      }
    }
  }

  MouseArea {
    id: mouseArea
    anchors.fill: parent
    drag.target: graph.draggable
    drag.smoothed: false
    drag.threshold: 0

    onPressed: {
      graph.draggedCable.begin(socket, Qt.point(mouseX, mouseY))
    }
    onReleased: {
      graph.draggedCable.end()
    }
    onPositionChanged: {
      graph.draggedCable.update()
    }
  }
}
