
mouseState = this.mouseState || {
  position: [0, 0],
  clickPosition: [0, 0]
}

const button = app.mouse.buttons[Qt.LeftButton]
const fragCoord = app.mouse.fragCoord
const position = [Math.floor(fragCoord[0]), Math.floor(fragCoord[1])]

if (button > 0) {
  mouseState.position = position
  if (button == 2)
    mouseState.clickPosition = position
}

const clickX = mouseState.clickPosition[0]
const clickY = mouseState.clickPosition[1]
shadertoyMouse = [
  mouseState.position[0],
  mouseState.position[1],
  button > 0 ? clickX : -clickX,
  button == 2 ? clickY : -clickY
]

this.mouseState = mouseState

toggled = this.toggled || new Array(256).fill(0)

const keys = app.keyboard.keysByKeyCode
const data = []
for (let i = 0; i < 256; ++i) {
  const key = keys[i]
  if (key == 2)
    toggled[i] = (toggled[i] == 1 ? 0 : 1)

  data[i] = (key > 0 ? 1 : 0)
  data[256 + i] = (key == 2 ? 1 : 0)
  data[512 + i] = toggled[i]
}
app.session.getItem("Textures/Keyboard").setData(data)
