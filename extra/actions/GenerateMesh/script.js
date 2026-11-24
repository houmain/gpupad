"use strict"

const manifest = {
  name: "&Generate Mesh..."
}

const session = app.session

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

    if (session.findItem(this.buffer))
      this.generate()
  }

  insert() {
    this.group = session.findItem(this.settings.group)

    if (!this.group)
      this.group =
        (this.settings.parent || session).insert({
          name: 'Mesh',
          type: 'Group',
          inlineScope: true,
        })

    if (this.settings.name)
      this.group.name = this.settings.name
    if (this.settings.dynamic)
      this.group.dynamic = true

    this.buffer =
      this.group.findItem(item => item.type == 'Buffer') ||
      this.group.insert({
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
      this.group.findItem(item => item.type == 'Stream') ||
      this.group.insert({
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

    this.drawCall = this.group.findItem(item => item.type == 'Call')

    this.group.replaceItems([this.buffer, this.stream, this.drawCall])

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
        throw new Error("Generating geometry failed")

      this.vertices.items[2].padding = padding * 4
      this.vertices.rowCount = vertices.length / components
      this.vertices.setData(vertices)

      if (!this.indices)
        this.indices = this.buffer.insert({
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
      this.indices.setData(indices)
    }
    else {
      const vertices = lib.getVerticesUnweld(geometry)
      if (!vertices.length)
        throw new Error("Generating geometry failed")

      this.vertices.items[2].padding = this.settings.vertexPadding * 4
      this.vertices.rowCount = vertices.length / components
      this.vertices.setData(vertices)

      if (this.indices) {
        this.indices.remove()
        this.indices = undefined
      }
    }
  }

  updateDrawCall() {
    if (this.settings.drawCall === false) {
      if (this.drawCall)
        this.drawCall.remove()
      this.drawCall = undefined
      return
    }

    if (!this.drawCall) {
      const target = session.findItem(item => item.type == 'Target')

      const program = session.findItem(
        item => (item.type == 'Program' &&
          item.items[0]?.shaderType == "Vertex"))

      this.drawCall = this.group.insert({
        name: 'Draw',
        type: 'Call',
        targetId: target?.id,
        programId: program?.id,
      })
    }

    this.drawCall.count =
      (this.settings.indexed ?
        this.indices.rowCount * 3 : this.vertices.rowCount)
    this.drawCall.callType =
      (this.settings.indexed ? 'DrawIndexed' : 'Draw')
    this.drawCall.vertexStreamId = this.stream?.id
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
  app.openEditor("ui.qml").title = manifest.name
}
