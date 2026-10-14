"""Convert pinned ICU .long data to MSVC-compatible, statically linked C++."""
import re
import sys
from pathlib import Path
source = Path(sys.argv[1]).read_text()
symbol = re.search(r'\.globl\s+_(\w+)', source).group(1)
with open(sys.argv[2], 'w') as out:
    out.write('extern "C" { __declspec(align(16)) extern const unsigned int ' + symbol + '[] = {\n')
    for line in source.splitlines():
        if line.startswith('.long '):
            out.write(line[6:] + ',\n')
    out.write('}; }\n')
