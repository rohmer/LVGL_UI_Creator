"""Development-only generator; the native app uses the checked-in C++ tables."""
from pathlib import Path
import unicodedata as ud

root=Path(__file__).resolve().parent
parts=['// Generated with Unicode '+ud.unidata_version+'; exact ESPOS CaseFold.txt mappings.\n']
def array(name,kind,rows):
    parts.append('constexpr '+kind+' '+name+'[] = {\n')
    parts.extend('    {'+', '.join(hex(value) for value in row)+'},\n' for row in rows)
    parts.append('};\n')
def ranges(predicate):
    out=[];start=None;previous=None
    for cp in range(0x110000):
        if predicate(chr(cp)):
            if start is None:start=cp
            previous=cp
        elif start is not None:out.append((start,previous));start=None
    if start is not None:out.append((start,previous))
    return out
array('kWordRanges','Range',ranges(lambda c:ud.category(c)[0] in 'LN'))
array('kMarkRanges','Range',ranges(lambda c:ud.category(c)[0]=='M'))
array('kSpaceRanges','Range',ranges(lambda c:ud.category(c) in ('Zs','Zl','Zp') or ord(c) in (9,10,11,12,13,0x85)))
lower=[];combining=[];decomp=[];compositions=[]
for cp in range(0x110000):
    char=chr(cp);small=char.lower()
    if len(small)==1 and ord(small)!=cp:lower.append((cp,ord(small)))
    ccc=ud.combining(char)
    if ccc:combining.append((cp,ccc))
    raw=ud.decomposition(char)
    if raw and not raw.startswith('<'):
        values=[int(item,16) for item in raw.split()]
        assert 1<=len(values)<=2
        decomp.append((cp,values[0],values[1] if len(values)>1 else 0))
        if len(values)==2 and ud.normalize('NFC',''.join(map(chr,values)))==char:
            compositions.append(((values[0]<<21)|values[1],cp))
array('kLower','Pair',lower)
array('kCombining','Pair',combining)
array('kDecompositions','Decomposition',decomp)
array('kCompositions','Composition',sorted(compositions))
fold=[];expanded=[]
for line in (root/'CaseFold.txt').read_text(encoding='utf-8').splitlines():
    if not line or line.startswith('#'):continue
    values=[int(value,16) for value in line.split()]
    fold.append((values[0],len(expanded),len(values)-1));expanded.extend(values[1:])
array('kFold','Decomposition',fold)
parts.append('constexpr std::uint32_t kFoldScalars[] = {\n')
for i in range(0,len(expanded),12):parts.append('    '+', '.join(hex(x) for x in expanded[i:i+12])+',\n')
parts.append('};\n')
(root/'UnicodeTables.inc').write_text(''.join(parts),encoding='utf-8')
