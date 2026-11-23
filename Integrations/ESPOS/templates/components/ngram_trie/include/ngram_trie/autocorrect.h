#pragma once
#include "ngram_trie/model.h"
#include "PredictionAutocorrect.h"
#include "Utf8Fold.h"
namespace ngram_trie {
using espos::keyboard::prediction::AutocorrectResult;
using espos::keyboard::prediction::AutocorrectScratch;
using espos::keyboard::prediction::FoldUtf8;
using espos::keyboard::prediction::BestAutocorrection;
using espos::keyboard::prediction::BestAutocorrectionUtf8;
using espos::keyboard::prediction::PrepareKeyboardQuery;
}
