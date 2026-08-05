pragma ComponentBehavior: Bound

import App

Item {
  id: bindingControl

  property string name
  property int bindingId
  property int valueCount: 1
  property var values: [0]
  property real minimum: 0
  property real maximum: 1

  Layout.fillWidth: true
  implicitWidth: content.implicitWidth
  implicitHeight: content.implicitHeight

  function setValue(index, value) {
    const nextValues = values.slice()
    nextValues[index] = value
    values = nextValues

    const binding = app.findItem(bindingId)
    if (binding)
      binding.values = nextValues
  }

  ColumnLayout {
    id: content
    width: parent.width
    spacing: 6

    RowLayout {
      Layout.fillWidth: true

      Label {
        text: bindingControl.name
        Layout.fillWidth: true
      }

      Label {
        text: qsTr("Min")
      }

      DoubleSpinBox {
        Layout.preferredWidth: 90
        Layout.preferredHeight: 28
        decimals: 3
        stepSize: 0.1
        from: -1000000
        to: bindingControl.maximum
        value: bindingControl.minimum
        updateValueOnModified: false
        editable: true
        wheelEnabled: true
        onValueModified: function(modifiedValue) {
          bindingControl.minimum = modifiedValue
        }
      }

      Label {
        text: qsTr("Max")
      }

      DoubleSpinBox {
        Layout.preferredWidth: 90
        Layout.preferredHeight: 28
        decimals: 3
        stepSize: 0.1
        from: bindingControl.minimum
        to: 1000000
        value: bindingControl.maximum
        updateValueOnModified: false
        editable: true
        wheelEnabled: true
        onValueModified: function(modifiedValue) {
          bindingControl.maximum = modifiedValue
        }
      }
    }

    Repeater {
      model: bindingControl.valueCount

      RowLayout {
        id: sliderRow

        required property int index

        Layout.fillWidth: true

        Slider {
          Layout.fillWidth: true
          from: bindingControl.minimum
          to: bindingControl.maximum
          value: bindingControl.values[sliderRow.index] ?? 0
          onMoved: bindingControl.setValue(sliderRow.index, value)
        }

        DoubleSpinBox {
          Layout.preferredWidth: 90
          Layout.preferredHeight: 28
          decimals: 3
          stepSize: 0.01
          from: bindingControl.minimum
          to: bindingControl.maximum
          value: bindingControl.values[sliderRow.index] ?? 0
          updateValueOnModified: false
          editable: true
          wheelEnabled: true
          onValueModified: function(modifiedValue) {
            bindingControl.setValue(sliderRow.index, modifiedValue)
          }
        }
      }
    }
  }
}
