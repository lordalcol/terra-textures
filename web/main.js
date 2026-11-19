// Terra Textures in WebGL: the same sphere and the same six layers as the
// 2009 OpenGL program (TerraTextures/sfera.cpp), combined in one fragment
// shader instead of fixed-function texture units.

const LAYERS = [
  { name: 'The Earth', file: 'earth.jpg', on: true },
  { name: 'The grid', file: 'earth-grid.png', on: true },
  { name: 'Red highlight on Europe', file: 'earth-europa.png', on: false },
  { name: 'Red highlight on America', file: 'earth-america.png', on: false },
  { name: 'Red highlight on Oceania', file: 'earth-oceania.png', on: false },
  { name: 'The clouds', file: 'fog.jpg', on: true },
];

const DEGREES_PER_SECOND = 18;
const CLOUD_DRIFT_PER_SECOND = 0.018; // texture widths per second
const AXIAL_TILT = 23.44;
const STEP = 5; // degrees between vertices

const VERTEX_SHADER = `
attribute vec3 aPosition;
attribute vec2 aUV;
uniform mat4 uMatrix;
varying vec2 vUV;
void main() {
  vUV = aUV;
  gl_Position = uMatrix * vec4(aPosition, 1.0);
}`;

// Like the 2009 program: every colour layer multiplies the colour (white
// leaves it unchanged), then the clouds: color * (1 - cloud) + cloud. The
// cloud texture is dark where the clouds are thick, so cloud = 1 - texel.
const FRAGMENT_SHADER = `
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex[6];
uniform float uOn[6];
uniform float uCloudOffset;
// A layer that is off counts as white: it leaves the colour unchanged.
vec3 layer(float on, vec3 texel) {
  return mix(vec3(1.0), texel, on);
}
void main() {
  vec3 color = layer(uOn[0], texture2D(uTex[0], vUV).rgb);
  color *= layer(uOn[1], texture2D(uTex[1], vUV).rgb);
  color *= layer(uOn[2], texture2D(uTex[2], vUV).rgb);
  color *= layer(uOn[3], texture2D(uTex[3], vUV).rgb);
  color *= layer(uOn[4], texture2D(uTex[4], vUV).rgb);
  float cloud = (1.0 - texture2D(uTex[5], vUV + vec2(uCloudOffset, 0.0)).r) * uOn[5];
  gl_FragColor = vec4(color * (1.0 - cloud) + cloud, 1.0);
}`;

// ------------------------------------------------------------------ the sphere

/** Rings of triangle strips from the south pole to the north pole, as in sfera.cpp. */
function buildSphere() {
  const rings = 180 / STEP;
  const segments = 360 / STEP;
  const data = [];
  for (let ring = 0; ring < rings; ring++) {
    for (let seg = 0; seg <= segments; seg++) {
      for (let k = 0; k < 2; k++) {
        const lat = -90 + (ring + 1 - k) * STEP;
        const lon = -180 + seg * STEP;
        const la = (lat * Math.PI) / 180;
        const lo = (lon * Math.PI) / 180;
        data.push(Math.cos(la) * Math.sin(lo), Math.sin(la), Math.cos(la) * Math.cos(lo));
        // WebGL textures start at the top row, BMPs in OpenGL at the bottom.
        data.push((lon + 180) / 360, 1 - (lat + 90) / 180);
      }
    }
    // Two repeated vertices join this band's strip to the next one with
    // degenerate triangles, so the whole sphere is one draw call.
    if (ring < rings - 1) {
      const last = data.slice(-5);
      data.push(...last);
      const lat = -90 + (ring + 2) * STEP;
      const la = (lat * Math.PI) / 180;
      const lo = (-180 * Math.PI) / 180;
      data.push(Math.cos(la) * Math.sin(lo), Math.sin(la), Math.cos(la) * Math.cos(lo), 0, 1 - (lat + 90) / 180);
    }
  }
  return new Float32Array(data);
}

// ------------------------------------------------------------------ matrices

const multiply = (a, b) => {
  const out = new Float32Array(16);
  for (let c = 0; c < 4; c++)
    for (let r = 0; r < 4; r++)
      for (let k = 0; k < 4; k++) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return out;
};
const perspective = (fovDeg, aspect, near, far) => {
  const f = 1 / Math.tan((fovDeg * Math.PI) / 360);
  return new Float32Array([f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) / (near - far), -1, 0, 0, (2 * far * near) / (near - far), 0]);
};
const translate = (x, y, z) => new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1]);
const rotate = (deg, axis) => {
  const r = (deg * Math.PI) / 180;
  const c = Math.cos(r);
  const s = Math.sin(r);
  if (axis === 'x') return new Float32Array([1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1]);
  if (axis === 'y') return new Float32Array([c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]);
  return new Float32Array([c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
};

// ------------------------------------------------------------------ WebGL

function compile(gl, type, source) {
  const shader = gl.createShader(type);
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader) ?? 'shader error');
  return shader;
}

function loadTexture(gl, unit, url) {
  const texture = gl.createTexture();
  gl.activeTexture(gl.TEXTURE0 + unit);
  gl.bindTexture(gl.TEXTURE_2D, texture);
  // a white pixel until the image arrives
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, 1, 1, 0, gl.RGB, gl.UNSIGNED_BYTE, new Uint8Array([255, 255, 255]));
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.onload = () => {
      gl.activeTexture(gl.TEXTURE0 + unit);
      gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, gl.RGB, gl.UNSIGNED_BYTE, image);
      gl.generateMipmap(gl.TEXTURE_2D);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.REPEAT);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
      resolve();
    };
    image.onerror = () => reject(new Error(`Can't load ${url}`));
    image.src = url;
  });
}

// ------------------------------------------------------------------ page

const canvas = document.getElementById('globe');
const errorBox = document.getElementById('error');
const spinButton = document.getElementById('spin');
const layerBox = document.getElementById('layers');

const state = { spinning: true, angle: 200, cloudOffset: 0, dragTilt: 15, last: performance.now() };

function showError(message) {
  errorBox.textContent = message;
  errorBox.hidden = false;
}

function renderLayerToggles() {
  layerBox.innerHTML = '';
  LAYERS.forEach((layer, i) => {
    const label = document.createElement('label');
    label.className = 'layer';
    const box = document.createElement('input');
    box.type = 'checkbox';
    box.checked = layer.on;
    box.addEventListener('change', () => {
      layer.on = box.checked;
    });
    label.append(box, Object.assign(document.createElement('kbd'), { textContent: String(i + 1) }), document.createTextNode(layer.name));
    layerBox.append(label);
  });
}

function setSpinning(on) {
  state.spinning = on;
  spinButton.setAttribute('aria-pressed', String(on));
  spinButton.querySelector('span').textContent = on ? 'Stop the rotation' : 'Start the rotation';
  spinButton.querySelector('svg').innerHTML = on ? '<path d="M8 5v14M16 5v14"/>' : '<path d="M7 4l13 8-13 8z" fill="currentColor"/>';
}

function main() {
  renderLayerToggles();
  const gl = canvas.getContext('webgl', { antialias: true });
  if (!gl) {
    showError('This browser has WebGL turned off or unavailable.');
    return;
  }

  const program = gl.createProgram();
  gl.attachShader(program, compile(gl, gl.VERTEX_SHADER, VERTEX_SHADER));
  gl.attachShader(program, compile(gl, gl.FRAGMENT_SHADER, FRAGMENT_SHADER));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program) ?? 'link error');
  gl.useProgram(program);

  const vertices = buildSphere();
  gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
  gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);
  const aPosition = gl.getAttribLocation(program, 'aPosition');
  const aUV = gl.getAttribLocation(program, 'aUV');
  gl.enableVertexAttribArray(aPosition);
  gl.enableVertexAttribArray(aUV);
  gl.vertexAttribPointer(aPosition, 3, gl.FLOAT, false, 20, 0);
  gl.vertexAttribPointer(aUV, 2, gl.FLOAT, false, 20, 12);
  const count = vertices.length / 5;

  const uMatrix = gl.getUniformLocation(program, 'uMatrix');
  const uOn = gl.getUniformLocation(program, 'uOn');
  const uCloudOffset = gl.getUniformLocation(program, 'uCloudOffset');
  gl.uniform1iv(gl.getUniformLocation(program, 'uTex'), [0, 1, 2, 3, 4, 5]);

  Promise.all(LAYERS.map((layer, i) => loadTexture(gl, i, new URL(`textures/${layer.file}`, import.meta.url).href))).catch((e) =>
    showError(e.message),
  );

  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.CULL_FACE);
  gl.clearColor(0, 0, 0, 1);

  function frame(now) {
    const dt = Math.min((now - state.last) / 1000, 0.1);
    state.last = now;
    if (state.spinning) state.angle = (state.angle + DEGREES_PER_SECOND * dt) % 360;
    state.cloudOffset = (state.cloudOffset - CLOUD_DRIFT_PER_SECOND * dt) % 1;

    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.round(canvas.clientWidth * dpr);
    const h = Math.round(canvas.clientHeight * dpr);
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
    gl.viewport(0, 0, w, h);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    let m = perspective(45, w / h, 0.1, 100);
    m = multiply(m, translate(0, 0, -3.2));
    m = multiply(m, rotate(state.dragTilt, 'x'));
    m = multiply(m, rotate(AXIAL_TILT, 'z'));
    m = multiply(m, rotate(state.angle, 'y'));
    gl.uniformMatrix4fv(uMatrix, false, m);
    gl.uniform1fv(uOn, LAYERS.map((l) => (l.on ? 1 : 0)));
    gl.uniform1f(uCloudOffset, state.cloudOffset);
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, count);
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

// ------------------------------------------------------------------ input

spinButton.addEventListener('click', () => setSpinning(!state.spinning));

document.addEventListener('keydown', (e) => {
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (e.target instanceof HTMLInputElement && e.key === ' ') return;
  if (e.key === ' ') {
    e.preventDefault();
    setSpinning(!state.spinning);
  } else if (e.key >= '1' && e.key <= '6') {
    const i = Number(e.key) - 1;
    LAYERS[i].on = !LAYERS[i].on;
    layerBox.querySelectorAll('input')[i].checked = LAYERS[i].on;
  }
});

// Drag to turn the globe around its axis and tilt the view.
let drag = null;
canvas.addEventListener('pointerdown', (e) => {
  drag = { x: e.clientX, y: e.clientY };
  canvas.setPointerCapture(e.pointerId);
});
canvas.addEventListener('pointermove', (e) => {
  if (!drag) return;
  const scale = 180 / canvas.clientWidth;
  state.angle += (e.clientX - drag.x) * scale;
  state.dragTilt = Math.max(-80, Math.min(80, state.dragTilt + (e.clientY - drag.y) * scale));
  drag = { x: e.clientX, y: e.clientY };
});
const endDrag = () => {
  drag = null;
};
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);

try {
  main();
} catch (e) {
  showError(e instanceof Error ? e.message : String(e));
}
