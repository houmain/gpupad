import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.0

ScrollView {
  id: root
  property alias typeIndex: type.currentIndex
  property alias type: type.currentText
  property alias typeNames: type.model
  property alias slices: slices.value
  property alias stacks: stacks.value
  property alias facetted: facetted.checked
  property alias insideOut: insideOut.checked
  property alias swapYZ: swapYZ.checked
  property alias width_: width.value
  property alias height_: height.value
  property alias depth: depth.value
  property alias radius: radius.value
  property alias subdivisions: subdivisions.value
  property alias seed: seed.value
  property alias scaleU: scaleU.value
  property alias scaleV: scaleV.value
  property alias indexed: indexed.checked

  property bool hasStacks
  property bool hasSlices
  property bool hasRadius
  property bool hasSubdivisions
  property bool hasSeed
  
  Component.onCompleted: script.initializeUi(root)

  GridLayout {
    x: 16
    y: 16
    rows: 1
    columns: 2

    Label {
      text: qsTr("Type")
    }
    ComboBox {
      id: type
      currentIndex: 0
      Layout.preferredHeight: 25
      Layout.preferredWidth: 150
      onActivated: { script.refresh() }
    }

    Label {
      text: qsTr("Slices")
      visible: hasSlices
    }
    SpinBox {
      id: slices
      visible: hasSlices
      Layout.preferredHeight: 25
      Layout.preferredWidth: 50
      value: 10
      from: 1
      to: 100
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Stacks")
      visible: hasStacks
    }
    SpinBox {
      id: stacks
      visible: hasStacks
      Layout.preferredHeight: 25
      Layout.preferredWidth: 50
      value: 10
      from: 1
      to: 100
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Subdivisions")
      visible: hasSubdivisions
    }
    SpinBox {
      id: subdivisions
      visible: hasSubdivisions
      Layout.preferredHeight: 25
      Layout.preferredWidth: 50
      value: 2
      from: 0
      to: 20
      editable: true
      onValueModified: { script.refresh() }
    }
    
    Label {
      text: qsTr("Seed")
      visible: hasSeed
    }
    SpinBox {
      id: seed
      visible: hasSeed
      Layout.preferredHeight: 25
      Layout.preferredWidth: 50
      value: 0
      from: 0
      to: 100
      editable: true
      onValueModified: { script.refresh() }
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
      text: qsTr("Radius")
      visible: hasRadius
    }
    DoubleSpinBox {
      id: radius
      visible: hasRadius
      Layout.preferredHeight: 25
      Layout.preferredWidth: 75
      value: 0.5
      from: 0
      to: 1000
      stepSize: 0.1
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
      text: qsTr("Facetted")
    }
    CheckBox {
      id: facetted
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
      text: qsTr("Inside Out")
    }
    CheckBox {
      id: insideOut
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

    Item {}
    Button {
      id: insert
      text: qsTr("Insert")
      Layout.preferredHeight: 30
      Layout.preferredWidth: 125
      onClicked: { script.insert() }
    }
  }
}
