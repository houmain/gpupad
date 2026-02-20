import QtQuick 2.12

Rectangle {
  id: root
  property alias name: text.text
  property alias attributes: attributes
  property var user
  property bool selected
    
  function intersects(rect) {
    return (rect.x > x + width ||
            rect.y > y + height ||
            rect.x + rect.width < x ||
            rect.y + rect.height < y) == false
  }
  
  function autoSize() {
    root.width = 150
    root.height = 30
    if (attributes.count > 0) {
      const last = attributes.itemAt(attributes.count - 1)
      root.height = last.y + last.height + 4
    }
  }
  
  function addAttribute(attribute) {
    const index = attributesModel.count
    attributesModel.insert(index, attribute)
    autoSize()
    return attributes.itemAt(index)
  }
  
  function indexOfAttribute(attribute) {
    for (let i = 0; i < attributes.count; ++i)
      if (attributes.itemAt(i) == attribute)
        return i
    return -1
  }  
  
  color: selected ? "#555" : "#444"
  border.color: selected ? "#FC0" : "#222"
  radius: 5
  
  Component.onCompleted: {
    autoSize();
  }
  
  Rectangle {
    anchors.left: parent.left
    anchors.right: parent.right
    height: 25
    color: "#333"
    border.color: root.border.color
  }

  Text {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.margins: 8
    elide: Text.ElideRight
    id: text
    x: 6
    y: 4
    color: selected ? "#FFF" : "#CCC"
    font.bold: true
    font.pointSize: 10
  }  
  
  ListModel {
    id: attributesModel
  }
    
  Repeater {
    id: attributes
    model: attributesModel
    delegate: Attribute{
      x: 0
      y: 25 + index * 25
      width: parent.width
      height: 24
      text: name
      input.visible: hasInput
      output.visible: hasOutput
    }
  }
}
