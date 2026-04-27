import App

ThemedScrollView {
  id: root

  ColumnLayout {
    ColumnLayout {
      id: sliders
      spacing: 30
      Layout.topMargin: 20
      Layout.leftMargin: 20    
      
      Label {
        text: `Session: '${app.session.name}'`
      }      
      
      Label {
        text: `Current Editor: '${app.currentEditor?.title}'`
      }
      
      Label {
        text: `Viewport Size: '${app.currentEditor?.viewportSize}`
      }
    }
  }

  Component.onCompleted: script.initializeUi(root)

  Component {
    id: sliderComponent
    
    Item {
      id: root
      property string name;
      property int bindingId
      property alias value: slider.value
      RowLayout {
        Label {
          id: label
          text: root.name
        }
        Slider {
          id: slider
          from: 0
          to: 10
          onMoved: {
            app.findItem(bindingId).values = [
              this.value,
            ]
          }
        }
      }
    }
  }

  function addSlider(binding) {
    var slider = sliderComponent.createObject(sliders, { });
    slider.bindingId = binding.id
    updateSlider(binding, slider)
    return slider
  }
  
  function updateSlider(binding, slider) {
    slider.name = binding.name
    slider.value = parseFloat(binding.values[0])
  }
  
  function removeSlider(slider) {
    slider.destroy();
  }
}
