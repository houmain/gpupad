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
  
  findSessionItem(predicate) {
    for (let i = app.session.items.length - 1; i >= 0; --i)
      if (predicate(app.session.items[i]))
          return app.session.items[i]
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
    
    if (app.session.item(this.buffer))
      this.generate()
  }
  
  insert() {
    this.group = app.session.insertItem({
      name: 'Mesh',
      type: 'Group',
      inlineScope: true
    })    
    
    this.buffer = app.session.insertItem(this.group, {
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
    
    this.stream = app.session.insertItem(this.group, {
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
    
    this.indices = undefined
    this.drawCall = undefined
    this.generate()
  }
  
  updateBuffer() {
    const lib = this.library
    const geometry = lib.generate(JSON.stringify(this.settings))
    const padding = this.settings.vertexPadding
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
    if (!this.settings.drawCall) {
      if (this.drawCall)
        app.session.deleteItem(this.drawCall)
      this.drawCall = undefined
      return
    }
    
    if (!this.drawCall) {
      const target = this.findSessionItem(
        (item) => (item.type == 'Target'))

      const program = this.findSessionItem(
        (item) => (item.type == 'Program' &&
          item.items[0]?.shaderType == "Vertex"))

      this.drawCall = app.session.insertItem(this.group, {
        name: 'Draw',
        type: 'Call',
        vertexStreamId: this.stream.id,
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
  this.script.settings = this.arguments
  this.script.insert()
  this.script.generate()
}
else {
  app.openEditor("ui.qml")
}
