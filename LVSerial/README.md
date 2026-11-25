# LVSerial persistence

LVSerial owns persistence for widgets, logical screens/canvases, typed variables, and complete projects. Existing widget, style, font, and image serializers produce their JSON records. `ObjectSerializer`, `Screen`, `LVVariable`, `Variable<T>`, and `Project` expose JSON and Protobuf entry points. The editor uses `Project::ToJSON`/`FromJSON` for load/save and uses the same model for binary export.

`Protobuf::Encode(EntityKind, type, jsonRecord)` and `Decode` also support standalone styles, fonts, assets, and other serialized records. The binary format contains typed Protobuf properties, not a JSON document stored in a bytes field. Scalar types, UTF-8 strings, binary buffers, empty containers, and nested arrays/objects survive round-trips.

The authoritative schema is [Proto/lvserial.proto](Proto/lvserial.proto). Checked-in generated C files use nanopb 0.4.9.2. Normal builds require neither protoc nor Python. To regenerate, compile a descriptor with protoc using the schema and `3rdParty/nanopb/generator/proto` include paths, then run `nanopb_generator.py` on that descriptor. The generator needs a matching Python protobuf runtime.

Raster fonts have an explicit `FONT_DATA` property kind and `FontData`
submessage, with typed metrics, glyphs, Unicode maps and kerning. Bitmap and
class/kerning bytes are stored directly, without base64 or a property path for
each glyph metric. The streaming C visitor receives the bounded font submessage
and can decode it with `lvserial_FontData_fields` without a heap or LVGL. The
C++ reader reconstructs the existing portable font model, which the runtime
loads once and shares among widgets by project font ID. Legacy generic font
records remain readable; passing `omitDefaults=false` keeps the legacy wire
representation. Neither path links desktop font converters into firmware.

The current Protobuf record format version is 2; version 1 files remain readable. Entity kind tags distinguish widgets, screens, variables, projects, styles, fonts, and assets. Property paths use JSON Pointer escaping, with container tokens emitted before their children. Limits are 511 bytes per property path, 63 bytes per type identifier, and 64 nested containers. Strings and image data are length-delimited rather than placed in fixed-size arrays.

Protobuf exports omit known default property names and values. Each affected object carries a small dictionary ID and presence mask instead, covering common widget, base, flag, scroll, style, font and variable defaults. This preserves the distinction between an absent property and an explicit default override. Empty arrays/objects, array positions, unknown properties, and nondefault false/zero/empty values retain their meaning. JSON saves are unchanged. `Protobuf::Encode(..., false)` writes the full version 1 representation when required for an older consumer or size comparison.

The version 2 wire representation reuses `Property.signed_value` for the dictionary ID and `Property.unsigned_value` for its 64-bit mask on OBJECT nodes only. `child_count` counts the explicit wire children. The authoritative immutable dictionaries are in `Device/lvserial_defaults.c`; existing entries must never be reordered or changed. The device C decoder restores default properties, clears these metadata fields and adjusts child counts before invoking the application's normal visitor. Firmware must update the device codec to read new exports; no LVGL, JSON or heap allocation is required to restore defaults.

## Plain C device library

Build `LVSerial/Device` as a standalone CMake project, or add it as a subdirectory in firmware. The `LVSerialDevice` target contains nanopb, the generated schema, and `lvserial_pb_encode`/`lvserial_pb_decode`. It needs no LVGL, C++, JSON library, or heap allocation.

Encoding enumerates `lvserial_property` values through a synchronous caller callback into a nanopb output stream. Decoding visits properties from a caller-owned input buffer. Text/binary payloads are exposed as bounded nanopb streams; the callback can consume them in chunks or write them directly to flash. Metadata and payload views are borrowed until the callback returns. Firmware maps entity kinds and property paths to its own objects or LVGL operations. This codec does not yet implement the application-specific on-device widget renderer. Direct C generation is available separately through the editor's **Export C project** command and the `UICreatorCodeGen` executable.

UICreator image records include original GIF/PNG/JPG bytes and image metadata, so both persistence formats are independent of the source file. The C exporter decodes these into compiled pixel/frame arrays; the Protobuf device application can choose its own resource strategy. The editor does not make that decision per image.

Project `fonts` entries contain stable `id`, unique `name`, complete bitmap `font`, and conversion metadata. Project styles use `{ "projectFont": "font_ID" }` references; `Project::FromJSON`/`FromProtobuf` resolves these before creating widgets. Missing or duplicate resources are rejected. Standalone widget serializers continue embedding their font for portability. Generated C stores a shared bitmap once and provides lookup by ID.

Converted image assets additionally retain `conversion` options, `previewFrames` (PNG), `delays`, and `deviceFrames` records containing `cf`, `w`, `h`, `stride`, `flags` and base64 `data`. Desktop previews use PNG frames; device rendering uses the exact device-frame records, which take precedence over the desktop image descriptor's preview format. Compression headers are included in frame data, and flags follow LVGL 9.6. The original `sourceData` is retained for future conversion without the original file.

## Validation

CTest covers every palette widget in JSON and Protobuf, default omission and smaller records for all widget types, all variable types and histories, screen/project persistence, pure-C default restoration and version 1 compatibility, malformed masks/input, and interoperability with the official protoc decoder when installed. The device codec can be built separately to verify that it has no desktop/editor dependency.

Project `images` is an array of `{id,name,image}` entries containing embedded ImageAsset records. Widget `base.imageId` records the selection, while `img.asset` is stored as `{projectImage:id}`. Project loading validates and expands references before restoring objects; standalone widget serialization remains self-contained. The same representation is preserved in Protobuf and shared images are emitted once by C generation.

Project `tries` is an array of `{id,name,format:"ngtr",data,indexData}` entries.
Authoring JSON uses base64; Protobuf writes model/index data as native binary
properties. Models may use fixed NGTR v2 with an empty index, compact NGTR v3
with NGIX v1, or compressed radix NGTR v4 with NGIX v2. V4 supports exact 64-bit
counts and optional trained conditional probabilities. Import validates
the complete structure and scored pruning bounds, generates a missing index and deduplicates identical
models. Names are display filenames; no external model file is required after
import. Advanced Keyboard stores a `trieId` reference. The reserved
`builtin-en-us` ID selects the bundled small English model without a catalog blob;
an empty ID disables its predictor.

`ProjectTries::Load` produces a shared immutable catalog, and
`ProjectTries::Scope` resolves references during widget restoration. Attached
widgets retain their shared data and own individual prediction workspaces until
deletion. Bulk catalog bytes are removed from per-screen metadata.
`Protobuf::DecodeBorrowedAssets` and the runtime's opt-in borrowed-assets loader
can retain trie payload spans inside a caller-owned immutable flash mapping.
Their input must outlive the runtime. Ordinary decoding/loading owns its bytes
and preserves the existing input-release contract. Other binary properties
continue decoding normally in either mode.
