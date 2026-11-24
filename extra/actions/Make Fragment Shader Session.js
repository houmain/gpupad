"use strict"

const manifest = {
  name: `Make &Fragment Shader Session`,
  applicable: (app.currentEditor?.type == "Shader")
}

if (!manifest.applicable) throw 0
const shaderFileName = app.currentEditor.fileName

const json = app.processShader(shaderFileName, "json")
if (!json)
  throw new Error("Invalid shader")
const reflection = JSON.parse(json)

const reuseFileName =
  app.session.findItem(item => (item.type == "Texture" &&
    app.isUntitled(item.fileName)))?.fileName;

const session = app.session
session.clear()

let width = 512
let height = 512

// Target
const color = session.insert({
  type: "Texture",
  name: "Color",
  width: width,
  height: height,
  format: "RGBA8_UNorm",
  fileName: reuseFileName,
})

color.openEditor()

let target = session.insert({
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
    textureId = session.insert({
      type: "Texture",
      name: `Texture ${uniform.name}`,
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
    textureId.setData(data);
  }
  else if (uniform.name.match(/time/) &&
    uniform.type == 'float') {
    editor = "Expression";
    values = ['app.time'];
  }
  else if (uniform.name.match(/resolution/) &&
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
    values = [1, 1, 1, 1];
  }
  else {
    switch (uniform.type) {
      case 'float': editor = "Expression"; values = [0]; break;
      case 'vec2': editor = "Expression2"; values = [0, 0]; break;
      case 'vec3': editor = "Expression3"; values = [0, 0, 0]; break;
      case 'vec4': editor = "Expression4"; values = [0, 0, 0, 0]; break;
    }
  }

  session.insert({
    type: "Binding",
    name: uniform.name,
    bindingType: bindingType,
    editor: editor,
    values: values,
    textureId: textureId,
  })
}

// Program
const program = session.insert({
  type: "Program",
})

// Vertex Shader
const vs = program.insert({
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

vs.setSource(`
${varyingDeclarations.join('\n')}

void main() {
  vec2 vertices[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
  gl_Position = vec4(vertices[gl_VertexID], 0, 1);
  ${varyings.join('.xy = ')} 0.5 * gl_Position.xy + vec2(0.5);
}
`)

// Fragment shader
const effect = program.insert({
  type: "Shader",
  shaderType: "Fragment",
  fileName: shaderFileName,
})

// Call
const call = session.insert({
  type: "Call",
  callType: "Draw",
  programId: program.id,
  targetId: target.id,
  count: 3,
})

app.evaluate(true)
