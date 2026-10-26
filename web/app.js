'use strict';
// Hosted deployments fetch verified assets on demand; local disc import remains available.
const telemetry = window.rrTelemetry || {track() {}, error() {}, log() {}, gameStarted() {}, gameStopped() {}};
const $ = id => document.getElementById(id);
let engine, storageRoot, assetDirectory, references, importing = false, cancelled = false, running = false;
let loadingStage = '', downloadingAssets = 0;
function updateLoading() {
    $('loading-message').textContent = loadingStage || 'Downloading game data…';
    $('loading').hidden = !running || (!loadingStage && !downloadingAssets);
}
function showGameMessage(message, title = 'Game stopped') {
    $('loading-title').textContent = title;
    $('loading-message').textContent = message;
    document.querySelector('.loading-track').hidden = true;
    $('loading').hidden = false;
}
const status = (message, error = false) => { $('status').textContent = message; $('status').classList.toggle('error', error); };
// The original TEXT/*.DLL font files are TrueType data. Use the same
// verified files as TextRenderer.
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
async function restoreHostedAssets() {
    const response = await fetch('game-assets/manifest.json', {cache: 'no-cache'});
    // An asset-free local build still supports the original disc picker.
    if (response.status === 404) return false;
    if (!response.ok) throw new Error('Game server unavailable. Reload to retry.');
    const manifest = await response.json();
    if (manifest.version !== 1 || !Array.isArray(manifest.files) || manifest.files.length !== references.length)
        throw new Error('Game server manifest is incomplete. Reload to retry.');
    const records = new Map(manifest.files.map(file => [file.path, file]));
    for (const reference of references) {
        const record = records.get(reference.path);
        if (!record || record.hash !== reference.hash || !Number.isSafeInteger(record.size) || record.size < 0)
            throw new Error('Game server files need updating. Reload to retry.');
    }
    const cache = await storageRoot.getDirectoryHandle('hosted-v1', {create: true});
    const pending = new Map(), verified = new Set();
    const digest = async bytes => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)),
        byte => byte.toString(16).padStart(2, '0')).join('');
    const read = async path => {
        const record = records.get(path);
        if (!record) throw new Error('Unknown game file: ' + path);
        if (pending.has(path)) return pending.get(path);
        const request = (async () => {
            try {
                const file = await (await cache.getFileHandle(record.hash)).getFile();
                if (file.size === record.size && (verified.has(record.hash) || await digest(await file.arrayBuffer()) === record.hash)) {
                    verified.add(record.hash);
                    return file;
                }
            } catch (error) {
                if (error.name !== 'NotFoundError') console.warn('Game cache unavailable:', error);
            }
            downloadingAssets++; updateLoading();
            try {
                const result = await fetch('game-assets/' + record.hash);
                if (!result.ok) throw new Error('Game file unavailable: ' + path);
                const bytes = await result.arrayBuffer();
                if (bytes.byteLength !== record.size || await digest(bytes) !== record.hash)
                    throw new Error('Game file failed verification: ' + path);
                // A full/evicted cache must not prevent playing downloaded data.
                try { await writeFile(cache, record.hash, bytes); verified.add(record.hash); }
                catch (error) { console.warn('Game file could not be cached:', error); }
                return new File([bytes], path);
            } finally { downloadingAssets--; updateLoading(); }
        })();
        pending.set(path, request);
        try { return await request; } finally { pending.delete(path); }
    };
    // This directory-shaped adapter also supplies the four original fonts.
    assetDirectory = {getFileHandle: async name => ({getFile: () => read(decodeURIComponent(name))})};
    engine.localAssets = {names: new Set(records.keys()), read};
    document.querySelector('.import-actions').hidden = true;
    $('setup-description').textContent = 'Choose how you want to play.';
    $('state').textContent = 'Game ready';
    try { await loadDiscFonts(assetDirectory); }
    catch (error) { console.warn('Game fonts unavailable:', error); }
    $('cache-note').hidden = false;
    $('ready').hidden = false;
    status('Ready to play.');
    telemetry.track('launcher_ready', {source: 'hosted'});
    return true;
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
        status('Your game is ready to play.');
        telemetry.track('launcher_ready', {source: 'local'});
        return true;
    } catch (error) {
        if (hasMarker || error.name !== 'NotFoundError') status('Stored disc unavailable. Re-import your original disc. ' + storageError(error), true);
        return false;
    }
}
async function importDisc(source, iso) {
    if (!engine || importing || running) return;
    importing = true; cancelled = false;
    telemetry.track('disc_import_start', {source: iso ? 'iso' : 'folder'});
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
        telemetry.track('disc_import_complete');
    } catch (error) {
        if (cancelled) telemetry.track('disc_import_cancelled');
        else telemetry.error(error, 'disc_import');
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
    const started = performance.now();
    const mode = quick ? 'quick_race' : 'main_game';
    telemetry.track('game_start', {mode, ...(quick ? {course: Number($('course').value)} : {})});
    telemetry.gameStarted(mode);
    document.body.classList.add('playing');
    loadingStage = 'Starting game…'; updateLoading();
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
        telemetry.track('game_exit', {mode, duration_ms: Math.round(performance.now() - started)});
        showGameMessage('Use Exit to launcher to play again.', 'Game closed');
    } catch (error) {
        telemetry.error(error, 'game');
        showGameMessage(storageError(error) + ' Use Exit to launcher to try again.');
    } finally { telemetry.gameStopped(); allocated.forEach(ptr => engine._free(ptr)); }
}
$('iso').addEventListener('change', e => e.target.files[0] && importDisc(e.target.files[0], true));
$('folder').addEventListener('change', e => e.target.files.length && importDisc(e.target.files, false));
$('cancel').onclick = () => { cancelled = true; };
$('play').onclick = () => startGame(false);
$('quick').onclick = () => startGame(true);
$('reload').onclick = () => { telemetry.track('launcher_exit'); location.reload(); };
$('fullscreen').onclick = () => document.querySelector('.viewport').requestFullscreen().then(() => telemetry.track('fullscreen')).catch(error => { telemetry.error(error, 'fullscreen'); showGameMessage(storageError(error)); });
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
            printErr: message => { console.warn(message); telemetry.log(message); },
            preRun: [module => { module.ENV.RR_PREF_OVERRIDE = '/persist'; }],
            onAbort: reason => { telemetry.error(new Error(String(reason)), 'engine_abort'); showGameMessage('Engine error: ' + reason + '. Use Exit to launcher to restart.'); }
        });
        engine.webLoading = stage => { loadingStage = stage; updateLoading(); };
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
        status('Connecting to game server…');
        if (!await restoreHostedAssets()) {
            document.querySelector('.import-actions').hidden = false;
            $('setup-description').textContent = 'Choose your original PC disc image (.iso) or the ROADRASH folder. Game files stay on this device.';
            status('Engine ready. Select your original PC disc. Allow about 500 MB of browser storage.');
            await restoreDisc();
        }
        for (const input of [$('iso'), $('folder')]) input.disabled = false;
        document.querySelectorAll('label.button').forEach(el => el.classList.remove('disabled'));
    } catch (error) { telemetry.error(error, 'initialization'); status(storageError(error), true); $('retry').hidden = false; }
})();
$('retry').onclick = () => { telemetry.track('connection_retry'); location.reload(); };
