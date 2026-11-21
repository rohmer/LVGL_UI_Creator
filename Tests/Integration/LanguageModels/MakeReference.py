"""Regenerate tiny native-port parity fixtures using a read-only ESPOS build.

This is developer tooling only. The CTest tests consume the checked-in files
and do not require .NET or an ESPOS checkout.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

repo = Path(__file__).resolve().parents[3]
if len(sys.argv) != 2:
    raise SystemExit('Usage: MakeReference.py <ESPOS/tools/NgramTrie>')
source = Path(sys.argv[1]).resolve()
stage = repo / 'build/language-model-reference'
stage.mkdir(parents=True, exist_ok=True)
reference = stage / 'reference-bin'
shutil.copytree(source / 'bin/Release/net8.0', reference, dirs_exist_ok=True)
environment = dict(os.environ)
for variable, directory in [('DOTNET_CLI_HOME', 'dotnet-home'), ('TEMP', 'temporary'), ('TMP', 'temporary')]:
    location = stage / directory
    location.mkdir(exist_ok=True)
    environment[variable] = str(location)
environment['DOTNET_SKIP_FIRST_TIME_EXPERIENCE'] = '1'
fixtures = Path(__file__).resolve().parent / 'fixtures'
fixtures.mkdir(exist_ok=True)
corpora = {
    'ascii': 'the world. the world. the world.\nhello world. hello there. help the hero.\nin the world. in the world.\n',
    'folded': 'Stra\u00dfe STRASSE. CAF\u00c9 caf\u00e9. \u039f\u03a3 \u03bf\u03c2.\nna\u00efve 123 can\u2019t can\'t.\n',
    'es': 'HOLA mundo. Ni\u00f1o ni\u00f1a. CANCI\u00d3N cancio\u0301n. \u00c1RBOL \u00e1rbol.\n',
    'fr': 'CAF\u00c9 caf\u00e9. \u00c9COLE e\u0301cole. \u0152UVRE \u0153uvre. \u00c9T\u00c9 \u00e9t\u00e9.\n',
    'de': 'Stra\u00dfe STRASSE Stra\u00dfe. F\u00dcNF f\u00fcnf. \u00c4PFEL \u00c4pfel.\n',
    'el': '\u0393\u0395\u0399\u0391 \u03ba\u03cc\u03c3\u03bc\u03bf\u03c2. \u03a3\u03a0\u0399\u03a4\u0399 \u03c3\u03c0\u03af\u03c4\u03b9. \u039b\u039f\u0393\u039f\u03a3 \u03bb\u03cc\u03b3\u03bf\u03c2.\n',
}
for name, content in corpora.items():
    (fixtures / (name + '.txt')).write_text(content, encoding='utf-8', newline='\n')
cases = [
    ('full', 'ascii', ['--no-wordlist']),
    ('legacy', 'ascii', ['--no-wordlist']),
    ('budget128', 'ascii', ['--no-wordlist', '--trie-size', '128']),
    ('budget20', 'ascii', ['--no-wordlist', '--trie-size', '20']),
    ('limits', 'ascii', ['--no-wordlist', '--unigrams', '3', '--bigrams', '2', '--trigrams', '1']),
    ('zero', 'ascii', ['--no-wordlist', '--unigrams', '0', '--bigrams', '0', '--trigrams', '0']),
    ('folded', 'folded', ['--no-wordlist']),
] + [(language, language, ['--language', language]) for language in ('es', 'fr', 'de', 'el')]
for name, corpus, arguments in cases:
    command = ['dotnet', str(reference / 'NgramTrie.dll'), '--no-cache', '--workers', '1',
               '--file', str(fixtures / (corpus + '.txt')), '--trie-output', str(fixtures / (name + '.ngtr')),
               '--output', str(fixtures / (name + '.json'))] + arguments
    if name != 'legacy':
        command.append('--compact')
    result = subprocess.run(command, env=environment, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{name}: {result.stderr}')
    print(f'{name}: {(fixtures / (name + ".ngtr")).stat().st_size} bytes')
command = ['dotnet', str(reference / 'NgramTrie.dll'), '--compact', '--no-cache', '--no-wordlist', '--low-memory',
           '--workers', '2', '--file', str(fixtures / 'ascii.txt'), '--trie-output', str(stage / 'disk.ngtr')]
result = subprocess.run(command, env=environment, capture_output=True, text=True)
if result.returncode:
    raise RuntimeError(result.stderr)
assert (stage / 'disk.ngtr').read_bytes() == (fixtures / 'full.ngtr').read_bytes(), 'C# disk and memory outputs differ'
manifest = {
    'description': 'Generated from the original ESPOS NgramTrie C# implementation; tiny authored corpora only.',
    'sourceSha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(source.glob('*.cs'))},
    'referenceDllSha256': hashlib.sha256((reference / 'NgramTrie.dll').read_bytes()).hexdigest(),
    'cases': [{'name': name, 'corpus': corpus + '.txt', 'options': options} for name, corpus, options in cases],
}
(fixtures / 'reference.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
