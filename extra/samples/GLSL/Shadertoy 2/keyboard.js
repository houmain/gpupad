
toggled = this.toggled || []

const keys = app.keyboard.keys
const data = []
for (let i = 0; i < 256; ++i) {
  const key = keys[i]
  if (key == 2)
    toggled[i] = (toggled[i] == 1 ? 0 : 1)
    
  data[i] = (key > 0 ? 1 : 0)
  data[256 + i] = (key == 2 ? 1 : 0)
  data[512 + i] = toggled[i]
}
app.session.setTextureData("Textures/Keyboard", data)
