// Exercise the real bundled SDKs without sending synthetic events to either service.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), zlib = require('node:zlib');
const dist = path.resolve(process.env.RR_WEB_DIST || 'build-agent-browser/dist');
const script = fs.readFileSync(path.join(dist, fs.readdirSync(dist).find(n => /^telemetry\.[a-f0-9]{16}\.js$/.test(n))), 'utf8');
(async () => {
    const browser = await chromium.launch({headless:true});
    try {
        const context = await browser.newContext({userAgent:'Mozilla/5.0 Chrome/151.0.0.0 Safari/537.36'});
        // PostHog deliberately drops webdriver traffic; simulate a human for payload assertions.
        await context.addInitScript(() => {
            Object.defineProperty(navigator, 'webdriver', {get:()=>false});
            Object.defineProperty(navigator, 'userAgentData', {get:()=>undefined});
        });
        const envelopes = [], analytics = [], errors = [], external = [];
        await context.route('**/*', async route => {
            const req = route.request(), url = new URL(req.url());
            if (url.hostname === 'o425745.ingest.us.sentry.io') {
                envelopes.push(req.postData()); return route.fulfill({json:{}});
            }
            if (url.hostname === 'us.i.posthog.com') {
                let body = req.postDataBuffer();
                if (body?.[0] === 0x1f) body = zlib.gunzipSync(body);
                if (body) {
                    const text = body.toString();
                    try { const parsed=JSON.parse(text); analytics.push(...[].concat(parsed.batch || parsed)); }
                    catch (_) { throw new Error('Unexpected PostHog encoding: ' + text.slice(0,100)); }
                }
                return route.fulfill({json:{status:1}});
            }
            if (!['road-rash.archit.xyz', 'localhost'].includes(url.hostname)) external.push(req.url());
            if (url.pathname === '/telemetry.js') return route.fulfill({contentType:'application/javascript',body:script});
            return route.fulfill({contentType:'text/html',headers:{'Content-Security-Policy':"default-src 'self'; script-src 'self'; connect-src 'self' https://o425745.ingest.us.sentry.io https://us.i.posthog.com"}, body:'<script src="/telemetry.js"></script><h1>Telemetry fixture</h1>'});
        });
        const page = await context.newPage();
        page.on('pageerror', e => errors.push(e.message));
        await page.goto('https://road-rash.archit.xyz/?private=test-secret#private');
        await page.evaluate(() => {
            rrTelemetry.track('game_start', {mode:'quick_race', course:0, filename:'secret.iso', save:'secret-save'});
            rrTelemetry.error(new Error('Telemetry fixture handled failure'), 'engine_abort');
            setTimeout(() => { throw new Error('Telemetry fixture uncaught failure'); }, 0);
        });
        await page.waitForFunction(() => true);
        // SDK queues flush in the background; wait on captured transport, not real services.
        await assertEventually(() => envelopes.some(x=>x.includes('uncaught failure')) && analytics.some(x=>x.event==='game_start'));
        assert.ok(envelopes.some(x=>x.includes('handled failure') && x.includes('engine_abort')));
        const start = analytics.find(x=>x.event==='game_start');
        assert.equal(start.properties.course,0);
        assert.equal(start.properties.filename,undefined);
        assert.equal(start.properties.save,undefined);
        assert.ok(analytics.some(x=>x.event==='$pageview'));
        assert.ok(!JSON.stringify(analytics).includes('test-secret'));
        assert.ok(!envelopes.join('').includes('test-secret'));
        assert.deepEqual(external,[],'no external SDK or replay loads');
        assert.deepEqual(errors,['Telemetry fixture uncaught failure']);
        const before = [envelopes.length,analytics.length];
        await page.goto('http://localhost/');
        await page.evaluate(() => {rrTelemetry.track('game_start');rrTelemetry.error(new Error('local'), 'game');});
        assert.deepEqual([envelopes.length,analytics.length],before,'local development does not send telemetry');
        // A browser which blocks both services must still run launcher hooks.
        await context.route('https://us.i.posthog.com/**', r=>r.abort());
        await context.route('https://o425745.ingest.us.sentry.io/**', r=>r.abort());
        await page.goto('https://road-rash.archit.xyz/');
        assert.equal(await page.evaluate(() => {rrTelemetry.track('launcher_ready');rrTelemetry.error(new Error('blocked'), 'game');return true;}),true);
        console.log('PASS real SDK transports, handled/uncaught errors, event properties, privacy, CSP, localhost and blocked services');
    } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
async function assertEventually(check) {
    const deadline=Date.now()+15000;
    while (!check()) { if (Date.now()>deadline) throw new Error('SDK transport did not flush'); await new Promise(r=>setTimeout(r,100)); }
}
