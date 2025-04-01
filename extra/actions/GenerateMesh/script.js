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
      indexed: ui.indexed
    }
    
    const typeIndex = this.ui.typeIndex
    ui.hasStacks = lib.hasSlicesStacks(typeIndex)
    ui.hasSlices = lib.hasSlicesStacks(typeIndex)
    ui.hasRadius = lib.hasRadius(typeIndex)
    ui.hasSubdivisions = lib.hasSubdivisions(typeIndex)
    ui.hasSeed = lib.hasSeed(typeIndex)
    
    if (app.session.item(this.vertices))
      this.generate()
  }
  
  insert() {
    let group = app.session.insertItem({
      name: 'Mesh',
      type: 'Group',
      inlineScope: true
    })    
    
    this.vertices = app.session.insertItem(group, {
      name: 'Vertices',
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
    
    if (this.settings.indexed) {
      this.indices = app.session.insertItem(group, {
        name: 'Indices',
        type: 'Buffer',
        items: [
          {
            name: 'Index',
            items: [
              {
                name: 'index',
                dataType: 'Uint16',
              }
            ]
          }
        ]
      })
    }
    
    app.session.insertItem(group, {
      name: 'Stream',
      type: 'Stream',
      items: [
        {
          name: 'aPosition',
          fieldId: this.vertices.items[0].items[0].id,
        },
        {
          name: 'aNormal',
          fieldId: this.vertices.items[0].items[1].id,
        },
        {
          name: 'aTexCoords',
          fieldId: this.vertices.items[0].items[2].id,
        }
      ]
    })
    this.generate()
  }
  
  generate() {
    const lib = this.library
    const geometry = lib.generate(JSON.stringify(this.settings))
    
    if (this.settings.indexed) {
      const vertices = lib.getVertices(geometry)
      const indices = lib.getIndices(geometry)
      if (!vertices.length)
        throw "Generating geometry failed"
        
      this.vertices.items[0].rowCount = vertices.length / 8
      app.session.setBufferData(this.vertices, vertices)
      
      this.indices.items[0].rowCount = indices.length
      app.session.setBufferData(this.indices, indices)
    }
    else {
      const vertices = lib.getVerticesUnweld(geometry)
      if (!vertices.length)
        throw "Generating geometry failed"
        
      this.vertices.items[0].rowCount = vertices.length / 8
      app.session.setBufferData(this.vertices, vertices)
    }
  }
}

this.script = new Script()

if (this.arguments) {
  this.script.settings = this.arguments
  this.script.insert()
  this.script.generate()
}
else {
  app.openEditor("ui.qml")
}
