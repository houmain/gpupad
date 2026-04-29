"use strict"

const manifest = {
  name: `Make &Fragment Shader Session`,
  applicable: (app.currentEditor?.type == "Shader")
}

if (!manifest.applicable) throw 0
const shaderFileName = app.currentEditor.fileName

const json = app.processShader(shaderFileName, "json")
if (!json)
  throw "Invalid shader"
const reflection = JSON.parse(json)

const reuseFileName =
  app.findItem(item => (item.type == "Texture" &&
               app.isUntitled(item.fileName)))?.fileName;

app.clearSession()

let width = 512
let height = 512

// Target
const color = app.insertItem({
  type: "Texture",
  name: "Color",  
  width: width,
  height: height,
  format: "RGB8_UNorm",
  fileName: reuseFileName,  
})

app.openEditor(color)

let target = app.insertItem({
  type: "Target",
  items: [
    {
      textureId: color.id,
    }
  ]
})

// Bindings
for (let uniform of reflection.uniforms || []) {
  let bindingType = "Uniform"
  let values = undefined
  let textureId = undefined
  let editor = undefined
  
  if (uniform.type.match(/^[ui]?sampler/) ||
      uniform.type.match(/^[ui]?image/)) {
    bindingType = (uniform.type.match(/^[ui]?sampler/) ?
      "Sampler" : "Image");
    textureId = app.insertItem({
      type: "Texture",
      name: `Texture ${ uniform.name}`,
      width: 10,
      height: 10,
      format: "RGBA8_UNorm",
    }).id;
    
    const data = []
    for (let y = 0; y < 10; ++y)
      for (let x = 0; x < 10; ++x) {
        const c = (x + y) % 2;
        data.push(c);
        data.push(c);
        data.push(c);
        data.push(1);
      }
    app.setTextureData(textureId, data);
  }
  else if (uniform.name.match(/time/) &&
      uniform.type == 'float') {
    editor = "Expression";
    values = ['app.time'];
  }
  else if (uniform.name.match(/resolution/)&&
      uniform.type == 'vec2') {
    editor = "Expression2";
    values = [width, height];
  }
  else if (uniform.name.match(/mouse/) &&
      uniform.type == 'vec2') {
    editor = "Expression";
    values = ['app.mouse.fragCoord'];
  }
  else if (uniform.name.match(/color/) &&
      (uniform.type == 'vec3' || uniform.type == 'vec4')) {
    editor = "Color";
    values = [1,1,1,1];
  }
  else {
    switch (uniform.type) {
      case 'float':editor = "Expression";  values = [0]; break;
      case 'vec2': editor = "Expression2"; values = [0,0]; break;
      case 'vec3': editor = "Expression3"; values = [0,0,0]; break;
      case 'vec4': editor = "Expression4"; values = [0,0,0,0]; break;
    }    
  }
  
  app.insertItem({
    type: "Binding",
    name: uniform.name,
    bindingType: bindingType,
    editor: editor,
    values: values,    
    textureId: textureId,
  })
}

// Program
const program = app.insertItem({
  type: "Program",
})

// Vertex Shader
const vs = app.insertItem(program, {
  type: "Shader",
  name: "Vertex Shader",
  shaderType: "Vertex",
})

const varyingDeclarations = []
const varyings = []
for (let input of reflection.inputs || []) {
  varyingDeclarations.push(`out ${input.type} ${input.name};`);
  varyings.push(input.name);
}
varyings.push('');

app.setShaderSource(vs, `
${varyingDeclarations.join('\n')}

void main() {
  vec2 vertices[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
  gl_Position = vec4(vertices[gl_VertexID], 0, 1);
  ${varyings.join('.xy = ')} 0.5 * gl_Position.xy + vec2(0.5);
}
`)

// Fragment shader
const effect = app.insertItem(program, {
  type: "Shader",
  shaderType: "Fragment",
  fileName: shaderFileName,
})

// Call
const call = app.insertItem({
  type: "Call",
  callType: "Draw",
  programId: program.id,
  targetId: target.id,
  count: 3,
})

app.evaluation = 'Reset'
