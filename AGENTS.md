# Advanced Keyboard authority

Read `Integrations/AdvancedKeyboard/README.md` before keyboard changes.
The implementation, tools, layouts, fonts, tries and portable tests are
maintained only in `https://github.com/rohmer/AdvancedKeyboard`.

- Creator consumes the pinned `3rdParty/AdvancedKeyboard` Git submodule;
  ESPOS consumes `third_party/AdvancedKeyboard` at the same locked commit.
  Edit and commit shared code in that repository, then update the consumer
  gitlink and `advanced-keyboard.lock.json`. Never recreate source/model copies
  in the old LVSerial, Tools, UICreator trainer or ESPOS keyboard directories.
- The Creator inspector and editor windows stay in Creator. ESPOS board
  drivers/startup and project-specific wrappers stay outside the dependency.
  `Integrations/ESPOS/manifest.json` owns only their deployment glue, lock and
  bootstrap helpers. Its reviewed sync preserves edits/conflicts and user data.
- After adapter changes, run sync unit tests, review/apply a push to the
  explicit ESPOS project, and run both `--check` and `--check-source`. Use a
  reviewed pull for peer-authored changes; never overwrite conflicts. Include
  the real resulting `sync-baseline.json`, not a fabricated baseline.
- Run relevant canonical, Creator/export/runtime and ESP32 checks. Generated
  SDK/project source snapshots are outputs, never another maintained fork.
- Preserve corpora, custom layouts/models, saved settings, ignored downloads,
  board configuration and build outputs. Never use an ancestor Git repository
  as ESPOS, and never initialize ESPOS itself without an explicit request.
