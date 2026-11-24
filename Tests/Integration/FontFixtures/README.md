`UICreatorFontFixture.woff2` is an ASCII subset of the already vendored
`3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf`, renamed
"UI Creator Font Fixture" to respect the font's reserved name. The original
SIL Open Font License and copyright notice are retained in `LICENSE.txt`.

This checked-in fixture tests the native Brotli/WOFF2 path. No Python modules,
Node runtime, or external font utilities are needed to run the test or app.
It gives the default OpenType script an explicit A/V adjustment of -152 font
units (the original font's legacy A/V kerning), so the tests can also exercise
GPOS extraction and both binary and C kerning-class output independently.

It was generated once with fontTools and Brotli using this development recipe:

```python
from fontTools.ttLib import TTFont
from fontTools import subset
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString

font = TTFont("3rdParty/LVGL/lvgl/src/libs/freetype/LiberationSans-Regular.ttf")
options = subset.Options()
options.layout_features = ["*"]
options.name_IDs = ["*"]
options.name_legacy = True
options.name_languages = ["*"]
subsetter = subset.Subsetter(options=options)
subsetter.populate(unicodes=range(0x20, 0x7f))
subsetter.subset(font)
del font["GPOS"]
addOpenTypeFeaturesFromString(font,
    "languagesystem DFLT dflt; feature kern { pos A V -152; } kern;")
for record in font["name"].names:
    if record.nameID in (1, 4, 6, 16):
        name = "UICreatorFontFixture" if record.nameID == 6 else "UI Creator Font Fixture"
        record.string = name.encode(record.getEncoding())
font.recalcTimestamp = False
font.flavor = "woff2"
font.save("Tests/Integration/FontFixtures/UICreatorFontFixture.woff2")
```
