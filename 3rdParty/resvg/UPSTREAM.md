# Native SVG renderer

Resvg 0.45.1, commit `1b6c2fddbcbeffa8135df4323b02aaae84890907`, from
https://github.com/linebender/resvg/tree/v0.45.1.

Archive SHA-256:
`02915519b7409f43110f3cbdc5f87724efd58da1d8516914bdabf060c8a9a178`.

The source and locked Cargo dependencies are unchanged. Test fixtures and example
inputs are omitted. `CMakeLists.txt` is Creator integration code. The static
native C API preserves SVG text, filters, gradients, clipping and embedded image
support without a Python runtime. Its minimum Rust version is 1.67.1; Creator's
pinned Rust 1.85.1 toolchain is supported.

Resvg is available under MIT or Apache-2.0, at the recipient's option. See
`LICENSE-MIT` and `LICENSE-APACHE`. Dependency licenses are recorded in the
packaged third-party notices.
