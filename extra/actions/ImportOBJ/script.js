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
      scaleU: ui.scaleU,
      scaleV: ui.scaleV,
      normalize: ui.normalize,
      center: ui.center,
      swapYZ: ui.swapYZ,
      indexed: ui.indexed,
    }
    
    if (this.group)
      this.update()
  }
  
  findSessionItem(type) {
    for (let i = app.session.items.length - 1; i >= 0; --i)
      if (app.session.items[i].type == type)
        return app.session.items[i]
  }

  getBaseName(name) {
    return name.match(/[^/\\]+$/)[0]
  }
  
  insert() {
    if (!this.settings.fileName)
      return
      
    const lib = this.library
    this.model = lib.loadFile(this.settings.fileName)
    const error = lib.getError(this.model)
    if (error)
      throw new Error(error)
    lib.setSettings(this.model, JSON.stringify(this.settings))
      
    this.group = app.session.insertItem({
      name: this.getBaseName(this.settings.fileName),
      type: 'Group',
      inlineScope: true,
    })
    
    this.buffer = app.session.insertItem(this.group, {
      name: 'Mesh',
      type: 'Buffer',
    })
    
    // TODO: update items on insert
    this.buffer = app.session.item(this.buffer)
    
    this.update()
  }

  update() {
    const lib = this.library
    lib.setSettings(this.model, JSON.stringify(this.settings))

    let tempId = 10000
    const groupItems = []
    const addGroupItem = (item) => {
      if (!item.id)
        item.id = tempId++
      if (item.items)
        for (let subItem of item.items)
          if (!subItem.id)
            subItem.id = tempId++
      return groupItems[groupItems.push(item) - 1]
    }
    
    app.session.clearItem(this.buffer)
    addGroupItem(this.buffer)

    const targetId = this.findSessionItem('Target')?.id
    const programId = this.findSessionItem('Program')?.id

    const shapeCount = lib.getShapeCount(this.model)
    if (this.settings.indexed) {
      const vertices = lib.getVertices(this.model)
      const block = app.session.insertItem(this.buffer, {
        name: 'Vertices',
        type: 'Block',
        rowCount: vertices.length / 8,
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
      
      const stream = addGroupItem({
        name: 'Stream',
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
            name: 'aTexCoords',
            fieldId: block.items[2].id,
          }      
        ]
      })

      let offset = vertices.length * 4
      for (let i = 0; i < shapeCount; ++i) {
        const name = lib.getShapeName(this.model, i)        
        const indices = lib.getShapeIndices(this.model, i)
        const block = app.session.insertItem(this.buffer, {
          name: name,
          type: 'Block',
          rowCount: indices.length,
          offset: offset,
          items: [
            {
              name: 'index',
              dataType: 'Uint32',
            }
          ]
        })
        app.session.setBlockData(block, indices)
        offset += indices.length * 4
        
        addGroupItem({
          name: 'Draw ' + name,
          type: 'Call',
          callType: 'DrawIndexed',
          vertexStreamId: stream.id,
          indexBufferBlockId: block.id,
          targetId: targetId,
          programId: programId,
          count: "",
        })
      }
    }
    else {
      let offset = 0
      const streams = {
        name: 'Streams',
        type: 'Group',
        items: []
      }
      
      for (let i = 0; i < shapeCount; ++i) {
        const name = lib.getShapeName(this.model, i)
        const vertices = lib.getShapeVertices(this.model, i)
        const block = app.session.insertItem(this.buffer, {
          name: name,
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
        offset += vertices.length * 8
        
        streams.items.push({
          name: name,
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
              name: 'aTexCoords',
              fieldId: block.items[2].id,
            }      
          ]
        })
      }
      addGroupItem(streams)
      
      for (let i = 0; i < streams.items.length; ++i) {
        addGroupItem({
          name: 'Draw ' + streams.items[i].name,
          type: 'Call',
          callType: 'Draw',
          vertexStreamId: streams.items[i].id,
          targetId: targetId,
          programId: programId,
          count: "",
        })        
      }
    }
    
    app.session.replaceItems(this.group, groupItems)
  } // update
  
} // Script

this.script = new Script()

if (this.arguments) {
  this.script.settings = this.arguments
  this.script.insert()
  this.script.update()
}
else {
  app.openEditor("ui.qml")
}
