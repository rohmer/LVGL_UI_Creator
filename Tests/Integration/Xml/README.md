# UI Creator XML tests

These tests exercise the application's owned `<uicreator format_version="1">`
format. They do not consume or validate LVGL Pro XML.

Configure with `BUILD_TESTING=ON` and `LVSERIAL_USE_XML=ON`, then build
`UICreatorXmlTests`, `UICreatorCodeGen` and `LVGLCompiler`:

```sh
cmake --build build --target UICreatorXmlTests UICreatorCodeGen LVGLCompiler --parallel
ctest --test-dir build -L xml --output-on-failure
```

The CTest group includes:

- `UICreatorXmlCodec`: every entity/JSON type, binary subtypes, signed and
  unsigned boundaries, exact string escaping/UTF-8, deterministic encoding,
  numeric validation, truncated documents and configurable parser limits.
- `UICreatorXmlRuntime`: actual LVGL rendering and state equivalence with
  Protobuf, including embedded fonts/images, barcode/QR widgets, custom trie
  predictions, Advanced Keyboard target switching, bindings, histories and
  interaction actions. Input buffers are overwritten and released after load.
- `UICreatorXmlLazy`: lazy construction, inactive screen unloading, retained
  widget state, variable updates and trie ownership. Protobuf is checked as a
  baseline; XML also exercises the borrowed-assets entry-point flag.
- `UICreatorXmlFilesystem`: XML loading through LVGL's memory filesystem,
  followed by release of the original file bytes and real rendering.
- `UICreatorXmlMalformed`: malformed envelopes and typed values, invalid
  project structure, size limits, entity/DTD rejection, bounded diagnostics
  and preservation of the existing active screen after failed loads.
- `UICreatorXmlCLI`: both exporters, independent ElementTree parsing,
  JSON/XML/Protobuf/C equivalence, Unicode paths and preservation of previous
  files when export fails.

`UICreatorXmlFixture` produces self-contained JSON, XML and Protobuf inputs. It
is a CTest fixture prerequisite, so selecting only the CLI test still generates
fresh inputs. The tests use a display buffer in memory and require no window,
board, panel, external XML service or license token.
