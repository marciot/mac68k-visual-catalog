function asciiStringCompare(buffer, offset, asciiString) {
  for (let i = 0; i < asciiString.length; i++) {
    if (buffer[offset + i] !== asciiString.charCodeAt(i)) return false;
  }
  return true;
}

function bufferToString(buffer, offset, length, encoding = "utf-8") {
  let result = "";
  const slice = buffer.slice(offset, offset + length);
  const decoder = new TextDecoder(encoding, { fatal: false });
  return decoder.decode(slice);
}

// Our encoder will store the type and creator for the Macintosh
// file containing the icon as a iTXt chunk in the PNG. This
// function extracts that information, but is not a general PNG
// parser and may not work if the PNG was modified.
function findTypeAndCreator(byteArray) {

  let offset = 0, chunkAt, chunkLen;

  function hasSignature() {
    if (byteArray[0] == 137 &&
        byteArray[1] == 80  &&
        byteArray[2] == 78  &&
        byteArray[3] == 71  &&
        byteArray[4] == 13  &&
        byteArray[5] == 10  &&
        byteArray[6] == 26  &&
        byteArray[7] == 10) {
      offset = 8;
      return true;
    }
    return false;
  }

  function hasChunk(chunkType) {
    const len = (byteArray[offset + 0] << 24)
              + (byteArray[offset + 1] << 16)
              + (byteArray[offset + 2] << 8)
              + (byteArray[offset + 3]);
    offset += 4;     // Skip chunk length
    if (asciiStringCompare(byteArray, offset, chunkType)) {
      offset += 4;   // Skip chunk type
      chunkAt  = offset;
      chunkLen = len;
      offset += len; // Skip chunk data
      offset += 4;   // Skip chunk checksum
      return true;
    }
    return false;
  }

  if (hasSignature())
  if (hasChunk('IHDR'))
  if (hasChunk('tRNS'))
  if (hasChunk('iTXt'))
  if (asciiStringCompare(byteArray, chunkAt, 'MacOS Info')) {
    // https://libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.iTXt
    const offset = 15;
    return bufferToString (byteArray, chunkAt + 15, chunkLen - 15);
  }
}

function summarizeApps(csvRecords, appMap) {

  // Split a name into normalized words
  function words(name) {
    return name.trim().split(/\s+/);
  }

  function capitalizeWord(word) {
    if (!word) return word;
    return word[0].toUpperCase() + word.slice(1).toLowerCase();
  }

  function titleCase(name) {
    return words(name).map(capitalizeWord).join(" ");
  }

  // Returns true if b starts with all words from a.
  function isWordPrefix(a, b) {
    const aw = words(a.toLowerCase());
    const bw = words(b.toLowerCase());

    if (aw.length > bw.length) return false;

    for (let i = 0; i < aw.length; i++) {
      if (aw[i] !== bw[i]) return false;
    }

    return true;
  }

  // Maps lowercase canonical key -> canonical name
  const canonicalNames = new Map();

  const records = [];

  for (const line of csvRecords) {
    const location = appMap[line.media];
    const rawName = line.name;

    const key = rawName.toLowerCase();

    if (!canonicalNames.has(key)) {
      canonicalNames.set(key, rawName);
    } else {
      const existing = canonicalNames.get(key);

      const existingScore = existing === titleCase(existing) ? 1 : 0;
      const newScore =      rawName  === titleCase(rawName)  ? 1 : 0;

      if (newScore > existingScore) {
        canonicalNames.set(key, rawName);
      }
    }

    records.push({
      location,
      originalName: rawName
    });
  }

  // Merge names with common word prefixes.
  let names = [...new Set([...canonicalNames.values()])];

  names.sort((a, b) => words(a).length - words(b).length);

  const alias = new Map();

  for (let i = 0; i < names.length; i++) {
    const base = names[i];

    for (let j = i + 1; j < names.length; j++) {
      const other = names[j];

      if (isWordPrefix(base, other)) {
        alias.set(other.toLowerCase(), base);
      }
    }
  }

  function canonicalize(name) {
    const preferred = canonicalNames.get(name.toLowerCase()) ?? name;
    return alias.get(preferred.toLowerCase()) ?? preferred;
  }

  // Build both maps.
  const byLocation = new Map();
  const byName = new Map();

  const seenPairs = new Set();

  for (const r of records) {
    const name = canonicalize(r.originalName);

    const pair = `${r.location}\u0000${name}`;
    if (seenPairs.has(pair)) continue;
    seenPairs.add(pair);

    if (!byLocation.has(r.location)) {
      byLocation.set(r.location, new Set());
    }

    byLocation.get(r.location).add(name);

    if (!byName.has(name)) {
      byName.set(name, new Set());
    }

    byName.get(name).add(r.location);
  }

  function format(map) {
    let out = "";

    for (const [key, values] of map) {
      out += key + ":\n";

      for (const value of [...values].sort()) {
        out += "  " + value + "\n";
      }
    }

    return out.trimEnd();
  }

  const locationOutput = format(byLocation);
  const nameOutput = format(byName);

  return locationOutput.split("\n").length < nameOutput.split("\n").length
    ? locationOutput
    : nameOutput;
}

function parseAppCsv(buffer, sourceMap) {
  const results = [];
  let start = 0;

  while (start < buffer.length) {
    // Find end of line
    let end = start;
    while (
      end < buffer.length &&
      buffer[end] !== 0x0A && // LF
      buffer[end] !== 0x0D    // CR
    ) {
      end++;
    }

    // Skip empty lines
    if (end > start) {
      const line = buffer.subarray(start, end);

      // Find comma
      const comma = line.indexOf(0x2C); // ','

      if (comma !== -1) {
        const media = Number(
          bufferToString(line, 0, comma, "ascii")
        );

        const encoding = (sourceMap[media].toLowerCase().includes("japan")) ? "shift_jis" : "macintosh";
        const name = bufferToString(
          line,
          comma + 1,
          line.length - comma - 1,
          encoding
        );
        results.push({ media, name });
      }
    }

    // Skip CR/LF combinations or individual CR/LF
    if (buffer[end] === 0x0D) end++;
    if (buffer[end] === 0x0A) end++;

    start = end;
  }

  return results;
}

export function makeIconTitle(iconData, iconCsv, sourceMap) {
  const csvRecords = parseAppCsv(iconCsv, sourceMap);
  const summary = summarizeApps(csvRecords, sourceMap);
  const typeAndCreator = findTypeAndCreator(iconData);
  return `${summary}\n\nFile Type: ${typeAndCreator}\n` ;
}