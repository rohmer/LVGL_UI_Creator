from pathlib import Path
import copy
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import feature_plan
import trie_assets


def var(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def single_radix(word=b"hello", count=(1 << 32) + 7, scored=False):
    # Four physical records: two roots and one whole-word edge per root.
    result = bytearray(struct.pack('<4sIII', b'NGTR', 4, 4, 2 + 2 * len(word)))
    result += b'\0' + var(8 | (2 if scored else 0))
    if scored: result += struct.pack('<h', 0)
    result += b'\0' + var(8 | (2 if scored else 0))
    if scored: result += b'\xd8\xa7'  # Score bytes resemble Arabic, but are not text.
    for _ in range(2):
        result += var(len(word)) + word + var(1 | (2 if scored else 0)) + var(count)
        if scored: result += b'\xd8\xa7'
    return bytes(result)


def fixed_v2():
    header = struct.pack('<4sIII', b'NGTR', 2, 4, 12)
    return header + b''.join(struct.pack('<IIHBB', first, 19, degree, label, 0)
        for first, degree, label in ((2, 1, 0), (3, 1, 0), (0, 0, ord('x')), (0, 0, ord('x'))))


def document(model, index=None):
    record = {'id': 'radix', 'name': 'Embedded model', 'format': 'ngtr', 'data': model}
    if index is not None: record['indexData'] = index
    return {'formatVersion': 2, 'runtimeTarget': '9.6', 'canvas': {'width': 320, 'height': 240, 'base': {}},
            'obj': [{'oid': 0, 'par': -1, 'base': {}, 'advancedKeyboard': {'language': 'fr-FR', 'trieId': 'radix'}}],
            'tries': [record]}


class RadixAssetTests(unittest.TestCase):
    def test_radix_and_scored_wide_counts_survive_wire_and_profile(self):
        root = feature_plan.repository_root()
        for scored in (False, True):
            original = document(single_radix(scored=scored))
            optimized = feature_plan.optimize_document(copy.deepcopy(original))
            self.assertEqual(original['tries'][0]['data'], optimized['tries'][0]['data'])
            self.assertEqual(optimized['tries'][0]['indexData'][:8], b'NGIX\x02\0\0\0')
            self.assertEqual(feature_plan.decode_project(feature_plan.encode_project(optimized)), optimized)
            plan = feature_plan.analyze_projects([optimized])
            self.assertIn(trie_assets.PREDICTION_GATE, plan['customFeatures'])
            self.assertNotIn(trie_assets.BUILTIN_GATE, plan['customFeatures'])
            self.assertEqual(plan['trieAssetCount'], 1)
            self.assertEqual(plan['trieModelBytes'], len(original['tries'][0]['data']))
            self.assertGreater(plan['trieIndexBytes'], 16)
            self.assertNotIn('BIDI', plan['enabledFeatures'])

    def test_binary_scores_do_not_enable_arabic_features(self):
        value = document(single_radix(scored=True))
        metadata = trie_assets.prepare(value, feature_plan.repository_root())
        self.assertFalse(metadata['trieNeedsBidi'])
        self.assertFalse(metadata['trieNeedsArabic'])
        value = document(single_radix(word='السلام'.encode('utf-8'), scored=True))
        metadata = trie_assets.prepare(value, feature_plan.repository_root())
        self.assertTrue(metadata['trieNeedsBidi'])
        self.assertTrue(metadata['trieNeedsArabic'])

    def test_unicode_continuation_crosses_radix_spans(self):
        # The first 256-byte span ends with the first byte of Arabic alef.
        prefix = b'a' * 255 + b'\xd8'
        tail = b'\xa7abc'
        model = bytearray(struct.pack('<4sIII', b'NGTR', 4, 6, 2 + 2 * (len(prefix) + len(tail))))
        model += b'\0\x08\0\x08'
        for _ in range(2): model += var(256) + prefix + b'\x08'
        for _ in range(2): model += var(len(tail)) + tail + b'\x01\x01'
        value = document(bytes(model))
        metadata = trie_assets.prepare(value, feature_plan.repository_root())
        self.assertTrue(metadata['trieNeedsBidi'])
        self.assertTrue(metadata['trieNeedsArabic'])

    def test_fixed_v2_uses_empty_index_and_roundtrips(self):
        for supplied in (None, b'', ''):
            optimized = feature_plan.optimize_document(document(fixed_v2(), supplied))
            self.assertEqual(optimized['tries'][0]['indexData'], b'')
            self.assertEqual(feature_plan.decode_project(feature_plan.encode_project(optimized)), optimized)
        with self.assertRaises(ValueError): feature_plan.optimize_document(document(fixed_v2(), b'invalid'))

    def test_mismatched_radix_index_and_malformed_span_rejected(self):
        root = feature_plan.repository_root()
        valid = document(single_radix(scored=True))
        trie_assets.prepare(valid, root)
        invalid = copy.deepcopy(valid)
        invalid['tries'][0]['indexData'] = invalid['tries'][0]['indexData'][:-1]
        with self.assertRaises(ValueError): trie_assets.prepare(invalid, root)
        invalid = bytearray(single_radix())
        invalid[12:16] = struct.pack('<I', 999)
        with self.assertRaises(ValueError): trie_assets.prepare(document(bytes(invalid)), root)


if __name__ == '__main__': unittest.main()
