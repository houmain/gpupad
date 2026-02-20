import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts 1.0
import NodeGraph 1.0

ScrollView {
  id: root
  width: 1200
  height: 800
  layer.enabled: true
  layer.samples: 4

  Rectangle {
    anchors.fill: parent
    color: "#555"
  }
          
  NodeGraph {
    id: graph       
    width: Math.max(8000, root.width)
    height: Math.max(4000, root.height)
  }

  focus: true
  Keys.onPressed: {
    if (event.key == Qt.Key_Delete)
      graph.removeSelectedNodes()
  }
  
  MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.RightButton
      
    onClicked: {
      addNode("Node", mouseX, mouseY, 5)
    }
  }       
      
  function addNode(name, x, y, attributes) {
    const node = graph.addNode({
      name: name,
      x: x,
      y: y,
    })
    for (let i = 0; i < attributes; i++)
      node.addAttribute({
        name: "Attribute " + (i + 1),
        hasInput: i < attributes / 2,
        hasOutput: i > attributes / 4,
      })

    return node
  }
  
  Component.onCompleted: {
    let nodes = []
    for (let i = 0; i < 10; ++i)
      nodes.push(addNode("Node " + i,
        100 + (i % 5) * 200,
        50 + Math.floor(i / 5) * 300,
        (i * 13) % 8 + 3))
    
    function rand(max) {
      return Math.floor(Math.random() * max)
    }
    
    for (let i = 0; i < nodes.length - 1; ++i) {
      let node0 = nodes[i]
      let node1 = nodes[i == 4 ? 9 : i + 1]
      
      for (let j = 0; j < 8; j++) {
        let attribute0 = node0.attributes.itemAt(rand(node0.attributes.count))
        let attribute1 = node1.attributes.itemAt(rand(node1.attributes.count))        
        graph.addLink(attribute0, attribute1)
      }
    }
  }
}
