# Built-in font sources

The editor includes twelve TrueType choices: the original four fonts supplied by
the user from `C:\tmp`, plus eight unmodified open-source regular faces: Inter,
JetBrains Mono, Lato, Montserrat, Noto Sans, Open Sans, Roboto and Source Sans 3.

`catalog.json` records original filenames, source URLs, pinned revisions or
release versions, byte counts, SHA-256 hashes, naming records and embedding flags.
The new upstream fonts use SIL Open Font License 1.1; each complete license is
included in its `*-OFL.txt` file. `NOTICES.txt` preserves embedded notices.
The original four user-provided fonts have their own embedded copyright notices;
they are not represented as open-source fonts. Embedding flags are metadata,
not a replacement for a font license.

All font binaries are original upstream/user files, not local subsets or renamed
font-family derivatives. Seven open-source additions are static Regular TTFs;
Roboto is the upstream variable TTF at its default Regular axes (weight 400,
width 100). The converter uses those defaults without exposing variation axes.
The entire twelve-font source catalog is about 3.6 MB.

The editor installs these files under `fonts/builtin` and resolves them relative
to its resource directory. It does not depend on a temporary import folder or a
network connection. Fonts are rasterized on demand after selection; browsing the
picker does not add them to a project. Device exports include only raster data
for fonts referenced by rendered screens, never this desktop TTF catalog.
