
name = "&Generate Mesh..."

class Script {
  initialize(ui) {
    this.ui = ui
    this.library = app.loadLibrary("GenerateMesh")
    this.settings = this.library.getSettings()
    
    const typeCount = this.library.getTypeCount()
    const typeNames = []
    for (let i = 0; i < typeCount; ++i)
      typeNames[i] = this.library.getTypeName(i)
    this.ui.typeNames = typeNames
    
    this.refresh()
  }
  
  refresh() {
    const lib = this.library
    const s = this.settings
    const ui = this.ui
    lib.setType(s, ui.type)
    lib.setWidth(s, ui.width_)
    lib.setHeight(s, ui.height_)
    lib.setDepth(s, ui.depth)
    lib.setFacetted(s, ui.facetted)
    lib.setSlices(s, ui.slices)
    lib.setStacks(s, ui.stacks)
    lib.setRadius(s, ui.radius)
    lib.setSubdivisions(s, ui.subdivisions)
    lib.setSeed(s, ui.seed)
    lib.setInsideOut(s, ui.insideOut)
    lib.setScaleU(s, ui.scaleU)
    lib.setScaleV(s, ui.scaleV)
    
    ui.hasStacks = lib.hasSlicesStacks(s)
    ui.hasSlices = lib.hasSlicesStacks(s)
    ui.hasRadius = lib.hasRadius(s)
    ui.hasSubdivisions = lib.hasSubdivisions(s)
    ui.hasSeed = lib.hasSeed(s)
    
    if (app.session.item(this.buffer))
      this.generate()
  }
  
  insert() {
    this.buffer = app.session.insertItem({
      name: 'Mesh',
      type: 'Buffer',
      items: [
        {
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
    
    if (this.ui.addStream) {
      app.session.insertItem({
        name: 'Stream',
        type: 'Stream',
        items: [
          {
            name: 'a_position',
            fieldId: this.buffer.items[0].items[0].id
          },
          {
            name: 'a_normal',
            fieldId: this.buffer.items[0].items[1].id
          },
          {
            name: 'a_tex_coords',
            fieldId: this.buffer.items[0].items[2].id
          }
        ]
      })
    }
  }
  
  generate() {
    const lib = this.library
    const s = this.settings    
    const geometry = lib.generate(s)
    const vertices = lib.getVertices(s, geometry)
    const indices = lib.getIndices(s, geometry)
    if (!vertices.length)
      throw "Generating geometry failed"
      
    this.buffer.items[0].rowCount = vertices.length / 8
    
    app.session.setBufferData(this.buffer, vertices)
  }
}

function apply() {
  script = new Script()
  app.openEditor("ui.qml")
}
