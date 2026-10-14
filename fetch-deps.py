"""Fetch only build dependencies, pinned by PDFium DEPS, always depth=1."""
import concurrent.futures
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parent / 'vendor' / 'pdfium'
scope = {}
scope['Var'] = lambda name: scope['vars'][name]
exec((root / 'DEPS').read_text(encoding='utf-8'), scope)
needed = [
    'base/allocator/partition_allocator', 'base/trace_event/common',
    'build', 'buildtools', 'third_party/abseil-cpp',
    'third_party/freetype/src', 'third_party/googletest/src',
    'third_party/icu', 'third_party/libjpeg_turbo', 'third_party/zlib',
    'third_party/nasm', 'tools/clang',
]

def fetch(name):
    spec = scope['deps'][name]
    if isinstance(spec, dict):
        spec = spec['url']
    url, revision = spec.rsplit('@', 1)
    dest = root / name
    dest.mkdir(parents=True, exist_ok=True)
    def git(*args):
        return subprocess.check_output(['git', '-C', str(dest), *args], stderr=subprocess.STDOUT, text=True)
    if not (dest / '.git').exists():
        git('init')
        git('remote', 'add', 'origin', url)
    try:
        if git('rev-parse', 'HEAD').strip() == revision:
            print('Ready', name, flush=True)
            return
    except subprocess.CalledProcessError:
        pass
    print('Fetching depth=1', name, flush=True)
    git('fetch', '--depth=1', 'origin', revision)
    git('checkout', '--detach', 'FETCH_HEAD')
    print('Ready', name, flush=True)

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for result in pool.map(fetch, needed):
        pass
