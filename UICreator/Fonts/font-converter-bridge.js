'use strict';
const fs = require('fs');
const path = require('path');
const { parse_args } = require('./lib/parse_args');
const convert = require('./lib/convert');
(async () => {
  const job = path.dirname(process.argv[2]);
  const request = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
  const args = parse_args(request.args, false, fs, path);
  const files = await convert(args);
  // The preview is uncompressed and unpadded so LVSerial can embed its bitmap.
  // Export compression/alignment settings remain independent of this preview.
  const previewArgs = parse_args(request.previewArgs, false, fs, path);
  const preview = await convert(previewArgs);
  fs.writeFileSync(path.join(job, 'preview.bin'), Object.values(preview)[0]);
  const manifest = [];
  for (const [destination, data] of Object.entries(files)) {
    const staged = 'output-' + manifest.length;
    fs.writeFileSync(path.join(job, staged), data);
    manifest.push({ destination, staged });
  }
  fs.writeFileSync(path.join(job, 'result.json'), JSON.stringify({ files: manifest }));
})().catch(error => { console.error(error.stack || error.message || String(error)); process.exitCode = 1; });
