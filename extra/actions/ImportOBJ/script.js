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
      drawCalls: ui.drawCalls,
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
      name: 'Buffer',
      type: 'Buffer',
    })
    
    this.streams = app.session.insertItem(this.group, {
      name: 'Streams',
      type: 'Group',
    })
    
    this.update()
  }
  
  update() {
    const lib = this.library
    lib.setSettings(this.model, JSON.stringify(this.settings))
    
    this.updateBuffer()
    this.updateStreams()
    this.updateDrawCalls()
  }  
  
  updateBuffer() {
    const lib = this.library
    app.session.clearItem(this.buffer)
    
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
      
      let offset = vertices.length * 4
      for (let i = 0; i < shapeCount; ++i) {
        const name = lib.getShapeName(this.model, i)        
        const indices = lib.getShapeIndices(this.model, i)
        const block = app.session.insertItem(this.buffer, {
          name: name,
          type: 'Block',
          rowCount: indices.length / 3,
          offset: offset,
          items: [
            {
              name: 'index',
              dataType: 'Uint32',
              count: 3,
            }
          ]
        })
        app.session.setBlockData(block, indices)
        offset += indices.length * 4
      }
    }
    else {
      let offset = 0
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
      }
    }
    // TODO: update items on insert
    this.buffer = app.session.item(this.buffer)
  }
  
  updateStreams() {
    let streams = []
    if (this.settings.indexed) {
      const block = this.buffer.items[0]
      streams.push({
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
    }
    else {
      for (let block of this.buffer.items)
        streams.push({
          name: block.name,
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
    
    // TODO: make replace items also replace sub items
    app.session.clearItem(this.streams)
    app.session.replaceItems(this.streams, streams)
    // TODO: update items on insert
    this.streams = app.session.item(this.streams)
  }
  
  updateDrawCalls() {
    if (!this.settings.drawCalls) {
      if (this.drawCalls)
        app.session.deleteItem(this.drawCalls)
      this.drawCalls = undefined
      return
    }
    
    if (!this.drawCalls)
      this.drawCalls = app.session.insertItem(this.group, {
        name: 'Calls',
        type: 'Group',
      })
      
    const targetId = this.findSessionItem('Target')?.id
    const programId = this.findSessionItem('Program')?.id
      
    let drawCalls = []
    if (this.settings.indexed) {
      for (let i = 1; i < this.buffer.items.length; ++i) {
        const stream = this.streams.items[0]
        const indices = this.buffer.items[i]
        drawCalls.push({
          name: 'Draw ' + indices.name,
          type: 'Call',
          callType: 'DrawIndexed',
          vertexStreamId: stream.id,
          indexBufferBlockId: indices.id,
          targetId: targetId,
          programId: programId,
          count: indices.rowCount * 3,
        })
      }
    }
    else {
      for (let i = 0; i < this.streams.items.length; ++i) {
        const stream = this.streams.items[i]
        drawCalls.push({
          name: 'Draw ' + stream.name,
          type: 'Call',
          callType: 'Draw',
          vertexStreamId: stream.id,
          targetId: targetId,
          programId: programId,
          count: this.buffer.items[i].rowCount,
        })
      }
    }

    app.session.replaceItems(this.drawCalls, drawCalls)
  }
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
