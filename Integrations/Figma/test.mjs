import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const code = fs.readFileSync(new URL('./code.js', import.meta.url), 'utf8');
const html = fs.readFileSync(new URL('./ui.html', import.meta.url), 'utf8');
new vm.Script(html.match(/<script>([\s\S]*?)<\/script>/)[1], {filename: 'ui.html'});
const manifest = JSON.parse(fs.readFileSync(new URL('./manifest.json', import.meta.url), 'utf8'));
assert.deepEqual(manifest.networkAccess.allowedDomains, ['none']);
assert.equal(manifest.documentAccess, 'dynamic-page');
assert.equal(manifest.main, 'code.js');
assert.equal(manifest.ui, 'ui.html');

let nextId = 0;
function layer(type, name, children = [], extra = {}) {
  return Object.assign({
    id: `2:${++nextId}`, type, name, children, visible: true,
    width: 100, height: 100, rotation: 0, blendMode: 'NORMAL',
    fills: [{type: 'SOLID', color: {r: .1, g: .2, b: .3}}], strokes: [], effects: [],
    absoluteBoundingBox: {x: 10, y: 20, width: 100, height: 100}
  }, extra);
}

function boot(roots, selected, behavior = () => undefined) {
  const messages = [], exports = [], events = {};
  const page = {id: '0:1', name: 'Screens', type: 'PAGE', children: roots, selection: selected};
  function rest(node) {
    return {id: node.id, name: node.name, type: node.type, absoluteBoundingBox: node.absoluteBoundingBox,
      children: node.children.map(rest)};
  }
  function setup(node, parent) {
    node.parent = parent;
    node.exportAsync = async settings => {
      exports.push({id: node.id, settings});
      const custom = await behavior(node, settings, context);
      if (custom !== undefined) return custom;
      return settings.format === 'JSON_REST_V1' ? {document: rest(node)} : new Uint8Array([0, 127, 128, 255]);
    };
    node.children.forEach(child => setup(child, node));
  }
  const figma = {
    root: {name: 'Dashboard 🖥️'}, currentPage: page,
    showUI: () => {}, closePlugin: () => {},
    ui: {postMessage: message => messages.push(message)},
    on: (name, callback) => { events[name] = callback; }
  };
  const context = vm.createContext({figma, __html__: '<html></html>', Uint8Array});
  roots.forEach(node => setup(node, page));
  vm.runInContext(code, context, {filename: 'code.js'});
  return {context, figma, messages, exports, events,
    run: options => context.exportDesign(Object.assign({scope: 'selection'}, options)),
    complete: () => messages.find(message => message.type === 'complete'),
    error: () => messages.find(message => message.type === 'error')};
}

// A complex child leaves its parent and editable siblings native.
const label = layer('TEXT', 'Caption');
const vector = layer('VECTOR', 'Logo');
const image = layer('RECTANGLE', 'Photo', [], {fills: [{type: 'IMAGE', imageHash: 'image-ref'}]});
const mask = layer('GROUP', 'Mask group', [layer('RECTANGLE', 'Mask', [], {isMask: true}), layer('TEXT', 'Masked text')]);
const nested = layer('FRAME', 'Nested', [label, vector, image, mask]);
const frame = layer('FRAME', 'Dashboard', [nested]);
const basic = boot([frame], [frame, nested]);
await basic.run();
assert.equal(basic.complete().frames, 1, 'nested selections are not duplicated');
const bundle = JSON.parse(basic.complete().text);
assert.equal(bundle.uicreatorFigmaVersion, 1);
assert.equal(bundle.document.children[0].children.length, 1);
assert.equal(bundle.document.children[0].children[0].children[0].children[0].name, 'Caption');
assert.deepEqual(Object.keys(bundle.uicreatorAssets).sort(), [vector.id, image.id, mask.id].sort());
assert.equal(bundle.uicreatorAssets[vector.id].data, Buffer.from([0, 127, 128, 255]).toString('base64'));
assert.deepEqual(bundle.uicreatorAssets[vector.id].bounds, vector.absoluteBoundingBox);
assert.equal(basic.exports.filter(call => call.settings.format === 'PNG').length, 3);
assert.ok(basic.exports.filter(call => call.settings.format === 'PNG').every(call => call.settings.useAbsoluteBounds === true));
assert.equal(basic.messages.at(-2).busy, false);

const hidden = layer('VECTOR', 'Hidden asset', [], {visible: false});
const hiddenFrame = layer('FRAME', 'Hidden test', [hidden]);
const hiddenDefault = boot([hiddenFrame], [hiddenFrame]);
await hiddenDefault.run();
assert.equal(hiddenDefault.complete().assets, 0);
const hiddenIncluded = boot([hiddenFrame], [hiddenFrame]);
await hiddenIncluded.run({includeHidden: true});
assert.equal(hiddenIncluded.complete().assets, 1);

const pageFrame = layer('FRAME', 'Inside section');
const pageGroup = layer('GROUP', 'Top-level group');
const pageExport = boot([layer('SECTION', 'Section', [pageFrame]), pageGroup, layer('RECTANGLE', 'Loose shape')], []);
await pageExport.run({scope: 'page'});
assert.equal(pageExport.complete().frames, 2);

const mixed = layer('TEXT', 'Mixed font', [], {fontName: Symbol('mixed')});
const oval = layer('ELLIPSE', 'Oval', [], {width: 80, height: 30});
const circle = layer('ELLIPSE', 'Circle');
const detailFrame = layer('FRAME', 'Details', [mixed, oval, circle]);
const details = boot([detailFrame], [detailFrame]);
await details.run();
assert.deepEqual(Object.keys(JSON.parse(details.complete().text).uicreatorAssets).sort(), [mixed.id, oval.id].sort());

const largeVector = layer('VECTOR', 'Large', [], {absoluteBoundingBox: {x: 10, y: 20, width: 8192, height: 100}});
const largeFrame = layer('FRAME', 'Large screen', [largeVector]);
const scaled = boot([largeFrame], [largeFrame]);
await scaled.run();
assert.equal(scaled.exports.find(call => call.settings.format === 'PNG').settings.constraint.value, .5);
assert.equal(scaled.complete().warnings.length, 1);

const empty = boot([], []);
await empty.run();
assert.match(empty.error().message, /Select one or more/);
assert.equal(empty.complete(), undefined);

const malformedFrame = layer('FRAME', 'Bad response');
const malformed = boot([malformedFrame], [malformedFrame], (_, settings) => settings.format === 'JSON_REST_V1' ? {} : undefined);
await malformed.run();
assert.match(malformed.error().message, /no design data/);
assert.equal(malformed.complete(), undefined);

const cancelledFrame = layer('FRAME', 'Cancel test');
const cancellation = boot([cancelledFrame], [cancelledFrame], (_, settings, context) => {
  if (settings.format === 'JSON_REST_V1') context.figma.ui.onmessage({type: 'cancel'});
});
await cancellation.run();
assert.match(cancellation.error().message, /cancelled/);
assert.equal(cancellation.complete(), undefined);

const excessive = layer('FRAME', 'Many layers', Array.from({length: 10000}, () => layer('RECTANGLE', 'Rectangle')));
const limited = boot([excessive], [excessive]);
await limited.run();
assert.match(limited.error().message, /10,000 layers/);
assert.equal(limited.exports.length, 0, 'limits are checked before rendering');

const failingVector = layer('VECTOR', 'Broken image');
const failingFrame = layer('FRAME', 'Failure test', [failingVector]);
const failure = boot([failingFrame], [failingFrame], (_, settings) => {
  if (settings.format === 'PNG') throw new Error('Figma renderer failed');
});
await failure.run();
assert.match(failure.error().message, /Figma renderer failed/);
assert.equal(failure.complete(), undefined);

console.log('Figma local plugin: manifest, native/raster bundle, selection, hidden/page export, scaling, cancellation, limits, and failures passed.');
