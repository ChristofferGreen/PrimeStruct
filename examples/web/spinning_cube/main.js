const canvas = document.querySelector("#cube-canvas");
const statusNode = document.querySelector("#status");

if (!(canvas instanceof HTMLCanvasElement) || statusNode === null) {
  throw new Error("Missing spinning_cube host DOM nodes");
}

const ctx = canvas.getContext("2d");
if (ctx === null) {
  throw new Error("2D canvas context is unavailable");
}

const wasmUrl = new URL("./cube.wasm", import.meta.url);

async function instantiateCubeModule() {
  try {
    if (WebAssembly.instantiateStreaming) {
      const response = await fetch(wasmUrl);
      if (!response.ok) {
        throw new Error(`Failed to fetch cube.wasm (${response.status})`);
      }
      return await WebAssembly.instantiateStreaming(response, {});
    }

    const response = await fetch(wasmUrl);
    if (!response.ok) {
      throw new Error(`Failed to fetch cube.wasm (${response.status})`);
    }
    const bytes = await response.arrayBuffer();
    return await WebAssembly.instantiate(bytes, {});
  } catch (error) {
    statusNode.textContent = `Wasm load skipped: ${String(error)}`;
    return null;
  }
}

function drawWireCubeProxy(tick) {
  const w = canvas.width;
  const h = canvas.height;
  const cx = w * 0.5;
  const cy = h * 0.52;
  const radius = Math.min(w, h) * 0.2;
  const depth = radius * 0.55;
  const angle = (tick % 360) * (Math.PI / 180);

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#0b1424";
  ctx.fillRect(0, 0, w, h);

  const points = [
    [-1, -1],
    [1, -1],
    [1, 1],
    [-1, 1],
  ];

  const rotate = ([x, y], offset = 0) => {
    const a = angle + offset;
    const rx = x * Math.cos(a) - y * Math.sin(a);
    const ry = x * Math.sin(a) + y * Math.cos(a);
    return [cx + rx * radius, cy + ry * radius * 0.65];
  };

  const front = points.map((point) => rotate(point));
  const back = points.map((point) => {
    const [x, y] = rotate(point, 0.35);
    return [x + depth, y - depth * 0.6];
  });

  const drawLoop = (loop, stroke) => {
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(loop[0][0], loop[0][1]);
    for (let i = 1; i < loop.length; i += 1) {
      ctx.lineTo(loop[i][0], loop[i][1]);
    }
    ctx.closePath();
    ctx.stroke();
  };

  drawLoop(back, "#3f89ff");
  drawLoop(front, "#8dd5ff");

  ctx.strokeStyle = "#6fb8ff";
  ctx.lineWidth = 2;
  for (let i = 0; i < 4; i += 1) {
    ctx.beginPath();
    ctx.moveTo(front[i][0], front[i][1]);
    ctx.lineTo(back[i][0], back[i][1]);
    ctx.stroke();
  }
}

function createTicker(exportedMain) {
  if (typeof exportedMain !== "function") {
    let frame = 0;
    return () => {
      frame += 1;
      return frame;
    };
  }

  return () => {
    try {
      return Number(exportedMain()) || 0;
    } catch {
      return 0;
    }
  };
}

async function run() {
  const wasm = await instantiateCubeModule();
  const exportedMain = wasm?.instance?.exports?.main;
  const nextTick = createTicker(exportedMain);

  statusNode.textContent =
    wasm === null
      ? "Host running without wasm output (compile cube.prime to cube.wasm)."
      : "Host running with cube.wasm bootstrap.";

  const frame = () => {
    drawWireCubeProxy(nextTick());
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}

run();
