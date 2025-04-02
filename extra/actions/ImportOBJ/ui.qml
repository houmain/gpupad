import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.0

ScrollView {
  id: root
  property alias fileName: fileName.text
  property alias width_: width.value
  property alias height_: height.value
  property alias depth: depth.value
  property alias scaleU: scaleU.value
  property alias scaleV: scaleV.value
  property alias normalize: normalize.checked
  property alias center: center.checked
  property alias swapYZ: swapYZ.checked
  property alias drawCalls: drawCalls.checked
  property alias indexed: indexed.checked

  Component.onCompleted: script.initializeUi(root)

  GridLayout {
    x: 16
    y: 16
    rows: 1
    columns: 2

    Label {
      text: qsTr("File")
    }
    RowLayout {
      TextField {
        id: fileName
        Layout.preferredHeight: 25
        Layout.preferredWidth: 150
        onTextChanged: {
          insert.enabled = (this.text.length > 0)
          script.refresh()
        }
      }
      Button {
        id: browseFileName
        text: qsTr("Browse")
        onClicked: {
          const result = app.openFileDialog("*.obj")
          if (result)
            fileName.text = result
        }
      }
    }

    Label {
      text: qsTr("Width")
    }
    DoubleSpinBox {
      id: width
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 1
      from: 0
      to: 1000
      decimals: 2
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Height")
    }
    DoubleSpinBox {
      id: height
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 1
      from: 0
      to: 1000
      decimals: 2
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Depth")
    }
    DoubleSpinBox {
      id: depth
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 1
      from: 0
      to: 1000
      decimals: 2
      editable: true
      onValueModified: { script.refresh() }
    }

    Label {
      text: qsTr("Scale U")
    }
    DoubleSpinBox {
      id: scaleU
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 1
      from: -1000
      to: 1000
      decimals: 2
      stepSize: 1.0
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Scale V")
    }
    DoubleSpinBox {
      id: scaleV
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 1
      from: -1000
      to: 1000
      decimals: 2
      stepSize: 1.0
      editable: true
      onValueModified: { script.refresh() }
    }

    Label {
      text: qsTr("Normalize")
    }
    CheckBox {
      id: normalize
      Layout.preferredHeight: 25
      Layout.preferredWidth: 25
      onToggled: { script.refresh() }
    }
    
    Label {
      text: qsTr("Center")
    }
    CheckBox {
      id: center
      Layout.preferredHeight: 25
      Layout.preferredWidth: 25
      onToggled: { script.refresh() }
    }
    
    Label {
      text: qsTr("Swap Y/Z")
    }
    CheckBox {
      id: swapYZ
      Layout.preferredHeight: 25
      Layout.preferredWidth: 25
      onToggled: { script.refresh() }
    }

    Label {
      text: qsTr("Indexed")
    }
    CheckBox {
      id: indexed
      Layout.preferredHeight: 25
      Layout.preferredWidth: 25
      checked: true
      onToggled: { script.refresh() }
    }
    
    Label {
      text: qsTr("Draw Calls")
    }
    CheckBox {
      id: drawCalls
      Layout.preferredHeight: 25
      Layout.preferredWidth: 25
      checked: true
      onToggled: { script.refresh() }
    }

    Item {}
    Button {
      id: insert
      text: qsTr("Insert")
      Layout.preferredHeight: 30
      Layout.preferredWidth: 125
      enabled: false
      onClicked: { script.insert() }
    }
  }
}
