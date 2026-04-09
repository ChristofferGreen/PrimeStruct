const DefaultStatusMessages = Object.freeze({
  wasmLoadSkippedPrefix: "Wasm load skipped: ",
  shaderLoadSkippedPrefix: "Shader load skipped: ",
  hostWithShaderAndWasm: "Host running with cube.wasm and cube.wgsl bootstrap.",
  hostWithShaderOnly: "Host running with shader path; compile cube.prime to cube.wasm.",
  hostWithoutWasm: "Host running without wasm output (compile cube.prime to cube.wasm).",
  hostWithWasmOnly: "Host running with cube.wasm bootstrap.",
});

function requireCanvas(selector, missingDomMessage) {
  const node = document.querySelector(selector);
  if (!(node instanceof HTMLCanvasElement)) {
    throw new Error(missingDomMessage);
  }
  return node;
}

function requireStatusNode(selector, missingDomMessage) {
  const node = document.querySelector(selector);
  if (node === null) {
    throw new Error(missingDomMessage);
  }
  return node;
}

async function instantiateWasmModule(wasmUrl, statusNode, statusMessages) {
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
    statusNode.textContent = `${statusMessages.wasmLoadSkippedPrefix}${String(error)}`;
    return null;
  }
}

async function loadShaderText(shaderUrl, statusNode, statusMessages) {
  try {
    const response = await fetch(shaderUrl);
    if (!response.ok) {
      throw new Error(`Failed to fetch cube.wgsl (${response.status})`);
    }
    return await response.text();
  } catch (error) {
    statusNode.textContent = `${statusMessages.shaderLoadSkippedPrefix}${String(error)}`;
    return "";
  }
}

function drawWireCubeProxy(canvas, ctx, tick) {
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

export async function launchBrowserRuntime({
  canvasSelector,
  statusSelector,
  wasmUrl,
  shaderUrl,
  missingDomMessage,
  canvasContextUnavailableMessage = "2D canvas context is unavailable",
  statusMessages = DefaultStatusMessages,
}) {
  const canvas = requireCanvas(canvasSelector, missingDomMessage);
  const statusNode = requireStatusNode(statusSelector, missingDomMessage);
  const ctx = canvas.getContext("2d");
  if (ctx === null) {
    throw new Error(canvasContextUnavailableMessage);
  }
  const [wasm, shaderText] = await Promise.all([
    instantiateWasmModule(wasmUrl, statusNode, statusMessages),
    loadShaderText(shaderUrl, statusNode, statusMessages),
  ]);
  const exportedMain = wasm?.instance?.exports?.main;
  const nextTick = createTicker(exportedMain);
  const hasShader = shaderText.includes("@vertex") && shaderText.includes("@fragment");

  statusNode.textContent =
    wasm === null
      ? hasShader
          ? statusMessages.hostWithShaderOnly
          : statusMessages.hostWithoutWasm
      : hasShader
          ? statusMessages.hostWithShaderAndWasm
          : statusMessages.hostWithWasmOnly;

  const frame = () => {
    drawWireCubeProxy(canvas, ctx, nextTick());
    requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
}
