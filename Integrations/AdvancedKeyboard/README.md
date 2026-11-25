# Canonical Advanced Keyboard dependency

Shared keyboard source is maintained only in
[rohmer/AdvancedKeyboard](https://github.com/rohmer/AdvancedKeyboard). Creator
uses its pinned `3rdParty/AdvancedKeyboard` submodule; ESPOS uses a nested
`third_party/AdvancedKeyboard` checkout at the same `advanced-keyboard.lock.json`
revision. ESPOS does not need a surrounding Git repository.

```sh
git submodule update --init --recursive
python Integrations/AdvancedKeyboard/dependency.py --check
```

For an unpacked source archive, or to prepare preserved OpenNLP resources:

```sh
python Integrations/AdvancedKeyboard/dependency.py --init
```

For ESPOS, run its deployed bootstrap with the explicit project directory:

```sh
python tools/setup_advanced_keyboard.py --project-root /path/to/ESPOS --kind espos --init
python tools/setup_advanced_keyboard.py --project-root /path/to/ESPOS --kind espos --check
```

The bootstrap clones only the locked public dependency. It does not initialize
ESPOS, use an ancestor Git repository, overwrite dirty checkouts, switch
user branches or update to a moving branch head. Existing ignored OpenNLP JARs
and Java-home settings can be reused; corpora/custom layouts/models/settings and
existing build outputs remain project-owned.

## Maintenance

Make shared C/C++, Python/C# tool, layout/font or trie changes in AdvancedKeyboard.
Run its tests, commit and push there. Then fetch/check out that published commit
in `3rdParty/AdvancedKeyboard`, update `advanced-keyboard.lock.json`, and stage
only the changed gitlink/lock with the consumer integration changes. Deploy the
same lock and adapter changes using the reviewed ESPOS adapter plan. An existing
clean ESPOS dependency can be fetched/checked out explicitly to the new locked
SHA before `--check`; local edits always require deliberate review.

Creator CMake accepts `-DADVKBD_SOURCE_ROOT=/explicit/development/checkout`;
ESPOS CMake accepts `-DADVANCED_KEYBOARD_ROOT=/explicit/development/checkout`.
Creator MSBuild accepts `/p:AdvancedKeyboardRoot=...`, and Python source generators
accept the `ADVANCED_KEYBOARD_ROOT` environment variable for deliberate development.
They never fall back to the retired copies. CI initializes submodules and checks
the pinned default checkout.

Creator-specific editor windows/catalog adapters and ESPOS-specific board
startup remain local integration code. Runtime SDKs export a selected source
snapshot from the dependency so devices/offline toolkits remain self-contained;
those generated outputs are not maintained implementations. The old extractor
and its duplicate templates were retired. Initial import provenance is retained
in the AdvancedKeyboard repository's `PROVENANCE` directory.

See the dependency's [build guide](../../3rdParty/AdvancedKeyboard/BUILD.md) and
[tools guide](../../3rdParty/AdvancedKeyboard/tools/README.md).
