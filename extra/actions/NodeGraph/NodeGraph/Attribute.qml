import QtQuick 2.12

Item {
  property Item node: parent
  property alias text: text.text
  property alias input: inputSocket
  property alias output: outputSocket
  property var boundInputs: []
  property var boundOutputs: []
  property int socketSize: 12

  Socket {
    id: inputSocket
    isInput: true
    width: socketSize
    height: socketSize
    x: -width / 2 + 1
    y: (parent.height - height) / 2
    z: 1
  }

  Socket {
    id: outputSocket
    isInput: false
    width: socketSize
    height: socketSize
    x: parent.width - width / 2 - 1
    y: (parent.height - height) / 2
    z: 1
  }

  Text {
    id: text
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.margins: 10
    horizontalAlignment:
      (inputSocket.visible ? (outputSocket.visible ? Text.AlignHCenter : Text.AlignLeft) : Text.AlignRight)
    elide: Text.ElideRight
    x: 12
    y: 2
    color: "#CCC"
    font.pointSize: 10
  }
}
