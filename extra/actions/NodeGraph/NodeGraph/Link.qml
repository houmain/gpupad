import QtQuick 2.12

Item {
  property Attribute from
  property Attribute to

  property int outX: from.node.x + from.x + from.output.x + from.output.width
  property int outY: from.node.y + from.y + from.output.y + from.output.height / 2
  property int inX:  to.node.x + to.x + to.input.y - to.input.width
  property int inY:  to.node.y + to.y + to.input.y + to.input.height / 2

  x: outX
  y: outY
  width:  inX - outX
  height: inY - outY
  
  Cable {
    anchors.fill: parent
  }
}
