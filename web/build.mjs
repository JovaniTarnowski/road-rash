import {build} from 'esbuild';
import {execFileSync} from 'node:child_process';
await build({entryPoints: ['telemetry.js'], bundle: true, minify: true, format: 'iife',
    outfile: process.env.RR_TELEMETRY_OUTPUT,
    legalComments: 'inline',
    define: {RR_WEB_RELEASE: JSON.stringify('road-rash@' + execFileSync('git', ['rev-parse', 'HEAD'], {encoding:'utf8'}).trim())}});

// Ship the SDK dependency notices alongside the existing SDL/codec notices.
const {readFileSync, readdirSync, appendFileSync} = await import('node:fs');
const {join, dirname} = await import('node:path');
const lock = JSON.parse(readFileSync('package-lock.json', 'utf8'));
for (const [directory, pkg] of Object.entries(lock.packages)) {
    if (!directory || pkg.dev) continue;
    for (const name of readdirSync(directory).filter(name => /^licen[cs]e(?:\.|$)/i.test(name))) {
        appendFileSync(join(dirname(process.env.RR_TELEMETRY_OUTPUT), 'LICENSES.txt'),
            `\n\n--- ${directory.replace('node_modules/', '')} ${pkg.version} ---\n\n` + readFileSync(join(directory, name), 'utf8'));
    }
}
