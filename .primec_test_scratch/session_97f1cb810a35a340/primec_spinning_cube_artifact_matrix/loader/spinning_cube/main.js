import { launchBrowserRuntime } from "../shared/browser_runtime_shared.js";

void launchBrowserRuntime({
  canvasSelector: "#cube-canvas",
  statusSelector: "#status",
  wasmUrl: new URL("./cube.wasm", import.meta.url),
  shaderUrl: new URL("./cube.wgsl", import.meta.url),
  missingDomMessage: "Missing spinning_cube host DOM nodes",
});
