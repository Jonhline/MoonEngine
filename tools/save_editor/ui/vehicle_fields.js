// Original serialized setup fields: career_setting.c (live offsets minus 8).
export const fields=[];
function group(name,offsets,width=1){offsets.forEach((o,i)=>fields.push({name:`${name} ${i+1}`,group:name,offset:o-8,width:Array.isArray(width)?width[i]:width}));}
group('Brake balance',[0x137,0x138]);
group('Suspension',[0x14e,0x14f,0x150,0x151,0x152,0x153,0x154,0x155,0x141,0x142,0x148,0x149,0x144,0x146,0x14a,0x14b,0x156,0x157],[1,1,1,1,1,1,1,1,1,1,1,1,2,2,1,1,1,1]);
group('LSD',[0x158,0x15a,0x15c,0x159,0x15b,0x15d]);
group('Gearbox',[0x118,0x11a,0x11c,0x11e,0x120,0x122,0x124,0x126,0x130,0x132,0x134],[2,2,2,2,2,2,2,2,2,1,2]);
group('Nitrous',[0x16f]);group('Torque distribution',[0x136]);
group('Ballast mass / position',[0x163,0x164]);fields.at(-1).signed=true;
group('ASM',[0x15f,0x161]);fields.at(-1).paired=true;fields.at(-2).paired=true;
group('Traction control',[0x15e]);group('Downforce',[0x139,0x13a]);
fields.push({name:'Power multiplier (%)',group:'Performance',offset:0x163,width:1},{name:'Mass multiplier (%)',group:'Performance',offset:0x162,width:1});
for(const [offset,name]of [[0x20,'Brakes'],[0x28,'Brake controller'],[0x30,'Chassis'],[0x38,'Engine'],[0x40,'Drivetrain'],[0x48,'Transmission'],[0x50,'Suspension'],[0x58,'LSD'],[0x60,'Front tyres'],[0x68,'Rear tyres'],[0x70,'Steering'],[0x78,'Weight reduction'],[0x80,'Racing modification'],[0xb0,'Turbo'],[0xb8,'Clutch'],[0xc8,'Propeller shaft'],[0xe0,'ASM unit'],[0xe8,'TCS unit'],[0xf0,'Wheel'],[0xf8,'Nitrous kit'],[0x100,'Wing']])fields.push({name:`${name} · equipped key`,group:'Equipped parts',offset,width:8});
export function read(body,offset,type='u8'){
 const sizes={u8:1,i8:1,u16:2,i16:2,u32:4,i32:4,u64:8,f32:4};const size=sizes[type];if(!size||!Number.isInteger(offset)||offset<0||offset+size>body.length)throw Error('Parameter is outside the car record.');
 const d=new DataView(Uint8Array.from(body).buffer);return type==='u64'?d.getBigUint64(offset,true).toString():type==='f32'?d.getFloat32(offset,true):type==='u8'?d.getUint8(offset):type==='i8'?d.getInt8(offset):type==='u16'?d.getUint16(offset,true):type==='i16'?d.getInt16(offset,true):type==='u32'?d.getUint32(offset,true):d.getInt32(offset,true);
}
export function write(body,offset,type,value){
 read(body,offset,type);const b=Uint8Array.from(body),d=new DataView(b.buffer),signed=type[0]==='i';
 if(type==='f32'){const n=Number(value);if(!Number.isFinite(n)||!Number.isFinite(Math.fround(n)))throw Error('Enter a finite float32 value.');d.setFloat32(offset,n,true);}
 else {if(!/^-?\d+$/.test(String(value)))throw Error('Enter an integer value.');const n=BigInt(value),bits=BigInt(type.slice(1)),lo=signed?-(1n<<(bits-1n)):0n,hi=signed?(1n<<(bits-1n))-1n:(1n<<bits)-1n;if(n<lo||n>hi)throw Error(`Value is outside ${type}.`);
 if(type==='u64')d.setBigUint64(offset,n,true);else if(type==='u8')d.setUint8(offset,Number(n));else if(type==='i8')d.setInt8(offset,Number(n));else if(type==='u16')d.setUint16(offset,Number(n),true);else if(type==='i16')d.setInt16(offset,Number(n),true);else if(type==='u32')d.setUint32(offset,Number(n),true);else d.setInt32(offset,Number(n),true);}
 return [...b];
}
