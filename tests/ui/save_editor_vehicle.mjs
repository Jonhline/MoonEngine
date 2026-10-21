import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import http from 'node:http';
import {createRequire} from 'node:module';
import {execFileSync} from 'node:child_process';
import * as model from '../../tools/save_editor/ui/friendly.js';
import * as fields from '../../tools/save_editor/ui/vehicle_fields.js';
const require=createRequire(import.meta.url),{chromium}=require('playwright');
const [editor,helper,disc,fixture,catalogPath,capture,...flags]=process.argv.slice(2);
const catalog=JSON.parse(fs.readFileSync(catalogPath,'utf8').replace(/^\uFEFF/,''));
const original=JSON.parse(execFileSync(editor,['--inspect',fixture],{encoding:'utf8'}));
const temp=fs.mkdtempSync(path.join(os.tmpdir(),'moon-vehicle-check-'));let sequence=0;
function operation({root,action,args}){
 const id=sequence++,json=path.join(temp,`${id}.json`),input=path.join(temp,`${id}.ogt4`),output=path.join(temp,`${id}-out.ogt4`);
 fs.writeFileSync(json,JSON.stringify(root));execFileSync(editor,['--apply-json',json,input]);
 const stdout=execFileSync(helper,[disc,input,output,action,...args],{encoding:'utf8',stdio:'pipe',env:{...process.env,SDL_AUDIODRIVER:'dummy'}});
 const stats=stdout.trim()?JSON.parse(stdout):{};
 const next=structuredClone(root);if(action!=='stats'){const saved=JSON.parse(execFileSync(editor,['--inspect',output],{encoding:'utf8'}));model.set(next,'garage',model.get(saved,'garage'));}
 return {root:next,stats};
}
// Every supported primitive: exact endian, signed limits, 64-bit IDs, no silent overflow.
for(const [type,value,size]of [['u8','255',1],['i8','-128',1],['u16','65535',2],['i16','-32768',2],['u32','4294967295',4],['i32','-2147483648',4],['u64','18446744073709551615',8],['f32','1.25',4]]){
 const bytes=fields.write(Array(0x4b8).fill(0),1,type,value);assert.equal(String(fields.read(bytes,1,type)),value);assert.equal(bytes[0],0);assert.equal(bytes[1+size],0);
}
assert.deepEqual(fields.write([0,0],0,'u16','4660'),[0x34,0x12]);
for(const [type,value]of [['u8','256'],['i8','128'],['u64','18446744073709551616'],['f32','Infinity'],['f32','1e100']])assert.throws(()=>fields.write(Array(8).fill(0),0,type,value));
assert.throws(()=>fields.read([0,0],0.5,'u8'));assert.throws(()=>fields.write([0],0,'u16','1'));
const noLicenses=structuredClone(original);model.set(noLicenses,'race_record',model.module());model.set(noLicenses,'license_record',model.module());
for(const exam of catalog.licenses)model.editExam(noLicenses,exam,9,'');
assert(catalog.licenses.every(e=>model.examState(noLicenses,e).grade===9),'Missing records must be created');
const before=JSON.stringify(original);assert.throws(()=>operation({root:original,action:'add',args:['invalid_model','0']}));assert.equal(JSON.stringify(original),before);
assert.throws(()=>operation({root:original,action:'tune',args:['0','1000000','900']}));assert.equal(JSON.stringify(original),before);
const server=http.createServer((req,res)=>{const name=req.url==='/'?'index.html':req.url.slice(1);if(!['index.html','sdk.css','editor.css','editor.js','friendly.js','vehicle_fields.js'].includes(name)){res.writeHead(404).end();return;}res.setHeader('Content-Type',name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':'text/html');res.end(fs.readFileSync(path.resolve('tools/save_editor/ui',name)));});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const browser=await chromium.launch({headless:true,channel:'msedge'});
try{
 const page=await browser.newPage({viewport:{width:1440,height:1000}}),errors=[];page.on('pageerror',e=>errors.push(String(e)));page.on('dialog',d=>d.accept());
 await page.exposeBinding('nativeVehicleTest',(_,args)=>operation(args));
 await page.addInitScript(({root,catalog})=>{window.edits=[];window.__TAURI__={core:{invoke:async(cmd,args)=>{if(cmd==='saved_catalog')return catalog;if(cmd==='open_profile')return{root,path:'Native operation test profile'};if(cmd==='validate_profile'){window.edits.push(args.root);return null;}if(cmd==='vehicle_action')return window.nativeVehicleTest(args);}}};},{root:original,catalog});
 await page.goto(`http://127.0.0.1:${server.address().port}`);await page.getByRole('button',{name:'Open profile…',exact:true}).click();await page.getByRole('button',{name:'Garage',exact:true}).click();
 await page.getByRole('button',{name:'Add vehicle…',exact:true}).click();await page.getByLabel('Search vehicle models').fill('rx7_rs_00');await page.getByLabel('Vehicle model',{exact:true}).selectOption('rx7_rs_00');await page.getByRole('button',{name:'Add vehicle',exact:true}).click();
 await page.waitForFunction(()=>window.edits.length>=1);let saved=await page.evaluate(()=>window.edits.at(-1));assert.equal(model.cars(saved,catalog).length,2);assert.equal(model.cars(saved,catalog)[1].body.length,0x4b8);assert(model.review(original,saved,catalog).some(c=>c.before==='Empty'&&c.after.includes('RX-7')));assert.deepEqual(model.get(saved,'money'),model.get(original,'money'));assert.deepEqual(model.get(saved,'calendar'),model.get(original,'calendar'));
 await page.getByRole('button',{name:'Use this car',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=2);saved=await page.evaluate(()=>window.edits.at(-1));assert.equal(model.get(model.get(saved,'garage'),'riding_index').value,'1');
 await page.getByRole('button',{name:'Advanced vehicle parameters',exact:true}).click();await page.getByLabel('Target power (cv)',{exact:true}).fill('400');await page.getByLabel('Target mass (kg)',{exact:true}).fill('1000');await page.getByRole('button',{name:'Apply power / mass',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=3);saved=await page.evaluate(()=>window.edits.at(-1));let car=model.cars(saved,catalog)[1];const stats=operation({root:saved,action:'stats',args:['1']}).stats;assert(Math.abs(stats.power_cv-400)<3);assert(Math.abs(stats.weight_kg-1000)<15);assert.equal(Number(model.get(car.car,'power').value),Math.round(stats.power_cv*100));assert.equal(Number(model.get(car.car,'weight').value),stats.weight_kg);assert.deepEqual(model.bytes(model.get(model.get(saved,'garage'),'current_car_ps2')),car.body);const compactFlags=BigInt.asUintN(64,BigInt(model.get(car.car,'flags').value));assert.equal(Number((compactFlags>>9n)&0x3ffffn),Math.round(stats.power_cv*100));assert.equal(Number((compactFlags>>32n)&8191n),stats.weight_kg);assert.equal(car.body[car.setup*0x178+0x163],stats.power_percent);assert.equal(car.body[car.setup*0x178+0x162],stats.weight_percent);
 await page.getByLabel('Target power (cv)',{exact:true}).fill('1000000');await page.getByRole('button',{name:'Apply power / mass',exact:true}).click();await page.getByRole('alert').waitFor();assert.match(await page.getByRole('alert').textContent(),/supports.*cv/);assert.deepEqual(await page.evaluate(()=>window.edits.at(-1)),saved,'Rejected tuning leaves the last applied document unchanged');
 await page.getByText('All setup parameters',{exact:true}).click();await page.getByLabel(/Traction control 1 ·/).fill('7');await page.getByRole('button',{name:'Apply setup parameters',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=4);saved=await page.evaluate(()=>window.edits.at(-1));car=model.cars(saved,catalog)[1];assert.equal(car.body[car.setup*0x178+0x156],7);
 await page.getByText('Every stored car byte / part key',{exact:true}).click();await page.screenshot({path:capture,fullPage:true});
 await page.getByRole('button',{name:'Remove vehicle',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=5);saved=await page.evaluate(()=>window.edits.at(-1));assert.equal(model.cars(saved,catalog).length,1);assert.equal(model.get(model.get(saved,'garage'),'riding_index').value,'-1');
 await page.getByRole('button',{name:'Undo',exact:true}).click();assert.equal(await page.getByLabel('Target power (cv)',{exact:true}).count(),1);
 await page.getByRole('button',{name:'Licenses',exact:true}).click();await page.getByRole('button',{name:'Grant B license',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=6);saved=await page.evaluate(()=>window.edits.at(-1));assert(catalog.licenses.filter(e=>e.group==='B').every(e=>model.examState(saved,e).grade===9));
 await page.getByRole('button',{name:'Remove B license',exact:true}).click();await page.waitForFunction(()=>window.edits.length>=7);saved=await page.evaluate(()=>window.edits.at(-1));assert(catalog.licenses.filter(e=>e.group==='B').every(e=>!model.examState(saved,e).completed));
 await page.getByRole('button',{name:'Advanced',exact:true}).click();await page.getByRole('button',{name:'Raw saved data',exact:true}).click();await page.locator('.row[title="garage"]').click();await page.getByRole('button',{name:'Add field / entry…',exact:true}).click();const dialog=page.getByRole('dialog');await dialog.getByLabel('Field name',{exact:true}).fill('debug_marker');await dialog.getByLabel('Initial value',{exact:true}).fill('9223372036854775807');await dialog.getByRole('button',{name:'Add',exact:true}).click();await page.waitForFunction(()=>window.edits.at(-1).value.find(([k])=>k==='garage')[1].value.some(([k])=>k==='debug_marker'));saved=await page.evaluate(()=>window.edits.at(-1));assert.equal(model.get(model.get(saved,'garage'),'debug_marker').value,'9223372036854775807');
 await page.locator('.row[title="garage.debug_marker"]').click();await page.getByRole('button',{name:'Remove field / entry',exact:true}).click();await page.waitForFunction(()=>!window.edits.at(-1).value.find(([k])=>k==='garage')[1].value.some(([k])=>k==='debug_marker'));saved=await page.evaluate(()=>window.edits.at(-1));assert.equal(model.get(model.get(saved,'garage'),'debug_marker'),undefined);
 for(const label of ['cappuccino_91','cappuccino_95']){
  let doc=operation({root:original,action:'add',args:[label,'0']}).root;
  doc=operation({root:doc,action:'select',args:['1']}).root;
  const cp=await browser.newPage();await cp.exposeBinding('nativeVehicleTest',(_,args)=>operation(args));
  await cp.addInitScript(({root,catalog})=>{window.applied=null;window.__TAURI__={core:{invoke:async(cmd,args)=>{if(cmd==='saved_catalog')return catalog;if(cmd==='open_profile')return{root,path:'Cappuccino regression'};if(cmd==='validate_profile'){window.applied=args.root;return null;}if(cmd==='vehicle_action')return window.nativeVehicleTest(args);}}};},{root:doc,catalog});
  await cp.goto(`http://127.0.0.1:${server.address().port}`);await cp.getByRole('button',{name:'Open profile…',exact:true}).click();await cp.getByRole('button',{name:'Garage',exact:true}).click();await cp.locator('.vehicle').nth(1).click();await cp.getByRole('button',{name:'Advanced vehicle parameters',exact:true}).click();await cp.getByLabel('Target power (cv)',{exact:true}).fill('160');await cp.getByLabel('Target mass (kg)',{exact:true}).fill('7');await cp.getByRole('button',{name:'Apply power / mass',exact:true}).click();await cp.waitForFunction(()=>window.applied!==null);
  const changed=await cp.evaluate(()=>window.applied),actual=operation({root:changed,action:'stats',args:['1']}).stats;assert(Math.abs(actual.power_cv-160)<1);assert.equal(actual.weight_kg,label==='cappuccino_91'?7:6);assert.match(await cp.locator('#status').textContent(),/Applied 159|Applied 160/);await cp.close();
 }
 assert.deepEqual(errors,[]);if(flags.includes('--negative'))assert.equal(stats.weight_kg,1,'Deliberate wrong native mass must fail');
 console.log('PASS native vehicle creation, selection, deletion, real power/mass, setup fields, license grant/remove, missing license records, typed byte editor and rejection checks');
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
