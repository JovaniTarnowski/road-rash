'use strict';
// No game-data network requests. File/OPFS are the only asset sources.
const $ = id => document.getElementById(id);
let engine, storageRoot, assetDirectory, references, importing = false, cancelled = false, running = false;
const status = (message, error = false) => { $('status').textContent = message; $('status').classList.toggle('error', error); };
// The original TEXT/*.DLL font files are TrueType data. Use the same
// verified local files as TextRenderer; never publish or fetch game fonts.
let discFonts;
async function loadDiscFonts(directory) {
    if (!discFonts) {
        discFonts = Promise.all([
            ['RoadRashBadloc', 'TEXT/BADLOC.DLL'],
            ['RoadRashFutura', 'TEXT/FUTR.DLL'],
            ['RoadRashFuturaBold', 'TEXT/FUTB.DLL'],
            ['RoadRashFuturaDemi', 'TEXT/FUTD.DLL']
        ].map(async ([family, path]) => {
            const file = await (await directory.getFileHandle(encodeURIComponent(path))).getFile();
            const face = await new FontFace(family, await file.arrayBuffer()).load();
            return face;
        })).then(faces => { faces.forEach(face => document.fonts.add(face)); });
    }
    try { await discFonts; }
    catch (error) { discFonts = undefined; throw error; }
}
const sync = populate => new Promise((resolve, reject) => engine.FS.syncfs(populate, e => e ? reject(e) : resolve()));
const call = (name, type = 'number', types = [], values = []) => engine.ccall(name, type, types, values);
async function writeFile(directory, name, bytes) {
    const handle = await directory.getFileHandle(name, {create: true});
    const stream = await handle.createWritable();
    try { await stream.write(bytes); await stream.close(); }
    catch (error) { await stream.abort().catch(() => {}); throw error; }
}
async function readJson(directory, name) {
    return JSON.parse(await (await (await directory.getFileHandle(name)).getFile()).text());
}
function storageError(error) {
    if (error.name === 'QuotaExceededError') return 'Browser storage is full. Free at least 600 MB on this device, then try again.';
    return error.message || String(error);
}
async function restoreDisc() {
    let hasMarker = false;
    try {
        const marker = await readJson(storageRoot, 'active.json');
        hasMarker = true;
        if (marker.version !== 1 || marker.files.length !== references.length) throw new Error('Disc import is incomplete');
        const dir = await storageRoot.getDirectoryHandle(marker.directory);
        const records = new Map(marker.files.map(file => [file.path, file]));
        for (const reference of references) {
            const record = records.get(reference.path);
            if (!record || record.hash !== reference.hash) throw new Error('Disc import needs updating');
            const file = await (await dir.getFileHandle(encodeURIComponent(reference.path))).getFile();
            if (file.size !== record.size) throw new Error('A stored game file is incomplete');
        }
        // Font support must not turn a verified disc into an import failure.
        // Unsupported browsers retain readable fallbacks; supported browsers
        // use the exact same font files as the game before launch is offered.
        try { await loadDiscFonts(dir); }
        catch (error) { console.warn('Local game fonts unavailable:', error); }
        assetDirectory = dir;
        engine.localAssets.names = new Set(references.map(r => r.path));
        $('ready').hidden = false;
        $('state').textContent = 'Disc ready';
        status('Your verified disc is stored on this device. Choose Main Game or Quick Race.');
        return true;
    } catch (error) {
        if (hasMarker || error.name !== 'NotFoundError') status('Stored disc unavailable. Re-import your original disc. ' + storageError(error), true);
        return false;
    }
}
async function importDisc(source, iso) {
    if (!engine || importing || running) return;
    importing = true; cancelled = false;
    $('ready').hidden = true; $('progress').hidden = false; $('cancel').hidden = false;
    $('progress').value = 0;
    for (const input of [$('iso'), $('folder')]) input.disabled = true;
    document.querySelectorAll('label.button').forEach(el => el.classList.add('disabled'));
    const directoryName = 'disc-' + crypto.randomUUID();
    let staging;
    try {
        // Preserve the previous complete disc until the new transaction commits.
        const estimate = await navigator.storage.estimate();
        if (estimate.quota && estimate.quota - estimate.usage < 510 * 1024 * 1024)
            throw new DOMException('Not enough room for disc import', 'QuotaExceededError');
        await navigator.storage.persist?.();
        staging = await storageRoot.getDirectoryHandle(directoryName, {create: true});
        const files = new Map();
        if (!iso) {
            for (const file of source) {
                const parts = file.webkitRelativePath.split('/');
                const index = parts.findIndex(p => p.toUpperCase() === 'ROADRASH');
                const relative = parts.slice(index >= 0 ? index + 1 : 1).join('/').toUpperCase();
                if (files.has(relative)) throw new Error('Duplicate disc file: ' + relative);
                files.set(relative, file);
            }
        }
        call('rr_import_begin', null, ['number'], [iso ? 1 : 0]);
        const imported = [];
        for (;;) {
            if (cancelled) throw new Error('Import cancelled. Your previous disc is preserved.');
            const request = call('rr_import_next', 'string');
            const need = JSON.parse(request);
            if (need.error) throw new Error(need.error);
            if (need.done) break;
            if (!iso && !files.has(need.file.toUpperCase())) {
                call('rr_import_missing', null); continue;
            }
            let bytes;
            if (iso) bytes = new Uint8Array(await source.slice(need.offset, need.offset + need.size).arrayBuffer());
            else bytes = new Uint8Array(await files.get(need.file.toUpperCase()).arrayBuffer());
            const pointer = engine._malloc(bytes.length || 1);
            if (!pointer) throw new Error('Not enough memory for disc import');
            try {
                engine.HEAPU8.set(bytes, pointer);
                call('rr_import_supply', null, ['number', 'number'], [pointer, bytes.length]);
            } finally { engine._free(pointer); }
            const output = call('rr_import_output', 'string');
            if (output) {
                const start = call('rr_import_data');
                const size = call('rr_import_size');
                await writeFile(staging, encodeURIComponent(output), engine.HEAPU8.slice(start, start + size));
                imported.push({path: output, size, hash: references.find(r => r.path === output).hash});
                call('rr_import_flushed', null);
            }
            const count = call('rr_import_count');
            $('progress').value = count;
            status(`Verifying disc · ${count} / ${references.length} files${output ? ' · ' + output : ''}`);
            await new Promise(resolve => setTimeout(resolve, 0));
        }
        if (imported.length !== references.length) throw new Error('Disc verification did not produce every required file');
        await writeFile(storageRoot, 'active.json', JSON.stringify({version: 1, directory: directoryName, files: imported}));
        // The marker is the commit point. Cleanup cannot invalidate this import.
        for await (const [name, handle] of storageRoot.entries()) {
            if (handle.kind === 'directory' && name.startsWith('disc-') && name !== directoryName)
                await storageRoot.removeEntry(name, {recursive: true}).catch(() => {});
        }
        await restoreDisc();
    } catch (error) {
        call('rr_import_cancel', null);
        if (staging) await storageRoot.removeEntry(directoryName, {recursive: true}).catch(() => {});
        await restoreDisc();
        status(storageError(error), true);
    } finally {
        importing = false; $('progress').hidden = true; $('cancel').hidden = true;
        for (const input of [$('iso'), $('folder')]) { input.disabled = false; input.value = ''; }
        document.querySelectorAll('label.button').forEach(el => el.classList.remove('disabled'));
    }
}
async function startGame(quick) {
    if (!engine || !assetDirectory || running) return;
    running = true;
    $('setup').hidden = true; $('canvas').hidden = false; $('loading').hidden = false;
    $('fullscreen').disabled = false; $('reload').hidden = false;
    $('state').textContent = 'Playing';
    $('canvas').focus();
    // Create/resume audio within the launch gesture, before asynchronous reads.
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (Audio) {
        engine.SDL3 = engine.SDL3 || {};
        engine.SDL3.audioContext = engine.SDL3.audioContext || new Audio();
        engine.SDL3.audioContext.resume().catch(() => {});
    }
    const args = ['rr-game', '--assets', '/assets', '--windowed', '--window-scale', '1'];
    if (quick) args.push('--race', '--course', $('course').value);
    const allocated = [];
    try {
        const argv = engine._malloc((args.length + 1) * 4); allocated.push(argv);
        args.forEach((arg, index) => {
            const ptr = engine._malloc(engine.lengthBytesUTF8(arg) + 1); allocated.push(ptr);
            engine.stringToUTF8(arg, ptr, engine.lengthBytesUTF8(arg) + 1);
            new DataView(engine.HEAPU8.buffer).setUint32(argv + index * 4, ptr, true);
        });
        new DataView(engine.HEAPU8.buffer).setUint32(argv + args.length * 4, 0, true);
        const code = await engine.ccall('main', 'number', ['number', 'number'], [args.length, argv], {async: true});
        await sync(false);
        if (code) throw new Error(engine.assetError || 'The game stopped with an error. Re-import your disc if a stored file is missing.');
        $('loading').textContent = 'Game closed. Use Back to launcher to play again.';
        $('loading').hidden = false;
    } catch (error) {
        $('loading').textContent = storageError(error) + ' Use Back to launcher to try again.';
        $('loading').hidden = false;
    } finally { allocated.forEach(ptr => engine._free(ptr)); }
}
$('iso').addEventListener('change', e => e.target.files[0] && importDisc(e.target.files[0], true));
$('folder').addEventListener('change', e => e.target.files.length && importDisc(e.target.files, false));
$('cancel').onclick = () => { cancelled = true; };
$('play').onclick = () => startGame(false);
$('quick').onclick = () => startGame(true);
$('reload').onclick = () => location.reload();
$('fullscreen').onclick = () => $('canvas').requestFullscreen().catch(error => { $('loading').textContent = storageError(error); $('loading').hidden = false; });
// SDL receives these events too; prevent browser scrolling while the game owns focus.
document.addEventListener('visibilitychange', () => {
    if (running) call('rr_web_visibility', null, ['number'], [document.hidden ? 1 : 0]);
});
window.addEventListener('keydown', event => {
    if (running && document.activeElement === $('canvas') && ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', ' ', 'Backspace'].includes(event.key)) event.preventDefault();
});
(async () => {
    try {
        if (!window.isSecureContext || !navigator.storage?.getDirectory)
            throw new Error('This browser needs secure local storage (OPFS). Open this page over HTTPS in a current desktop browser.');
        references = await (await fetch('references.json')).json();
        // SDL's Asyncify presentation and local file reads may yield more than
        // once between draws. Keep the last completed WebGL frame visible.
        const canvas = $('canvas');
        const getContext = canvas.getContext.bind(canvas);
        canvas.getContext = (kind, attributes) => getContext(kind,
            kind.startsWith('webgl') || kind === 'experimental-webgl'
                ? {...attributes, preserveDrawingBuffer: true} : attributes);
        engine = await createRoadRash({
            canvas: $('canvas'), noInitialRun: true,
            print: console.log,
            printErr: message => { console.warn(message); },
            preRun: [module => { module.ENV.RR_PREF_OVERRIDE = '/persist'; }],
            onAbort: reason => { $('loading').textContent = 'Engine error: ' + reason + '. Use Back to launcher to restart.'; $('loading').hidden = false; }
        });
        engine.FS.mkdirTree('/persist');
        engine.FS.mount(engine.FS.filesystems.IDBFS, {autoPersist: true}, '/persist');
        await sync(true);
        storageRoot = await (await navigator.storage.getDirectory()).getDirectoryHandle('road-rash', {create: true});
        engine.localAssets = {
            names: new Set(),
            read: async path => {
                try { return await (await assetDirectory.getFileHandle(encodeURIComponent(path))).getFile(); }
                catch (error) { throw new Error('Stored game file missing: ' + path + '. Re-import your disc.'); }
            }
        };
        status('Engine ready. Select your original PC disc. Allow about 500 MB of browser storage.');
        await restoreDisc();
        for (const input of [$('iso'), $('folder')]) input.disabled = false;
        document.querySelectorAll('label.button').forEach(el => el.classList.remove('disabled'));
        // Hide the DOM loading label once SDL presents its first frame.
        const observer = new MutationObserver(() => { if (running && $('canvas').width > 0) $('loading').hidden = true; });
        observer.observe($('canvas'), {attributes: true, attributeFilter: ['width', 'height']});
    } catch (error) { status(storageError(error), true); }
})();
