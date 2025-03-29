"use strict"

const manifest = {
  name: "&Import Wavefront OBJ...",
}

class Script {
  constructor() {
    this.library = app.loadLibrary("ImportOBJ")
  }
  
  initializeUi(ui) {
    this.ui = ui
    this.refresh()
  }

  refresh() {
    const ui = this.ui
    this.settings = {
      fileName: ui.fileName,
      width: ui.width_,
      height: ui.height_,
      depth: ui.depth,
    }
    
    if (this.group)
      this.update()
  }

  getBaseName(name) {
    return name.match(/[^/\\]+$/)[0]
  }
  
  insert() {
    const lib = this.library
    this.model = lib.loadFile(this.settings.fileName)
    const error = lib.getError(this.model)
    if (error)
      throw new Error(error)
      
    this.group = app.session.insertItem({
      name: this.getBaseName(this.settings.fileName),
      type: 'Group',
      inlineScope: true,
    })
  }
  
  update() {
    const lib = this.library
    const group = this.group
    const model = this.model
    lib.setSettings(model, JSON.stringify(this.settings))

    app.session.clearItem(group)

    const buffer = app.session.insertItem(group, {
      name: 'Mesh',
      type: 'Buffer',
    })

    let offset = 0
    const shapeCount = lib.getShapeCount(model)
    for (let i = 0; i < shapeCount; ++i) {
      const vertices = lib.getShapeVertices(model, i)
      
      const block = app.session.insertItem(buffer, {
        name: 'Shape ' + i,
        type: 'Block',
        rowCount: vertices.length / 8,
        offset: offset,
        items: [
          {
            name: 'position',
            dataType: 'Float',
            count: 3
          },
          {
            name: 'normal',
            dataType: 'Float',
            count: 3
          },
          {
            name: 'texcoord',
            dataType: 'Float',
            count: 2
          }          
        ]
      })
      
      app.session.setBlockData(block, vertices)
      offset += block.rowCount

      app.session.insertItem(group, {
        name: 'Shape ' + i,
        type: 'Stream',
        items: [
          {
            name: 'aPosition',
            fieldId: block.items[0].id,
          },
          {
            name: 'aNormal',
            fieldId: block.items[1].id,
          },
          {
            name: 'aTexCoord',
            fieldId: block.items[2].id,
          }      
        ]
      })
    }
  }
}

this.script = new Script()

if (this.arguments) {
  this.script.settings = this.arguments
  this.script.insert()
  this.script.update()
}
else {
  app.openEditor("ui.qml")
}
