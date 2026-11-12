/* Frontend integration using the native bridge's real save fixture. */
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {execFileSync}=require('node:child_process');
const {chromium}=require('playwright');
(async()=>{
 const editor=path.resolve(process.argv[2]), fixture=path.resolve(process.argv[3]), capture=path.resolve(process.argv[4]);
 const root=JSON.parse(execFileSync(editor,['--inspect',fixture],{encoding:'utf8'}));
 const http=require('node:http');
 const server=http.createServer((req,res)=>{
  const name=req.url==='/'?'index.html':req.url.slice(1);
  if(!['index.html','sdk.css','editor.css','editor.js','friendly.js','vehicle_fields.js'].includes(name)){res.writeHead(404).end();return;}
  res.setHeader('Content-Type',name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':'text/html');
  res.end(fs.readFileSync(path.resolve('tools/save_editor/ui',name)));
 });
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({headless:true,channel:process.env.MOON_TEST_BROWSER||'msedge'});
 try {
  const page=await browser.newPage({viewport:{width:1280,height:800}}),errors=[];
  page.on('pageerror',e=>errors.push(String(e)));
  await page.addInitScript(({root,fixture})=>{
   window.calls=[];window.__TAURI__={core:{invoke:async(cmd,args)=>{
    window.calls.push({cmd,args});
    if(cmd==='open_profile')return {root,path:fixture,timestamp:0};
    if(cmd==='validate_profile'){
     const money=args.root.value.find(([k])=>k==='money')[1];
     if(!/^-?\d+$/.test(money.value))throw 'Enter a signed 64-bit integer';
     return null;
    }
    if(cmd==='save_copy')return 'Saved a new copy; original backed up';
   }}};
  },{root,fixture});
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.getByRole('button',{name:'Open profile…',exact:true}).click();
  await page.getByRole('button',{name:'Advanced',exact:true}).click();
  await page.getByRole('button',{name:/Credits.*9007199254740993/}).click();
  await page.locator('#edit-value').fill('invalid');await page.getByRole('button',{name:'Apply',exact:true}).click();
  await assert.equal(await page.locator('#status').textContent(),'Enter a signed 64-bit integer');
  assert.equal(await page.locator('#save').isDisabled(),true);
  await page.locator('#edit-value').fill('12345678901234567');await page.getByRole('button',{name:'Apply',exact:true}).click();
  await page.waitForFunction(()=>!document.getElementById('save').disabled);
  await page.getByRole('button',{name:'Undo',exact:true}).click();assert.equal(await page.locator('#save').isDisabled(),true);
  await page.getByRole('button',{name:'Redo',exact:true}).click();assert.equal(await page.locator('#save').isEnabled(),true);
  await page.locator('#save').click();assert.match(await page.locator('#changes').textContent(),/9007199254740993.*12345678901234567/);
  await page.getByRole('button',{name:'Keep editing',exact:true}).click();
  await page.locator('#save').click();await page.locator('#confirm-save').click();
  const saved=await page.evaluate(()=>window.calls.findLast(c=>c.cmd==='save_copy').args.root);
  assert.equal(saved.value.find(([k])=>k==='money')[1].value,'12345678901234567');
  assert.deepEqual(saved.value.find(([k])=>k==='garage'),root.value.find(([k])=>k==='garage'));
  await page.getByRole('button',{name:'Advanced',exact:true}).click();await page.locator('#search').fill('unknown_nested');
  assert.ok(await page.locator('.row').count()>0);
  await page.locator('#search').fill('<img src=x onerror=alert(1)>');assert.equal(await page.locator('.row').count(),0);
  await page.locator('#search').fill('');
  await page.getByRole('button',{name:/Credits.*12345678901234567/}).click();
  await page.screenshot({path:capture});assert.deepEqual(errors,[]);
  console.log('PASS editor frontend: validation, exact integers, undo/redo, review, copy, unknown fields, safe search');
 } finally {await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);process.exit(1);});
