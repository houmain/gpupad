"use strict"

const manifest = {
  name: "&Generate Mesh..."
}

class Script {
  constructor() {
    this.library = app.loadLibrary("GenerateMesh")
  }
  
  initializeUi(ui) {
    this.ui = ui
    
    const typeCount = this.library.getTypeCount()
    const typeNames = []
    for (let i = 0; i < typeCount; ++i)
      typeNames[i] = this.library.getTypeName(i)
    this.ui.typeNames = typeNames
    
    this.refresh()
  }
  
  refresh() {
    const lib = this.library
    const ui = this.ui
    
    this.settings = {
      type: ui.type,
      width: ui.width_,
      height: ui.height_,
      depth: ui.depth,
      facetted: ui.facetted,
      slices: ui.slices,
      stacks: ui.stacks,
      radius: ui.radius,
      subdivisions: ui.subdivisions,
      seed: ui.seed,
      insideOut: ui.insideOut,
      swapYZ: ui.swapYZ,
      scaleU: ui.scaleU,
      scaleV: ui.scaleV,
      drawCall: ui.drawCall,
      indexed: ui.indexed,
      vertexPadding: ui.vertexPadding,
    }
    
    const typeIndex = this.ui.typeIndex
    ui.hasStacks = lib.hasSlicesStacks(typeIndex)
    ui.hasSlices = lib.hasSlicesStacks(typeIndex)
    ui.hasRadius = lib.hasRadius(typeIndex)
    ui.hasSubdivisions = lib.hasSubdivisions(typeIndex)
    ui.hasSeed = lib.hasSeed(typeIndex)
    
    if (app.session.findItem(this.buffer))
      this.generate()
  }
  
  insert() {
    this.group = app.session.findItem(this.settings.group)
    
    if (!this.group)
      this.group =
        app.session.insertItem(this.settings.parent || app.session, {
          name: (this.settings.name || 'Mesh'),
          type: 'Group',
          inlineScope: true,
          dynamic: this.settings.dynamic,
        })

    this.buffer =
      app.session.findItem(this.group, item => item.type == 'Buffer') ||
      app.session.insertItem(this.group, {
        name: 'Buffer',
        type: 'Buffer',
        items: [
          {
            name: 'Vertex',
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
          }
        ]
      })
    this.vertices = this.buffer.items[0]
    this.indices = this.buffer.items[1]
    
    this.stream =
      app.session.findItem(this.group, item => item.type == 'Stream') ||
      app.session.insertItem(this.group, {
        name: 'Stream',
        type: 'Stream',
        items: [
          {
            name: 'aPosition',
            fieldId: this.buffer.items[0].items[0].id,
          },
          {
            name: 'aNormal',
            fieldId: this.buffer.items[0].items[1].id,
          },
          {
            name: 'aTexCoords',
            fieldId: this.buffer.items[0].items[2].id,
          }
        ]
      })
    
    this.drawCall = app.session.findItem(this.group, item => item.type == 'Call')
    
    app.session.replaceItems(this.group, [this.buffer, this.stream, this.drawCall])
    
    this.generate()
  }
  
  updateBuffer() {
    const lib = this.library
    const geometry = lib.generate(JSON.stringify(this.settings))
    const padding = this.settings.vertexPadding || 0
    const components = 8
    if (this.settings.indexed) {
      const vertices = lib.getVertices(geometry)
      if (!vertices.length)
        throw "Generating geometry failed"
        
      this.vertices.items[2].padding = padding * 4
      this.vertices.rowCount = vertices.length / components
      app.session.setBlockData(this.vertices, vertices)
      
      if (!this.indices)
        this.indices = app.session.insertItem(this.buffer, {
          name: 'Indices',
          type: 'Block',
          items: [
            {
              name: 'index',
              dataType: 'Uint32',
              count: 3,
            }
          ]
        })
      
      const indices = lib.getIndices(geometry)
      this.indices.offset = this.vertices.rowCount * (components + padding) * 4
      this.indices.rowCount = indices.length / 3
      app.session.setBlockData(this.indices, indices)
    }
    else {
      const vertices = lib.getVerticesUnweld(geometry)
      if (!vertices.length)
        throw "Generating geometry failed"
        
      this.vertices.items[2].padding = this.settings.vertexPadding * 4
      this.vertices.rowCount = vertices.length / components
      app.session.setBlockData(this.vertices, vertices)
      
      if (this.indices) {
        app.session.deleteItem(this.indices)
        this.indices = undefined
      }
    }
  }
  
  updateDrawCall() {
    if (this.settings.drawCall === false) {
      if (this.drawCall)
        app.session.deleteItem(this.drawCall)
      this.drawCall = undefined
      return
    }
    
    if (!this.drawCall) {
      const target = app.session.findItem(item => item.type == 'Target')

      const program = app.session.findItem(
        item => (item.type == 'Program' &&
          item.items[0]?.shaderType == "Vertex"))

      this.drawCall = app.session.insertItem(this.group, {
        name: 'Draw',
        type: 'Call',
        vertexStreamId: this.stream?.id,
        targetId: target?.id,
        programId: program?.id,
      })
    }
    
    this.drawCall.count =
      (this.settings.indexed ?
        this.indices.rowCount * 3 : this.vertices.rowCount)
    this.drawCall.callType =
      (this.settings.indexed ? 'DrawIndexed' : 'Draw')
    this.drawCall.indexBufferBlockId = this.indices?.id
  }
  
  generate() {
    this.updateBuffer()
    this.updateDrawCall()
  }
} // Script

this.script = new Script()

if (this.arguments) {
  const settings = this.arguments
  this.script.settings = settings
  this.script.insert()
  this.script.generate()
  this.result = this.script.group
}
else {
  app.openEditor("ui.qml", manifest.name)
}
