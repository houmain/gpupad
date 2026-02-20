import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.0

ScrollView {
  id: root

  Component.onCompleted: script.initializeUi(root)

  Component {
    id: sliderComponent
    
    Item {
      id: root
      property string name;
      property int bindingId
      property alias value: slider.value
      Label {
        id: label
        text: root.name
      }
      Slider {
        id: slider
        x: 100
        from: 0
        to: 10
        onMoved: {
          app.session.findItem(bindingId).values = [
            this.value,
          ]
        }
      }
    }
  }

  function addSlider(binding, y) {
      var slider = sliderComponent.createObject(root, {
        x = 10,
        y = y,
      });
      slider.name = binding.name
      slider.bindingId = binding.id
      slider.value = parseFloat(binding.values[0])
  }  
}
