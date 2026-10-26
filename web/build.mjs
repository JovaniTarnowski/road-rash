import {build} from 'esbuild';
import {execFileSync} from 'node:child_process';
await build({entryPoints: ['telemetry.js'], bundle: true, minify: true, format: 'iife',
    outfile: process.env.RR_TELEMETRY_OUTPUT,
    legalComments: 'inline',
    define: {RR_WEB_RELEASE: JSON.stringify('road-rash@' + execFileSync('git', ['rev-parse', 'HEAD'], {encoding:'utf8'}).trim())}});
