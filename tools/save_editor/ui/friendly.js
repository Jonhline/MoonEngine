// Presentation/edit boundary for the native saved data. No game logic runs here.
// Sources: gtmode_game.c, career_original.c, career_license.c, menu_car.inc.
export const integer = value => ({kind:'int',value:String(value)});
export const module = () => ({kind:'module',value:[]});
export const array = () => ({kind:'array',value:[]});
export function get(node,key){return node?.kind==='module'?node.value.find(([k])=>k===key)?.[1]:undefined;}
export function set(node,key,value){if(node?.kind!=='module')throw Error('This profile section is not a saved object.');const pair=node.value.find(([k])=>k===key);if(pair)pair[1]=value;else node.value.push([key,value]);return value;}
function ensure(node,key,kind='module'){const n=get(node,key);if(n&&n.kind!==kind)throw Error(`Unsupported ${key} layout. Use Advanced to inspect it.`);return n||set(node,key,kind==='module'?module():array());}
const val=(n,otherwise=0)=>n?.value??otherwise;
export function bytes(n){if(n?.kind!=='array')return null;if(n.value.length&&n.value.every(x=>x.kind==='string')){try{return [...atob(n.value.map(x=>x.value).join(''))].map(c=>c.charCodeAt(0));}catch{return null;}}if(n.value.some(x=>x.kind!=='int'||!/^\d+$/.test(x.value)||BigInt(x.value)>255n))return null;return n.value.map(x=>Number(x.value));}
const rawBlob=b=>({kind:'array',value:b.map(integer)});
const blob=b=>({kind:'array',value:[{kind:'string',value:btoa(b.map(x=>String.fromCharCode(x)).join(''))}]});
export function setBody(root,index,body){const car=cars(root,null).find(c=>c.index===index)?.car;if(!car||body.length!==0x4b8)throw Error('Invalid complete car record.');set(car,'full_car_ps2',blob(body));set(car,'oil_state',rawBlob(body.slice(0x498)));set(car,'odometer',integer(read32(body,0x498)));const garage=get(root,'garage');if(Number(get(garage,'riding_index')?.value)===index){set(garage,'current_car_ps2',blob(body));const cached=get(garage,'riding_car');if(cached?.kind==='module'){set(cached,'full_car_ps2',blob(body));set(cached,'oil_state',rawBlob(body.slice(0x498)));}}return root;}
export const read32=(b,i)=>b[i]+b[i+1]*256+b[i+2]*65536+b[i+3]*16777216;
function write32(b,i,n){for(let j=0;j<4;j++)b[i+j]=Number((BigInt(n)>>BigInt(8*j))&255n);}
function number(text,min,max,title){if(!/^-?\d+$/.test(String(text)))throw Error(`${title}: enter a whole number.`);const n=BigInt(text);if(n<BigInt(min)||n>BigInt(max))throw Error(`${title}: use ${min}–${max}.`);return Number(n);}
export function mileage(text){if(!/^\d+(\.\d{1,3})?$/.test(String(text)))throw Error('Mileage: enter kilometres with up to 3 decimal places.');const [whole,frac='']=String(text).split('.');return number(BigInt(whole)*1000n+BigInt(frac.padEnd(3,'0')),0,1600000000,'Mileage (metres)');}
export function cars(root,catalog){const garage=get(root,'garage'), list=get(garage,'cars');if(list?.kind!=='array')return [];return list.value.flatMap((car,index)=>{if(car.kind!=='module')return [];const label=get(car,'car_code')?.value,code=get(car,'code');const meta=catalog?.cars?.find(c=>c.label===label||(code&&BigInt(c.id)===BigInt.asUintN(32,BigInt(code.value))));const body=bytes(get(car,'full_car_ps2')),wear=bytes(get(car,'oil_state'));let meter=Number(val(get(car,'odometer')));if(wear?.length===32)meter=read32(wear,0);else if(body?.length===0x4b8)meter=read32(body,0x498);return [{index,car,meta,name:meta?.name||get(car,'name')?.value||label||`Vehicle ${index+1}`,meter,body,paint:Number(val(get(car,'color_index'))),setup:body?.length===0x4b8?read32(body,0x488):0}];});}
export const aids=[['Traction control',0x15e, false],['ASM oversteer',0x15f,true],['ASM understeer',0x161,true],['Front brake balance',0x137,false],['Rear brake balance',0x138,false]];
export function editCar(root,index,values,catalog){
 const info=cars(root,catalog).find(c=>c.index===index);if(!info)throw Error('That garage slot is empty.');
 const meters=mileage(values.km), setup=number(values.setup,0,2,'Active setup');
 const paint=number(values.paint,-1,127,'Paint');if(paint!==info.paint&&(!info.meta||!info.meta.paints.some(p=>p.index===paint)))throw Error('Load the disc catalogue and choose an available factory paint.');
 const car=info.car,body=info.body?.slice();let wear=bytes(get(car,'oil_state'));
 if(!body||body.length!==0x4b8)throw Error('This vehicle has no supported full car record. Its raw data is available in Advanced.');
 if(!wear||wear.length!==32)wear=body.slice(0x498,0x4b8);
 write32(wear,0,meters);body.splice(0x498,32,...wear);write32(body,0x488,setup);
 if(paint!==info.paint)for(let s=0;s<3;s++)body[s*0x178+0x18]=paint&255;
 if(values.aids)for(const [name,offset,paired] of aids){const n=number(values.aids[name],0,255,name);body[setup*0x178+offset-8]=n;if(paired)body[setup*0x178+offset-7]=n;}
 set(car,'odometer',integer(meters));set(car,'oil_state',rawBlob(wear));set(car,'full_car_ps2',blob(body));set(car,'color_index',integer(paint));
 const flags=get(car,'flags');if(paint!==info.paint&&flags?.kind==='int'){let bits=BigInt.asUintN(64,BigInt(flags.value));bits=(bits&~(127n<<2n))|(BigInt(paint&127)<<2n);set(car,'flags',integer(BigInt.asIntN(64,bits)));set(car,'variation',integer(paint&127));}
 const compact=bytes(get(car,'compact_unit_ps2'));if(compact?.length===32){write32(compact,20,meters);const f=get(car,'flags');if(f)for(let i=0;i<8;i++)compact[8+i]=Number((BigInt.asUintN(64,BigInt(f.value))>>BigInt(i*8))&255n);set(car,'compact_unit_ps2',blob(compact));}
 const garage=get(root,'garage'),slot=Number(val(get(car,'slot'),val(get(car,'garage_index'),index))),riding=Number(val(get(garage,'riding_index'),-1));
 if(riding===index||riding===slot){set(garage,'current_car_ps2',blob(body));const cached=get(garage,'riding_car');if(cached?.kind==='module')for(const key of ['odometer','oil_state','full_car_ps2','color_index','flags','variation','compact_unit_ps2']){const v=get(car,key);if(v)set(cached,key,structuredClone(v));}}
 const units=bytes(get(garage,'original_units_ps2'));if(units?.length===32000&&compact?.length===32&&slot>=0&&slot<1000){units.splice(slot*32,32,...compact);set(garage,'original_units_ps2',blob(units));}
 return root;
}
export const grades=[['Not completed',15],['Bronze',11],['Silver',10],['Gold',9]];
export function examState(root,exam){const race=get(root,'race_record'),indexed=get(race,'records'),cached=get(get(race,'units'),exam.code);const record=indexed?.kind==='array'&&indexed.value[exam.record]?.kind==='module'?indexed.value[exam.record]:cached;let grade=15;const raw=get(record,'original_record');if(raw?.kind==='int')grade=Number((BigInt(raw.value)>>4n)&15n);else{const name=get(record,'result')?.value;grade=({gold:9,silver:10,bronze:11,red:12})[name]??15;}const ranks=get(get(root,'license_record'),'records');const attempt=ranks?.kind==='array'?ranks.value[exam.ranking]?.value?.[0]:null;const ms=Number(val(get(attempt,'time'),0x157529ff));return {grade,ms,completed:grade!==15&&grade<=12};}
export function timeText(ms){if(ms===0x157529ff)return '';return `${Math.floor(ms/60000)}:${String(Math.floor(ms/1000)%60).padStart(2,'0')}.${String(ms%1000).padStart(3,'0')}`;}
export function timeValue(s){const m=/^(?:(\d+):)?(\d{1,2})\.(\d{1,3})$/.exec(s);if(!m||Number(m[2])>=60)throw Error('Time: use m:ss.mmm, for example 0:32.500.');const n=Number(m[1]||0)*60000+Number(m[2])*1000+Number(m[3].padEnd(3,'0'));if(n<=0||n>=0x157529ff)throw Error('Time is outside the saved ranking range.');return n;}
export function editExam(root,exam,grade,time){
 const region=get(get(root,'ps2_import'),'region');if(region&&Number(region.value)!==1)throw Error('Guided license editing currently supports US profiles only. Other regions remain available in Advanced.');
 if(!Number.isInteger(exam.record)||exam.record<0||exam.record>=768||!Number.isInteger(exam.ranking)||exam.ranking<0||exam.ranking>=125)throw Error('Load a valid disc catalogue before editing licenses.');
 grade=number(grade,9,15,'Medal');if(![9,10,11,15].includes(grade))throw Error('Choose a listed medal.');
 let ms=0x157529ff;if(grade!==15){ms=time?timeValue(time):exam.targets?.[grade-9];if(!ms||ms===0x157529ff)throw Error('Enter a valid best time.');
 const target=exam.targets?.[grade-9],faster=grade>9?exam.targets?.[grade-10]:0;
 if(target&&ms>target)throw Error('The time is slower than the selected medal target.');if(faster&&ms<=faster)throw Error('That time earns a higher medal. Choose the matching medal.');}
 const race=ensure(root,'race_record'),records=ensure(race,'records','array'),named=get(race,'units');let record;
 if(!records.value.length&&named?.kind==='module'&&named.value.length){const existing=named.value.find(([code])=>code===exam.code||code.endsWith(exam.code));record=existing?.[1]||set(named,exam.code,module());}
 else{while(records.value.length<=exam.record)records.value.push({kind:'nil'});record=records.value[exam.record];if(record.kind==='nil')record=records.value[exam.record]=module();}
 if(record.kind!=='module')throw Error('Unsupported race record.');
 const old=BigInt.asUintN(32,BigInt(val(get(record,'original_record'),65520)));let word=(old&~0xfffn)|1n|(BigInt(grade)<<4n)|(BigInt(grade)<<8n);
 if(grade===15)word=(word&~15n);set(record,'original_record',integer(word));
 if(get(record,'bits'))set(record,'bits',integer(word&65535n));
 const name=grades.find(g=>g[1]===grade)[0].toLowerCase();if(get(record,'result'))set(record,'result',{kind:'string',value:grade===15?'NONE':name});
 const units=get(race,'units');if(units?.kind==='module')for(const [code,unit] of units.value){if(code===exam.code||code.endsWith(exam.code)){const raw=get(unit,'original_record');if(raw)set(unit,'original_record',integer(word));if(get(unit,'result'))set(unit,'result',{kind:'string',value:grade===15?'NONE':name});}}
 const licenses=ensure(root,'license_record'),all=ensure(licenses,'records','array');while(all.value.length<=exam.ranking)all.value.push(array());let attempts=all.value[exam.ranking];if(attempts.kind==='nil')attempts=all.value[exam.ranking]=array();if(attempts.kind!=='array')throw Error('Unsupported license ranking.');
 // Preserve attempts/history on a medal edit; adjust the best entry only and
 // retain the ten-entry shape. Clearing a test explicitly clears its ranking.
 const entry=attempts.value[0]?.kind==='module'?structuredClone(attempts.value[0]):module();set(entry,'time',integer(ms));set(entry,'result',integer(grade));if(!get(entry,'date')){const today=get(get(root,'calendar'),'today');const d=today?.kind==='array'?today.value.map(n=>Number(n.value)):[];set(entry,'date',integer(d.length===3?BigInt(d[0])|(BigInt(d[1])<<16n)|(BigInt(d[2])<<24n):0));}
 const retained=grade===15?[]:attempts.value.slice(1).filter(a=>Number(val(get(a,'time'),0x157529ff))>=ms);
 attempts.value=[entry,...retained].slice(0,10);while(attempts.value.length<10){const empty=module();set(empty,'time',integer(0x157529ff));attempts.value.push(empty);}
 return root;
}
export function review(before,after,catalog){
 const out=[],add=(path,old,value)=>{if(old!==value)out.push({path,before:String(old),after:String(value)});};
 for(const [key,name] of [['username','Player name'],['money','Credits']])add(name,get(before,key)?.value??'Not set',get(after,key)?.value??'Not set');
 const oldCars=cars(before,catalog),newCars=cars(after,catalog);for(const old of oldCars)if(!newCars.some(c=>c.index===old.index))add(`Garage slot ${old.index+1}`,old.name,'Removed');for(const car of newCars){const old=oldCars.find(c=>c.index===car.index);if(!old){add(`Garage slot ${car.index+1}`,'Empty',car.name);continue;}const prefix=car.name;add(`${prefix} · Model`,old.name,car.name);for(const [key,title]of [['power','Power (cv × 100)'],['weight','Mass (kg)']])add(`${prefix} · ${title}`,get(old.car,key)?.value??'Not set',get(car.car,key)?.value??'Not set');
 add(`${prefix} · Mileage`,`${(old.meter/1000).toFixed(3)} km`,`${(car.meter/1000).toFixed(3)} km`);
 const paint=i=>car.meta?.paints.find(p=>p.index===i)?.name||`Paint ${i+1}`;add(`${prefix} · Paint`,paint(old.paint),paint(car.paint));add(`${prefix} · Active setup`,String.fromCharCode(65+old.setup),String.fromCharCode(65+car.setup));
 if(old.body?.length===0x4b8&&car.body?.length===0x4b8)for(let s=0;s<3;s++)for(const [name,offset]of aids)add(`${prefix} · Setup ${String.fromCharCode(65+s)} · ${name}`,old.body[s*0x178+offset-8],car.body[s*0x178+offset-8]);}
 for(const exam of catalog?.licenses||[]){const a=examState(before,exam),b=examState(after,exam),grade=g=>grades.find(([,v])=>v===g)?.[0]||'Imported result';add(`${exam.group}-${exam.number} · Medal`,grade(a.grade),grade(b.grade));add(`${exam.group}-${exam.number} · Best time`,timeText(a.ms)||'Not set',timeText(b.ms)||'Not set');}
 return out;
}
