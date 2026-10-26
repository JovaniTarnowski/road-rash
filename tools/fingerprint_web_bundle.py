#!/usr/bin/env python3
"""Give each browser bundle immutable URLs, including its exact Wasm dependency."""
import hashlib
from pathlib import Path
import re
import sys


def fingerprint(directory):
    # Remove only generated fingerprints from a previous invocation. The build
    # has just copied fresh source assets into this exclusively generated dir.
    generated = re.compile(r'(?:rr-game|app|style|references)\.[0-9a-f]{16}\.(?:js|wasm|css|json)')
    for path in directory.iterdir():
        if path.is_file() and generated.fullmatch(path.name):
            path.unlink()

    def rename(name, data=None):
        original = directory / name
        data = original.read_bytes() if data is None else data
        stem, extension = name.rsplit('.', 1)
        target = f'{stem}.{hashlib.sha256(data).hexdigest()[:16]}.{extension}'
        (directory / target).write_bytes(data)
        original.unlink()
        return target

    def replace_once(text, before, after):
        if text.count(before) != 1:
            raise ValueError(f'Expected exactly one bundle reference to {before}')
        return text.replace(before, after)

    wasm = rename('rr-game.wasm')
    glue = replace_once((directory / 'rr-game.js').read_text(), 'rr-game.wasm', wasm)
    engine = rename('rr-game.js', glue.encode())
    references = rename('references.json')
    app = replace_once((directory / 'app.js').read_text(), "fetch('references.json')", f"fetch('{references}')")
    app = rename('app.js', app.encode())
    style = rename('style.css')
    index = (directory / 'index.html').read_text()
    for before, after in [('rr-game.js', engine), ('app.js', app), ('style.css', style)]:
        index = replace_once(index, before, after)
    (directory / 'index.html').write_text(index)
    print(f'Fingerprint bundle: {engine} -> {wasm}')


if __name__ == '__main__':
    fingerprint(Path(sys.argv[1]).resolve())
