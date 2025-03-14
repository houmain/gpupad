"use strict"

var name = "Import gl&TF";

function applicable(items) {
  return true;
}

var PrimitiveType = {
  4: "Triangles",
};

var ComponentType_DataType = {
  5120: "Int8",
  5121: "UInt8",
  5122: "Int16",
  5123: "Uint16",
  5126: "Float",
};

var ComponentType_Size = {
  5120: 1,
  5121: 1,
  5122: 2,
  5123: 2,
  5126: 4,
};

var Type_Count = {
  "SCALAR": 1,
  "VEC2": 2,
  "VEC3": 3,
  "VEC4": 4,
  "MAT2": 4,
  "MAT3": 9,
  "MAT4": 16
};

var AttributeName = {
  "POSITION": "a_Position",
  "NORMAL": "a_Normal",
  "TANGENT": "a_Tangent",
  "TEXCOORD_0": "a_UV",
};

var Texture_Uniform = {
  "baseColorTexture": "u_BaseColorSampler",
  "normalTexture": "u_NormalSampler",
  "emissiveTexture": "u_EmissiveSampler",
  "occlusionTexture": "u_OcclusionSampler",
  "metallicRoughnessTexture": "u_MetallicRoughnessSampler",
};

var Factor_Uniform = {
  "emissiveFactor": "u_EmissiveFactor",
};

var gltf;
var basePath;
var group;
var idGenerator = 1;
var addedBuffers = { };
var addedStreams = { };
var addedTextures = { };

function nextId() { return idGenerator++; }

function getAbsolute(fileName) {
  return basePath + fileName;
}

function addBuffer(accessor, stride, name) {
  if (!addedBuffers[accessor.bufferView]) {
    var bufferView = gltf.bufferViews[accessor.bufferView];
    var buffer = {
      id: nextId(),
      type: "Buffer",
      name: name,
      fileName: getAbsolute(gltf.buffers[bufferView.buffer].uri),
      offset: bufferView.byteOffset,
      rowCount: (stride ? bufferView.byteLength / stride : 1),
      items: [],
    };
    addedBuffers[accessor.bufferView] = buffer;
    group.items.push(buffer);
  }
  return addedBuffers[accessor.bufferView];
}

function addIndexBuffer(accessor) {
  var stride = ComponentType_Size[accessor.componentType];
  var buffer = addBuffer(accessor, stride, "Indices");
  var column = {
    type: "Column",
    dataType: ComponentType_DataType[accessor.componentType],
  };
  buffer.items.push(column);
  return buffer;
}

function addColumn(accessor, name) {
  var buffer = addBuffer(accessor);
  var column = {
    id: nextId(),
    type: "Column",
    name: name,
    dataType: ComponentType_DataType[accessor.componentType],
    count: Type_Count[accessor.type],
  };
  buffer.items.push(column);
  return column;
}

function addStream(primitive) {
  if (!addedStreams[primitive]) {
    var stream = {
      id: nextId(),
      type: "Stream",
      items: [],
    };
  
    for (var a in primitive.attributes) {
      var accessor = gltf.accessors[primitive.attributes[a]];
      var stride = Type_Count[accessor.type] *
        ComponentType_Size[accessor.componentType];
      stream.items[stream.items.length] = {
        type: "Attribute",
        name: (AttributeName[a] || a),
        bufferId: addBuffer(accessor, stride).id,
        columnId: addColumn(accessor, a).id,
      };
    }
    addedStreams[primitive] = stream;
    group.items.push(stream);
  }
  return addedStreams[primitive];
}

function getFirstAttribute(primitive) {
  for (var a in primitive.attributes)
    return primitive.attributes[a];
}

function addDrawCall(primitive) {
  var call = {
    type: "Call",
    callType: "Draw",
    name: "Draw",
    vertexStreamId: addStream(primitive).id,
    primitiveType: PrimitiveType[primitive.mode || 4],
  };
  if (primitive.indices >= 0) {
    var indices = addIndexBuffer(gltf.accessors[primitive.indices]);
    call.callType = "DrawIndexed";
    call.indexBufferId = indices.id;
    call.count = indices.rowCount;
  }
  else {
    call.count = addBuffer(gltf.accessors[getFirstAttribute(primitive)]).rowCount;
  }
  group.items.push(call);
  return call;
}

function addTexture(imageIndex) {
  var image = gltf.images[imageIndex];
  if (!addedTextures[imageIndex]) {
    var texture = {
      id: nextId(),
      type: "Texture",
      fileName: getAbsolute(image.uri),
    };
    addedTextures[imageIndex] = texture;
    group.items.push(texture);
  }
  return addedTextures[imageIndex];
}

function addSamplerBinding(uniform, texture) {
  var binding = {
    type: "Binding",
    bindingType: "Sampler",
    name: uniform,
    values: [{
      textureId: texture.id,
      minFilter: "LinearMipMapLinear",
      magFilter: "Linear",                      
    }]
  };
  group.items.push(binding);
}

function setMaterial(material) {
  var uniform;
  for (var p in material) {
    if (p == "pbrMetallicRoughness") {
      setMaterial(material[p]);
    }
    else if (uniform = Texture_Uniform[p]) {
      addSamplerBinding(uniform, 
        addTexture(material[p].index));
    }
    else if (uniform = Factor_Uniform[p]) {
      console.log(p, " ", uniform);
    }
    else {
      console.log(p);
    }
  }
}

function findItemId(type, items) {
  if (!items)
    items = app.session.items;
  for (var i in items) {
    var item = items[i];
    if (item.type == type)
      return item.id;
    for (var c in item.items) {
      var itemId = findItemId(type, item.items);
      if (itemId)
        return itemId;
    }
  }
}

function apply(items) {
  var fileName = app.openFileDialog("*.gltf");
  if (!fileName)
    return;

  gltf = JSON.parse(app.readTextFile(fileName));

  var pathEnd = Math.max(fileName.lastIndexOf('/'), fileName.lastIndexOf('\\')) + 1;
  basePath = fileName.substr(0, pathEnd);
  fileName = fileName.substr(pathEnd, fileName.lastIndexOf('.') - pathEnd);

  group = {
    name: fileName,
    items: [],
  };

  for (var m in gltf.meshes) {
    var mesh = gltf.meshes[m];
    for (var p in mesh.primitives) {
      var primitive = mesh.primitives[p];
      if (primitive.material >= 0)
        setMaterial(gltf.materials[primitive.material]);
      
      var call = addDrawCall(primitive);

      // automatically use first program and target
      call.programId = findItemId("Program");
      call.targetId = findItemId("Target");
    }
  }
  app.session.insertItem(group);
}
