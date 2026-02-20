import QtQuick 2.12
import QtQuick.Shapes 1.15

Shape {
  id: shape
  
  property point c1: Qt.point(
    Math.max(shape.width / 8 * 7,
      Math.min(Math.abs(shape.height), 100)),
    shape.height / 8);
    
  property point c2: Qt.point(
    Math.min(shape.width / 8,
      shape.width - Math.min(Math.abs(shape.height), 100)),
    shape.height / 8 * 7);

  ShapePath {
    capStyle: ShapePath.RoundCap
    strokeWidth: 6
    strokeColor: "#222"
    fillColor: "transparent"
    PathCubic {
      x: shape.width
      y: shape.height
      control1X: c1.x
      control1Y: c1.y
      control2X: c2.x
      control2Y: c2.y
    }
  }
  
  ShapePath {
    capStyle: ShapePath.RoundCap
    strokeWidth: 4
    strokeColor: "#999"
    fillColor: "transparent"
    PathCubic {
      x: shape.width
      y: shape.height
      control1X: c1.x
      control1Y: c1.y
      control2X: c2.x
      control2Y: c2.y
    }
  }
}