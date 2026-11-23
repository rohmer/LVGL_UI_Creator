# Shared Advanced Keyboard

When changing Advanced Keyboard, read `Integrations/ESPOS/README.md` and its
sync manifest. Keep the portable implementation, layouts and bundled model
catalog aligned with the ESPOS AdvancedKeyboard component.

- Edit shared native code in `LVSerial/Widgets/AdvancedKeyboard`. ESPOS board
  drivers and compatibility/UI adapters belong outside the portable component;
  their maintained deployment sources are in `Integrations/ESPOS/templates`.
- For new language, model, prediction, target-field or language/size-control
  behavior, update the corresponding ESPOS adapter where applicable. The
  Creator inspector itself is platform UI and is not copied into ESPOS.
- After changing manifest-owned files, run the sync unit tests, review and apply
  a push to the explicit ESPOS checkout, then run both `--check` and
  `--check-source` before declaring those shared changes synchronized. Use a
  reviewed pull for changes authored in ESPOS. Resolve conflicts without
  overwriting edits on either side.
- Include `sync-baseline.json` updates with the source changes, and run the
  relevant native/editor and ESP32 builds/tests. Source checks are also
  registered with CTest. If the peer is unavailable, state that parity remains
  unverified; do not fabricate a new baseline.
- Preserve unrelated board configuration, corpora, saved settings and build
  outputs. Never use an ancestor Git repository as the ESPOS repository, and
  do not initialize or publish a separate repository unless requested.

The user's instruction to keep these projects in sync authorizes routine
paired source updates. It does not authorize discarding conflicts or user data.
