import {untar} from './tinytar/untar.js'
import {makeIconSummary, findTypeAndCreator} from './makeIconTitle.js'

export class IconLoader {
  constructor(baseDirectory) {
    this.baseDirectory = baseDirectory.replace(/\/$/, "");
    this.#index = null;
    this.#sources = null;
    this.#totalIcons = 0;
    this.#ready = this.#initialize();
  }

  #index;
  #sources;
  #totalIcons;
  #ready;

  #initialize() {
    return Promise.all([
      this.#loadIndex(),
      this.#loadSources()
    ]);
  }

  async #loadIndex() {
    if (this.#index) {
      return;
    }

    const response = await fetch(`${this.baseDirectory}/index.csv`);
    if (!response.ok) {
        throw new Error("Failed to load index.csv");
    }
    const text = await response.text();

    this.#index = [];

    let runningStart = 0;

    for (const line of text.split(/\r?\n/)) {
      if (!line.trim()) {
        continue;
      }

      const [prefix, countStr] = line.split(",");
      const count = Number(countStr);
      if (!Number.isInteger(count)) {
        throw new Error("Failed to parse index.csv");
      }

      this.#index.push({
        prefix,
        count,
        start: runningStart
      });

      runningStart += count;
    }

    this.#totalIcons = runningStart;
    if (this.onload) {
      this.onload(this.#totalIcons);
    }
  }

  async #loadSources() {
    if (this.#sources) {
      return;
    }
    const response = await fetch(`${this.baseDirectory}/sources.json`);
    if (!response.ok) {
        throw new Error("Failed to load sources.json");
    }
    this.#sources = await response.json();
  }

  #findArchive(globalIndex) {
    if (globalIndex < 0 || globalIndex >= this.#totalIcons) {
      throw new RangeError("Icon index out of range");
    }

    // Binary search
    let lo = 0;
    let hi = this.#index.length - 1;

    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      const entry = this.#index[mid];

      if (globalIndex < entry.start) {
        hi = mid - 1;
      }
      else if (globalIndex >= entry.start + entry.count) {
        lo = mid + 1;
      }
      else {
        return {
          archiveIndex:   mid,
          localIndex: globalIndex - entry.start
        };
      }
    }
    throw new Error("Index lookup failed");
  }

  async #fetchArchive(url) {
    console.log("Fetching", url);
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Failed to fetch ${url}`);
    }
    const zipStream = response.body.pipeThrough(new DecompressionStream("gzip"));
    const tarArchive = new Uint8Array(await new Response(zipStream).arrayBuffer());
    return untar(tarArchive, {checkHeader: false});
  }

  // If using non-deflated PNG images, this function might help reduce
  // memory usage by making the dataUrls smaller, but it does not
  // make a difference in the current deflated PNG I am using.
  async crunchImage(dataUrl) {
    if (!this.canvas) {
      this.canvas = document.createElement("canvas");
      this.canvas.width = 32;
      this.canvas.height = 32;
    }

    const ctx = this.canvas.getContext("bitmaprenderer");
    const img = new Image();

    await new Promise((resolve, reject) => {
      img.onload = resolve;
      img.onerror = () => reject(new Error("Failed to load image."));
      img.src = dataUrl;
    });

    const bitmap = await createImageBitmap(img);
    ctx.transferFromImageBitmap(bitmap);
    bitmap.close();

    const newDataURL = this.canvas.toDataURL("image/png");
    const sizeRatio = newDataURL.length / dataUrl.length;

    return sizeRatio < 1 ? newDataURL : dataUrl;
  }

  async #loadArchiveByIndexUncached(archiveIndex) {
    const prefix = this.#index[archiveIndex].prefix;
    const archiveUrl = `${this.baseDirectory}/${prefix}.tgz`;

    const iconFiles = [];
    const csvFiles = new Map();
    const files = await this.#fetchArchive(archiveUrl);

    // First separate out the .png and the .csv files
    files.forEach(file => {
      // For some odd reason, file names are coming back padded with zero bytes, hence the slice
      const zeroIdx = file.name.indexOf('\0');
      if (zeroIdx != -1) {
        file.name = file.name.slice(0, zeroIdx);
      }
      // Now triage files based on their type
      const extension = file.name.split('.').pop();
      if(file.name.toLowerCase().endsWith(".csv")) {
        csvFiles.set(file.name, file.data);
      }
      else if(file.name.toLowerCase().endsWith(".png")) {
        iconFiles.push(file);
      }
    });

    // Now convert the files into an icon record
    const processedIcons = await Promise.all(iconFiles.map(async icon => {
      const name = icon.name;
      const csvName = name.replace(/png$/i,"csv");
      const typeAndCreator = findTypeAndCreator(icon.data);
      const summary = makeIconSummary(icon.data, csvFiles.get(csvName), this.#sources);
      const base64data = icon.data.toBase64();
      //const dataUrl = await this.crunchImage(`data:image/png;base64,${base64data}`);
      const dataUrl = `data:image/png;base64,${base64data}`;
      return {name, dataUrl, summary, typeAndCreator};
    }));

    if (processedIcons.length != this.#index[archiveIndex].count) {
      console.log(`Count mismatch for "${prefix}" (`, this.#index[archiveIndex].count, "!=", processedIcons.length, ")");
    }
    return processedIcons;
  }

  async #loadArchiveByIndex(archiveIndex) {
    const entry = this.#index[archiveIndex];
    if (entry.cachedPromise) {
      return entry.cachedPromise;
    }
    entry.cachedPromise = this.#loadArchiveByIndexUncached(archiveIndex)
      .catch(err => {
        delete entry.cachedPromise;
        throw err;
      });
    return entry.cachedPromise;
  }

  async *loadIcons(startIndex, numberOfIcons, signal) {
    await this.#ready;
    let emitted = 0;

    if (startIndex >= this.#totalIcons) return [];

    let {archiveIndex, localIndex} = this.#findArchive(startIndex);

    while ((emitted < numberOfIcons) && (archiveIndex < this.#index.length)) {
      const icons = await this.#loadArchiveByIndex (archiveIndex);
      signal.throwIfAborted();
      // Prefetch, to improve performance
      if (archiveIndex + 1 < this.#index.length) {
        this.#loadArchiveByIndex(archiveIndex + 1);
      }
      // Now return the requested icons
      while ((emitted < numberOfIcons) && (localIndex < icons.length)) {
        yield icons[localIndex];
        localIndex++;
        emitted++;
      }
      archiveIndex++;
      localIndex = 0;
    }
  }

  async getTotalIcons() {
    await this.#ready;
    return this.#totalIcons;
  }
}