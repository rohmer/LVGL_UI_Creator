# Problems and accessibility checks

The persistent **Problems** panel reports the current project's errors and warnings beside the canvas. It checks a changed document after a short pause, using a revision counter rather than repeatedly serializing an idle project. Changing the active editing screen refreshes live geometry and font measurements. **Check now** forces a check; the filter selects all findings, errors or warnings. Large result sets are displayed in pages of 200 findings.

Click a finding to select its widget, including navigation to another screen. Available quick fixes are explicit and undoable:

- Increase a small touch target to a minimum of 44 pixels.
- Restore natural keyboard focus order.
- Clear an invalid variable-to-widget binding while keeping the variable.
- Remove actions whose object or screen references are broken.

If the project changes before a quick fix is selected, the panel refreshes the findings first. It does not apply a stale ordinal ID to another widget.

## Contrast and focus

Contrast checks compare text with a known opaque solid background. The default advisory minimum is 4.5:1; a widget can store another threshold in `base.accessibility.minimumContrast`. Translucent, gradient or image backgrounds are skipped because a simple solid-color ratio would be misleading. Warnings are design guidance rather than certification of accessibility compliance.

The **Focus order** tab edits the selected control's `base.accessibility.tabIndex`. Nonnegative values are explicit priorities, followed by controls with natural order. `-1` excludes that control and its internal focusable descendants. **Natural order** removes the override. Validation identifies duplicate or invalid indexes and explicit indexes on hidden/disabled controls. The portable runtime helper applies the same ordering to native LVGL focus groups, including custom textbox entries.

## Translated text

The **Translations** tab assigns a `base.translationKey` to a selected widget and stores text values in a project locale table:

```json
{
  "translations": {
    "defaultLocale": "en",
    "locales": {
      "en": {"settings.title": "Settings"},
      "fr": {"settings.title": "Paramètres"}
    }
  }
}
```

**Assign key and save** changes the selected widget's key and saves the entered locale text. **Save translation** updates a value without reassigning a widget. **Load saved text** retrieves a stored value for editing. Typing in these controls does not change the authored project until Save is selected.

Checks identify missing locale entries, missing glyphs and text that does not fit the selected widget's actual content area and font. Font/layout-dependent checks run for the live editing screen; navigating to another screen checks its actual fonts. Text is measured with wrapping for labels and single-line width for buttons and single-line text areas. The authored text remains the fallback; the validation editor does not silently replace it or change keyboard language models.

Existing layout, touch, asset, glyph, binding and event checks remain in **Debug > Validate screen**. Export and build guards use errors only; warnings remain reviewable without preventing export.
