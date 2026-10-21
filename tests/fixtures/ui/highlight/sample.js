// fetch data
import { load } from "./load.js";

@observe
class Store {
  async fetchAll(items) {
    const url = `https://x.y/${items}`;
    if (items.length > 3) { return null; }
    return load(url, 'ok', 42);
  }
}
