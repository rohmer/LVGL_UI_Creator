/* Local Figma development plugin. No network requests or document mutations. */
"use strict";

figma.showUI(__html__, { width: 440, height: 500, themeColors: true });

const MAX_NODES = 10000;
const MAX_ASSET_BYTES = 24 * 1024 * 1024;
const MAX_BUNDLE_BYTES = 48 * 1024 * 1024;
const MAX_RASTER_EDGE = 4096;
const MAX_RASTER_PIXELS = 16 * 1024 * 1024;
const SCREEN_TYPES = new Set(["FRAME", "COMPONENT", "COMPONENT_SET", "INSTANCE", "GROUP", "SECTION"]);
const NATIVE_TYPES = new Set(["FRAME", "COMPONENT", "COMPONENT_SET", "INSTANCE", "GROUP", "SECTION", "RECTANGLE", "ELLIPSE", "TEXT", "LINE"]);
let busy = false;
let cancelled = false;

function post(type, fields) { figma.ui.postMessage(Object.assign({ type: type }, fields || {})); }
function checkCancelled() { if (cancelled) throw new Error("Export cancelled."); }
function visible(items) { return Array.isArray(items) ? items.filter(item => item.visible !== false) : []; }
function rounded(value) { return Math.round(value * 1000) / 1000; }

function selectionRoots() {
  const nodes = figma.currentPage.selection.filter(node => SCREEN_TYPES.has(node.type));
  const ids = new Set(nodes.map(node => node.id));
  return nodes.filter(node => {
    for (let parent = node.parent; parent && parent.type !== "PAGE"; parent = parent.parent)
      if (ids.has(parent.id)) return false;
    return true;
  });
}

function pageFrames() {
  const result = [];
  function visit(node) {
    if (node.type === "SECTION") { node.children.forEach(visit); return; }
    if (SCREEN_TYPES.has(node.type)) result.push(node);
  }
  figma.currentPage.children.forEach(visit);
  return result;
}

function updateSelection() {
  post("selection", {
    names: selectionRoots().map(node => node.name),
    pageCount: pageFrames().length,
    pageName: figma.currentPage.name,
    busy: busy
  });
}

function rasterReasons(node) {
  const reasons = [];
  if (!NATIVE_TYPES.has(node.type)) reasons.push(node.type.toLowerCase());
  const fills = visible(node.fills);
  const strokes = visible(node.strokes);
  if (fills.some(paint => paint.type !== "SOLID")) reasons.push("image or gradient fill");
  if (typeof node.fills === "symbol") reasons.push("mixed fills");
  if (fills.length > 1) reasons.push("multiple fills");
  if (strokes.some(paint => paint.type !== "SOLID") || strokes.length > 1) reasons.push("complex stroke");
  if (visible(node.effects).length) reasons.push("effects");
  if (typeof node.rotation === "number" && Math.abs(node.rotation % 360) > 0.01) reasons.push("rotation");
  if (node.blendMode && node.blendMode !== "PASS_THROUGH" && node.blendMode !== "NORMAL") reasons.push("blend mode");
  if (node.children && node.children.some(child => child.isMask && child.visible !== false)) reasons.push("masked group");
  if (node.type === "ELLIPSE" && node.arcData &&
      (node.arcData.innerRadius > 0 || Math.abs(node.arcData.endingAngle - node.arcData.startingAngle - Math.PI * 2) > 0.01))
    reasons.push("partial ellipse");
  if (node.type === "ELLIPSE" && Math.abs(node.width - node.height) > 0.01) reasons.push("non-circular ellipse");
  if (node.type === "TEXT") {
    if (typeof node.fontName === "symbol" || typeof node.fontSize === "symbol" ||
        typeof node.textCase === "symbol" || typeof node.textDecoration === "symbol") reasons.push("mixed text styles");
    if (node.textDecoration && node.textDecoration !== "NONE" && typeof node.textDecoration !== "symbol") reasons.push("text decoration");
  }
  if (Array.isArray(node.dashPattern) && node.dashPattern.length) reasons.push("dashed stroke");
  if (node.type === "RECTANGLE" &&
      [node.topLeftRadius, node.topRightRadius, node.bottomRightRadius, node.bottomLeftRadius]
        .filter(value => typeof value === "number").some((value, _, values) => value !== values[0]))
    reasons.push("unequal corner radii");
  return reasons;
}

function base64(bytes) {
  const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const chunks = [];
  let chunk = "";
  for (let i = 0; i < bytes.length; i += 3) {
    const a = bytes[i], hasB = i + 1 < bytes.length, hasC = i + 2 < bytes.length;
    const b = hasB ? bytes[i + 1] : 0, c = hasC ? bytes[i + 2] : 0;
    chunk += alphabet[a >> 2] + alphabet[((a & 3) << 4) | (b >> 4)] +
      (hasB ? alphabet[((b & 15) << 2) | (c >> 6)] : "=") + (hasC ? alphabet[c & 63] : "=");
    if (chunk.length >= 16384) { chunks.push(chunk); chunk = ""; }
  }
  chunks.push(chunk);
  return chunks.join("");
}

function documentFromResponse(response, nodeId) {
  if (response && response.document) return response.document;
  if (response && response.nodes && response.nodes[nodeId]) return response.nodes[nodeId].document;
  throw new Error("Figma returned no design data for this frame.");
}

function utf8Length(text) {
  let bytes = 0;
  for (let i = 0; i < text.length; i++) {
    const point = text.charCodeAt(i);
    if (point < 0x80) bytes++;
    else if (point < 0x800) bytes += 2;
    else if (point >= 0xd800 && point <= 0xdbff && i + 1 < text.length &&
             text.charCodeAt(i + 1) >= 0xdc00 && text.charCodeAt(i + 1) <= 0xdfff) { bytes += 4; i++; }
    else bytes += 3;
  }
  return bytes;
}

async function exportDesign(message) {
  if (busy) return;
  busy = true;
  cancelled = false;
  post("busy", { busy: true });
  try {
    const includeHidden = message.includeHidden === true;
    const roots = (message.scope === "page" ? pageFrames() : selectionRoots())
      .filter(node => includeHidden || node.visible !== false);
    if (!roots.length) throw new Error("Select one or more frames, components, instances, or groups first.");
    const assets = {};
    const warnings = [];
    let nodeCount = 0;
    let assetBytes = 0;
    let assetCount = 0;
    function inspect(node) {
      if (!includeHidden && node.visible === false) return;
      if (++nodeCount > MAX_NODES) throw new Error("Export exceeds 10,000 layers. Select fewer frames.");
      if (node.children) node.children.forEach(inspect);
    }
    roots.forEach(inspect);

    async function collectAssets(node) {
      checkCancelled();
      if (!includeHidden && node.visible === false) return;
      const reasons = rasterReasons(node);
      if (reasons.length) {
        const bounds = node.absoluteBoundingBox;
        if (!bounds || bounds.width <= 0 || bounds.height <= 0) {
          warnings.push(node.name + ": no visible bounds for image fallback.");
        } else {
          if (![bounds.x, bounds.y, bounds.width, bounds.height].every(Number.isFinite))
            throw new Error(node.name + ": invalid image bounds.");
          const rasterScale = Math.min(1, MAX_RASTER_EDGE / Math.max(bounds.width, bounds.height),
            Math.sqrt(MAX_RASTER_PIXELS / (bounds.width * bounds.height)));
          if (rasterScale < 0.01) throw new Error(node.name + ": layer is too large to render. Reduce its size in Figma.");
          post("progress", { message: "Rendering " + node.name + "…" });
          // Absolute bounds preserve the same origin and full extent used by the importer.
          const bytes = await node.exportAsync({
            format: "PNG", constraint: { type: "SCALE", value: rasterScale },
            contentsOnly: true, useAbsoluteBounds: true, colorProfile: "SRGB"
          });
          checkCancelled();
          assetBytes += bytes.length;
          if (assetBytes > MAX_ASSET_BYTES) throw new Error("Images exceed the 24 MiB export limit. Select fewer frames.");
          const imageName = (node.name || "Layer").replace(/[\\/:*?"<>|\u0000-\u001f]/g, "_").slice(0, 100) + ".png";
          assets[node.id] = {
            name: imageName, mimeType: "image/png", data: base64(bytes),
            bounds: { x: rounded(bounds.x), y: rounded(bounds.y), width: rounded(bounds.width), height: rounded(bounds.height) },
            scale: rasterScale, reason: reasons.join(", ")
          };
          assetCount++;
          if (rasterScale < 1) warnings.push(node.name + ": image resolution reduced to fit the export limit.");
          return; // The raster includes descendants; exporting them again would duplicate visuals.
        }
      }
      if (node.children) for (const child of node.children) await collectAssets(child);
    }

    const documents = [];
    for (let index = 0; index < roots.length; index++) {
      checkCancelled();
      const root = roots[index];
      post("progress", { message: "Reading frame " + (index + 1) + " of " + roots.length + ": " + root.name });
      const response = await root.exportAsync({ format: "JSON_REST_V1" });
      checkCancelled();
      documents.push(documentFromResponse(response, root.id));
      await collectAssets(root);
    }
    const bundle = {
      uicreatorFigmaVersion: 1,
      name: figma.root.name,
      document: { id: "uicreator-document", name: figma.root.name, type: "DOCUMENT", children: [
        { id: figma.currentPage.id, name: figma.currentPage.name, type: "CANVAS", children: documents }
      ] },
      uicreatorAssets: assets,
      uicreatorExportWarnings: warnings
    };
    const text = JSON.stringify(bundle);
    if (utf8Length(text) > MAX_BUNDLE_BYTES) throw new Error("Export is too large. Select fewer frames.");
    const name = (figma.root.name || "Figma design").replace(/[\\/:*?"<>|\u0000-\u001f]/g, "_").slice(0, 100);
    post("complete", { filename: name + ".figma.json", text: text, frames: roots.length, assets: assetCount, warnings: warnings });
  } catch (error) {
    post("error", { message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy = false;
    post("busy", { busy: false });
    updateSelection();
  }
}

figma.ui.onmessage = message => {
  if (!message || typeof message.type !== "string") return;
  if (message.type === "export") exportDesign(message);
  else if (message.type === "cancel") cancelled = true;
  else if (message.type === "refresh") updateSelection();
  else if (message.type === "close") { cancelled = true; figma.closePlugin(); }
};
figma.on("selectionchange", updateSelection);
figma.on("currentpagechange", updateSelection);
updateSelection();
