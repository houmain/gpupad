"use strict"

const manifest = {
  name: "&Import Wavefront OBJ",
}

function getBaseName(name) {
  return name.match(/[^/\\]+$/)[0]
}

const lib = app.loadLibrary("ImportOBJ")
const fileName = app.openFileDialog("*.obj")
if (!fileName)
  throw 0

const mesh = lib.loadFile(fileName)
const error = lib.getError(mesh)
if (error)
  throw new Error(error)

const group = app.session.insertItem({
  name: getBaseName(fileName),
  type: 'Group',
  inlineScope: true,
})

const buffer = app.session.insertItem(group, {
  name: 'Mesh',
  type: 'Buffer',
})

let offset = 0
const shapeCount = lib.getShapeCount(mesh)
for (let i = 0; i < shapeCount; ++i) {
  const vertices = lib.getShapeMeshVertices(mesh, i)
  
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
        fieldId = block.items[0].id,
      },
      {
        name: 'aNormal',
        fieldId = block.items[1].id,
      },
      {
        name: 'aTexCoord',
        fieldId = block.items[2].id,
      }      
    ]
  })
}

