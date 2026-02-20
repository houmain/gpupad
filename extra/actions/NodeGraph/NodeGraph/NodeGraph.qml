import QtQuick 2.12

Item {
  id: graph
  property alias nodes: nodes
  property alias links: links
  property alias draggedCable: draggedCable
  property alias draggable: draggable
  signal linkAdded
  signal linkRemoved
  signal nodeAdded
  signal nodeRemoved

  property int topZ: 1
  property var selectedNodes: []

  function canLink(fromAttribute, toAttribute) {
    return (fromAttribute &&
            toAttribute &&
            fromAttribute.output.visible &&
            toAttribute.input.visible &&
            toAttribute.boundInputs.length == 0 &&
            fromAttribute.node != toAttribute.node)
  }

  function addNode(nodeData) {
    const index = nodesModel.count
    nodesModel.insert(index, nodeData)
    const node = nodes.itemAt(index)
    nodeAdded(node)
    node.z = topZ++
    return node
  }

  function unlinkInputs(attribute) {
    for (let from of attribute.boundInputs.slice())
      removeLink(from, attribute)
  }

  function unlinkOutputs(attribute) {
    for (let to of attribute.boundOutputs.slice())
      removeLink(attribute, to)
  }

  function removeNode(node) {
    for (let i = 0; i < nodes.count; ++i)
      if (nodes.itemAt(i) == node) {
        for (let j = 0; j < node.attributes.count; ++j) {
          const attribute = node.attributes.itemAt(j)
          unlinkInputs(attribute)
          unlinkOutputs(attribute)
        }
        nodesModel.remove(i)
        selectedNodes.splice(selectedNodes.indexOf(node), 1)
        nodeRemoved(node)
        break
      }
  }

  function selectNode(node) {
    if (!node.selected) {
      node.selected = true
      selectedNodes.push(node)
    }
    node.z = topZ++
  }

  function deselectNode(node) {
    if (node.selected) {
      node.selected = false
      selectedNodes.splice(selectedNodes.indexOf(node), 1)
    }
  }

  function clearSelection() {
    for (let s of selectedNodes)
      s.selected = false
    selectedNodes.length = 0
  }

  function toggleNodeSelection(node) {
    if (node.selected)
      deselectNode(node)
    else
      selectNode(node)
  }

  function selectNodesInRect(rect) {
    for (let i = 0; i < nodes.count; ++i) {
      let node = nodes.itemAt(i)
      if (node.intersects(rect))
        selectNode(node)
    }
  }

  function removeSelectedNodes() {
    for (let node of selectedNodes.slice())
      removeNode(node)
  }

  function addLink(fromAttribute, toAttribute) {
    if (!canLink(fromAttribute, toAttribute))
      return
    const index = linksModel.count
    linksModel.insert(index, {
      fromAttribute: fromAttribute,
      toAttribute: toAttribute
    })
    toAttribute.boundInputs.push(fromAttribute)
    fromAttribute.boundOutputs.push(toAttribute)
    const link = links.itemAt(index)
    linkAdded(link)
    return link
  }

  function removeLink(fromAttribute, toAttribute) {
    for (let i = 0; i < linksModel.count; ++i) {
      const link = linksModel.get(i)
      if (link.fromAttribute == fromAttribute &&
          link.toAttribute == toAttribute) {
        toAttribute.boundInputs.splice(
          toAttribute.boundInputs.indexOf(fromAttribute), 1)
        fromAttribute.boundOutputs.splice(
          fromAttribute.boundOutputs.indexOf(toAttribute), 1)
        linkRemoved(link)
        linksModel.remove(i)
        break
      }
    }
  }

  function getNodeAtPosition(point) {
    let rect = Qt.rect(point.x, point.y, 0, 0);
    for (let i = 0; i < nodes.count; ++i) {
      let node = nodes.itemAt(i)
      if (node.intersects(rect))
        return node
    }
  }

  Repeater {
    id: nodes
    model: ListModel {
      id: nodesModel
    }
    delegate: Node {
      id: node
      x: model.x
      y: model.y
      z: 1
      user: model.user || { }
      width: model.width
      height: model.height
      name: model.name
    }
  }

  Repeater {
    id: links
    model: ListModel {
      id: linksModel
    }
    delegate: Link{
      from: fromAttribute
      to: toAttribute
    }
  }

  Item {
    id: draggable
    property Socket target: null

    width: 1
    height: 1
    Drag.active: true

    Rectangle {
      visible: false
      color: 'transparent'
      border.color: 'red'
      anchors.fill: parent
      anchors.margins: -5
      radius: 5
    }
  }

  MouseArea {
    anchors.fill: parent
    drag.target: graph.draggable
    drag.smoothed: false
    drag.threshold: 5

    onPressed: {
      const node = getNodeAtPosition(Qt.point(mouseX, mouseY))
      const control = (mouse.modifiers & Qt.ControlModifier)
      if (node && control) {
        toggleNodeSelection(node)
      }
      else if (node) {
        if (!node.selected)
          clearSelection()
        selectNode(node)
        dragSelectedNodes.begin()
      }
      else {
        if (!control)
          clearSelection()
        selectionRect.begin(Qt.point(mouseX, mouseY))
      }
    }

    onPositionChanged: {
      dragSelectedNodes.update()
      selectionRect.update()
    }

    onReleased: {
      dragSelectedNodes.end(Qt.point(mouseX, mouseY))
      selectionRect.end()
    }
  }

  Item {
    id: dragSelectedNodes
    property bool active
    property point prev

    function begin() {
      active = true
      draggable.parent = graph
      prev = Qt.point(draggable.x, draggable.y)
    }

    function update() {
      if (active) {
        const dx = draggable.x - prev.x
        const dy = draggable.y - prev.y
        for (let s of selectedNodes) {
          s.x += dx
          s.y += dy
        }
        prev = Qt.point(draggable.x, draggable.y)
      }
    }

    function end(point) {
      active = false
    }
  }

  Cable {
    id: draggedCable
    visible: false
    z: 1000000

    function begin(socket, pos) {
      clearSelection()

      if (socket.isInput && socket.attribute.boundInputs.length == 1) {
        // unplug from output socket
        const to = socket.attribute
        const from = to.boundInputs[0]
        removeLink(from, to)
        socket = from.output
        pos = mapToItem(socket, mapFromItem(to.input, pos))
      }

      visible = true
      draggable.Drag.keys = [100]
      draggable.parent = socket
      draggable.x = pos.x
      draggable.y = pos.y
      update()
    }

    function update() {
      let socket = draggable.parent
      if (!socket)
        return

      let p1 = parent.mapFromItem(draggable,
        -draggable.x, -draggable.y + socket.height / 2)
      let p2 = parent.mapFromItem(draggable, 0, 0)

      // snap to target
      if (draggable.target)
        p2 = parent.mapFromItem(draggable.target,
          0, socket.height / 2)

      // swap when connecting input
      if (socket.isInput) {
        let tmp = p1; p1 = p2; p2 = tmp
      }

      p1.x += socket.width

      x = p1.x
      y = p1.y
      width = p2.x - p1.x
      height = p2.y - p1.y
    }

    function end() {
      if (draggable.target) {
        let from = draggable.parent.attribute
        let to = draggable.target.attribute
        if (draggable.parent.isInput) {
          let tmp = to; to = from; from = tmp
        }
        addLink(from, to)
      }
      draggable.parent = null
      visible = false
    }
  }

  SelectionRect {
    id: selectionRect
    visible: false
    z: 1000000
    property point start

    function begin(pos) {
      visible = true
      start = pos
      draggable.Drag.keys = [0]
      draggable.parent = graph
      draggable.x = pos.x
      draggable.y = pos.y
      update()
    }

    function update() {
      const rect = getRect()
      x = rect.x
      y = rect.y
      width = rect.width
      height = rect.height
    }

    function end() {
      if (visible) {
        draggable.parent = null
        visible = false
        selectNodesInRect(getRect())
      }
    }

    function getRect() {
      const x1 = Math.min(draggable.x, start.x)
      const x2 = Math.max(draggable.x, start.x)
      const y1 = Math.min(draggable.y, start.y)
      const y2 = Math.max(draggable.y, start.y)
      return Qt.rect(x1, y1, x2 - x1, y2 - y1)
    }
  }
}
