import QtQuick
import QtQuick.Controls

Item {
  property int decimals: 2
  property real value: 0.0
  property real from: 0.0
  property real to: 100.0
  property real stepSize: 1.0
  property alias editable: spinbox.editable
  signal valueModified

  SpinBox{
    id: spinbox
    property real factor: Math.pow(10, decimals)    
    width: parent.width
    height: parent.height
    stepSize: parent.stepSize * factor
    value: parent.value * factor
    to: parent.to * factor
    from: parent.from * factor
    
    validator: RegularExpressionValidator {
      regularExpression: /[0-9]+[.]?[0-9]+/
    }
    
    valueFromText: function(text, locale) {
      return Number.fromLocaleString(locale, text) * factor + 0.5
    }
    
    textFromValue: function(value, locale) {
      return parseFloat(value / factor).toFixed(decimals)
    }
    
    onValueModified: {
      parent.value = value / factor
      parent.valueModified()
    }
  }
}
