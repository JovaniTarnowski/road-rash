// A browser with the old fixed-name loader cached must boot the new release.
const {chromium}=require('playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const assert=require('node:assert/strict');
const dist=path.resolve(process.env.RR_WEB_DIST || 'build-agent-browser/dist');
let current=false;
const server=http.createServer((req,res)=>{
    const name=new URL(req.url,'http://localhost').pathname.slice(1);
    if(!current){
        res.setHeader('Content-Type',name==='rr-game.js'?'application/javascript':'text/html');
        res.setHeader('Cache-Control',name==='rr-game.js'?'public,max-age=14400':'no-store');
        res.end(name==='rr-game.js'?'window.staleEngine=true;':'<script src="rr-game.js"></script>');return;
    }
    const file=path.join(dist,name || 'index.html');
    if(!file.startsWith(dist+path.sep)||!fs.existsSync(file)||!fs.statSync(file).isFile()){res.writeHead(404);res.end();return;}
    res.setHeader('Content-Type',({'.html':'text/html','.js':'application/javascript','.wasm':'application/wasm','.css':'text/css','.json':'application/json'})[path.extname(file)]||'text/plain');
    res.setHeader('Cache-Control',name.endsWith('.html')||!name?'no-store':'public,max-age=31536000,immutable');
    fs.createReadStream(file).pipe(res);
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({headless:true});
 try{
    const page=await browser.newPage(),errors=[],requests=[];
    page.on('pageerror',e=>errors.push(e.message));
    const url='http://127.0.0.1:'+server.address().port;
    await page.goto(url);
    assert.equal(await page.evaluate(()=>window.staleEngine),true);
    current=true;
    page.on('request',r=>requests.push(new URL(r.url()).pathname));
    await page.reload();
    await page.waitForFunction(()=>document.querySelector('#status')?.textContent.includes('Engine ready'),null,{timeout:30000});
    assert.equal(await page.evaluate(()=>window.staleEngine),undefined);
    assert.ok(requests.some(p=>/^\/rr-game\.[0-9a-f]{16}\.js$/.test(p)));
    assert.ok(requests.some(p=>/^\/rr-game\.[0-9a-f]{16}\.wasm$/.test(p)));
    assert.ok(!requests.includes('/rr-game.js')&&!requests.includes('/rr-game.wasm'));
    assert.deepEqual(errors,[]);
    console.log('PASS stale cached loader cannot contaminate fingerprinted release; Wasm instantiated');
 }finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);server.close();process.exitCode=1;});
