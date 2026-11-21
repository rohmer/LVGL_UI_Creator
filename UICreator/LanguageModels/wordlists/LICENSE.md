# English word list (`words_alpha.txt`)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>

# Spanish word list (`words_es_top150000.txt`)

Adapted from Hermit Dave's [FrequencyWords Spanish 2018 data](https://github.com/hermitdave/FrequencyWords/blob/master/content/2018/es/es_full.txt), which uses the OpenSubtitles corpus. FrequencyWords data and this adaptation are licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). [Source license notice](https://github.com/hermitdave/FrequencyWords#license)

The adaptation lowercases and normalizes entries, combines frequencies for duplicate spellings, and retains the 150,000 most frequent alphabetic forms recognized by at least one of the Mexican, Spanish, or Argentine editions of [RLA-ES v2.9](https://github.com/sbosio/rla-es/releases/tag/v2.9). The text file lists the forms in frequency order.

# French word list (`words_fr_top150000.txt`)

Adapted from [Lexique 4.10](https://www.lexique.org/) by Boris New, Christophe Pallier, Guillaume Schalchli, Jérôme Bourgin, and Manuel Gimenes. Lexique and this adaptation are licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).

The list uses published orthographic subtitle frequencies, with repeated spellings deduplicated by their highest published total. Alphabetic entries were checked against the [Grammalecte 7.7 lexicon](https://www.grammalecte.net/?lang=fr_fr#dictionaries), which was also used to restore unambiguous ligatures such as `œ` in `cœur`. It includes internal-apostrophe words attested in Lexique. The output uses lowercase NFC with straight apostrophes.

# German and Greek word lists (`words_de_top150000.txt`, `words_el_top150000.txt`)

Adapted from Hermit Dave's FrequencyWords 2018 data for [German](https://github.com/hermitdave/FrequencyWords/blob/master/content/2018/de/de_full.txt) and [Greek](https://github.com/hermitdave/FrequencyWords/blob/master/content/2018/el/el_full.txt), based on OpenSubtitles. FrequencyWords data and these adaptations are licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). [Source license notice](https://github.com/hermitdave/FrequencyWords#license)

Entries were lowercased and normalized, with frequencies combined for duplicate spellings. Forms were checked against the [LibreOffice German frami dictionary](https://github.com/LibreOffice/dictionaries/blob/master/de/README_extension_owner.txt) or [LibreOffice Greek dictionary](https://github.com/LibreOffice/dictionaries/blob/master/el_GR/README_el_GR.txt) for the respective language. The files retain accents, German ß, and Greek final sigma.
