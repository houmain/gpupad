import QtQuick
import QtQuick.Controls

Item {
  id: root

  property int decimals: 2
  property real value: 0.0
  property real from: 0.0
  property real to: 100.0
  property real stepSize: 1.0
  property bool updateValueOnModified: true
  property alias editable: spinbox.editable
  property alias wheelEnabled: spinbox.wheelEnabled

  signal valueModified(real modifiedValue)

  implicitWidth: spinbox.implicitWidth
  implicitHeight: spinbox.implicitHeight

  SpinBox {
    id: spinbox

    property real factor: Math.pow(10, root.decimals)

    anchors.fill: parent
    stepSize: Math.max(1, Math.round(root.stepSize * factor))
    value: Math.round(root.value * factor)
    from: Math.round(root.from * factor)
    to: Math.round(root.to * factor)

    validator: DoubleValidator {
      bottom: root.from
      top: root.to
      decimals: root.decimals
      notation: DoubleValidator.StandardNotation
      locale: spinbox.locale.name
    }

    valueFromText: function(text, locale) {
      return Math.round(Number.fromLocaleString(locale, text) * factor)
    }

    textFromValue: function(value, locale) {
      return Number(value / factor).toLocaleString(
          locale, "f", root.decimals)
    }

    onValueModified: {
      const modifiedValue = value / factor
      if (root.updateValueOnModified)
        root.value = modifiedValue
      root.valueModified(modifiedValue)
    }
  }
}
