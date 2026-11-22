# GBUTest spelling cases

`common-misspellings.jsonl` contains 1,550 English spelling cases covering 906 expected words. It is a curated fixture of familiar misspellings and representative typing slips. The cases have no frequency ranking.

Each UTF-8 line is a JSON object with one misspelled input, its expected correction, and a category.

```json
{"input":"teh","expected":"the","category":"transposition"}
```

| Category | Meaning | Cases |
| --- | --- | ---: |
| `missing_letter` | The input omits one letter. | 508 |
| `extra_letter` | The input adds one letter. | 247 |
| `substitution` | The input replaces one letter. | 269 |
| `transposition` | The input swaps two adjacent letters. | 444 |
| `multiple_edits` | The correction requires more than one edit. | 82 |

Inputs and corrections are lowercase single words, matching the trie generator's token convention. Every input has one expected answer in this fixture. The expected correction can be absent from a particular trained trie, depending on the corpus, n-gram limits, and byte budget. A test runner can report those cases separately from ranking errors.

## Run the benchmark

From the ESPOS project directory, point the benchmark at an existing `.ngtr` file.

```powershell
dotnet run --project tools/GBUTest/GBUTest.csproj -c Release -- D:\Corpus\model.ngtr
```

The runner calls the simulator's production predictor on every case with empty preceding context. It compares prefix-only predictions with keyboard-weighted correction. Top 1 and top 3 rates in the table are over all cases in that row. **Coverage** is the share of cases whose expected word is present in the model's unigram vocabulary. The report also gives hit rates over that covered subset.

The separate adjacent-key suite samples one substitution per eligible expected word with a fixed seed. It uses the simulator's keyboard neighbor map and skips inputs already used by the fixture or recognized by the trie as complete words.

Add `--json` after the model path for counts by category, the model SHA-256, and the list of expected words absent from the trie.

## Generate fuzzed sentence pairs

From the ESPOS project directory, build and open the desktop fuzzer.

```powershell
dotnet build tools/GBUTest.Fuzzer/GBUTest.Fuzzer.csproj -c Release
.\tools\GBUTest.Fuzzer\bin\Release\net8.0-windows\GBUTest.Fuzzer.exe
```

Choose an input text file with one sentence per line and at least 1,000 nonblank lines. The fuzzer samples 1,000 different input line positions at random and keeps the sampled pairs in random order. Choose an output file and set the two percentages. Both start at 5% and can be set to 0% to disable that type of mistake.

Select an **Eligibility trie** (`.ngtr`, version 2 or 3). The fuzzer uses the English keyboard profile’s case folding. An original word is eligible only if it is stored as a complete unigram in the selected trie.

- **Common misspelling** chooses one of the fixture's misspellings when a word has one and its chance succeeds.
- **Adjacent key mistake** changes one letter to a neighboring English QWERTY key when its chance succeeds. The two chances are independent for each word. If both succeed, the adjacent substitution is applied after the common misspelling.

The fuzzer preserves tokens containing digits, English number words, and dates such as `2026-11-17`, `Nov. 17, 2026`, and `17th of May`. Regenerate pairs created with older versions to apply number and date protection to their fuzzed lines.

The UTF-8 output contains 2,000 lines. Each original sentence is immediately followed by its fuzzed version, including pairs where the sentences are identical. The side-by-side preview colors any pair with a common misspelling red, and pairs changed only by adjacent-key mistakes green. Keep the `.edits.json` file beside the text output to preserve these colors in **Open paired file**. Changed pairs in older text files open in green.

## Score typing and autocorrect

Build and open the desktop tester from the ESPOS project directory.

```powershell
dotnet build tools/GBUTest.UI/GBUTest.UI.csproj -c Release
.\tools\GBUTest.UI\bin\Release\net8.0-windows\GBUTest.UI.exe
```

Choose a fuzzed pair file and a trie, then click **Run test**. Use the outcome filter to inspect individual corrections and their points.

The runner evaluates autocorrect at each word boundary, using the last two committed words in the sentence as context. It normalizes the keyboard-weighted trie scores into probabilities across plausible replacements and keeping the typed word. A replacement must rank first and meet the threshold. The default threshold is 0.9. Autocorrect leaves digit-bearing tokens and recognized English number and calendar words unchanged.

```powershell
dotnet run --project tools/GBUTest/GBUTest.csproj -c Release -- --pairs D:\Corpus\pairs.txt --trie D:\Corpus\model.ngtr --threshold 0.9
```

| Outcome | Result | Points |
| --- | --- | ---: |
| Good | Corrected a typo to the intended word | +10 |
| Bad | Replaced a typo with the wrong word | −25 |
| Ugly | Replaced a correctly typed word with the wrong word | −100 |
| Correct and kept | Kept a correctly typed word | +1 |
| Missed | Left a typo unchanged | 0 |

The report shows the total word count and composite score, calculated as total points divided by total words. The composite score is unavailable if the input contains no words. Add `--json` to include each scored word and its outcome in machine-readable output.

### Compare thresholds in the desktop tester

Use **Run regression (0.5–1.0)** to score the same paired file and trie at autocorrect thresholds from 0.5 through 1.0, including both endpoints. Set the threshold step from 0.001 to 1. With the default step of 0.05, the 11 thresholds are 0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.80, 0.85, 0.90, 0.95, and 1.00. Each run recomputes word decisions and their context because an accepted correction can affect the next word. The results show the outcome counts, total points, word count, and composite score for each threshold, and flag all thresholds tied for the highest composite score.

Each completed sweep saves a new CSV beside the paired sentence file. Click **Open CSV** to open it. Each row contains the tested threshold, scores, word count, best flag, and outcome counts. If the CSV cannot be saved, the results remain available in the tester.
