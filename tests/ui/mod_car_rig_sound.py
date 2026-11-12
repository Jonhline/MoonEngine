"""Disc-backed converter camera and independent ENGINE sound override contracts."""
import argparse, json, math, os, pathlib, re, struct, subprocess, sys, tempfile
p = argparse.ArgumentParser()
p.add_argument("tools", type=pathlib.Path)
p.add_argument("disc", type=pathlib.Path)
p.add_argument("output", type=pathlib.Path)
p.add_argument("--negative", action="store_true")
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from vol_file import Vol
v = Vol(a.disc)
donor, _ = v.read_file("/car/menu/fdeu0006")
count, offset = struct.unpack_from("<II", donor, 0x80)
assert (count, offset) == (25, 0x488)
table = donor[offset:offset+count*32]
base_env = dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_OVERLAY=str((a.output/"no-mods").resolve()))
def run(tool, args, ok=True, overlay=None):
    env = dict(base_env)
    if overlay: env["MOON_OVERLAY"] = str(overlay)
    r = subprocess.run([str((a.tools/(tool+".exe")).resolve()), *map(str,args)], env=env,
                       capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
    assert (r.returncode == 0) == ok, r.stdout + r.stderr
    return r.stdout

def engine(label, overlay=None):
    text = run("gt4spec", [a.disc, label, "ENGINE", "0x58"], overlay=overlay)
    data = bytes(int(x,16) for line in text.splitlines() if re.match(r"^  [0-9a-f]{3}:",line)
                 for x in line.split(":",1)[1].split())
    assert len(data) == 113, text
    return data

def cameras(path):
    data = path.read_bytes()
    n, at = struct.unpack_from("<II",data,0x80)
    assert n <= 32 and at + n*32 <= len(data)
    return n, at, data[at:at+n*32]

with tempfile.TemporaryDirectory(dir=a.output) as temp:
    root = pathlib.Path(temp)
    # Off-centre, non-unit source makes accidental omission of the spatial transform visible.
    binary = struct.pack("<18f",9,2,19,11,2,21,10,4,20,0,1,0,0,1,0,0,1,0) + struct.pack("<4H",0,1,2,0)
    doc = {"asset":{"version":"2.0"},"images":[],"textures":[],"materials":[],"buffers":[{"byteLength":len(binary)}],
           "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":6}],
           "accessors":[{"bufferView":i,"componentType":5126,"count":3,"type":"VEC3"} for i in range(2)]+[{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],
           "meshes":[{"name":"body","primitives":[{"indices":2,"attributes":{"POSITION":0,"NORMAL":1}}]}]}
    text = json.dumps(doc).encode();text += b" "*((-len(text))%4)
    glb = root/"source.glb"
    glb.write_bytes(struct.pack("<III",0x46546c67,2,28+len(text)+len(binary)) +
                    struct.pack("<II",len(text),0x4e4f534a)+text+
                    struct.pack("<II",len(binary),0x004e4942)+binary)
    result = root/"donor.car4"
    run("moonmod_glbcar",[glb,result,"--length",4,"--forward","-Z","--wheels-from",a.disc,"ka_01"])
    n, at, copied = cameras(result)
    if a.negative: copied = bytes([copied[0]^1])+copied[1:]
    assert (n,at,copied) == (count,offset,table), "donor camera bytes changed"
    print("donor cameras: 25 @ 0x488; all 800 bytes identical")
    sparse = root/"sparse-rig.json"
    sparse.write_text("camera 6 pos=0,-1.5,0 turns=0,0,0 lens=2.05 spare=0\n",encoding="utf-8",newline="")
    run("moonmod_glbcar",[glb,result,"--length",4,"--forward","-Z","--rig",sparse,"--wheels-from",a.disc,"ka_01"])
    sn,so,srows=cameras(result)
    assert (sn,so)==(25,0x488) and srows[:6*32]==table[:6*32] and srows[7*32:]==table[7*32:]
    assert srows[6*32:7*32]!=table[6*32:7*32],"sparse camera override lost"
    print("sparse camera 6: 24 inherited rows byte-identical, authored slot replaced")
    for forward, expected in [("-Z",(22,0,34)),("+X",(34,0,-22)),("-X",(-34,0,22)),("+Z",(-22,0,-34))]:
        rig = root/"rig.json"
        rig.write_text("forward "+forward+"\ncamera 1 pos=1,-2,-3 turns=30,10,20 lens=2.7 spare=7\n",encoding="utf-8",newline="")
        run("moonmod_glbcar",[glb,result,"--length",4,"--rig",rig,"--wheels-from",a.disc,"ka_01"])
        n,at,rows=cameras(result);row=struct.unpack_from("<8f",rows,32)
        assert n==25 and at==0x488 and rows[:32]==table[:32] and rows[64:]==table[64:]
        assert max(abs(row[i]-expected[i]) for i in range(3)) < 1e-5, (forward,row)
        assert abs(row[6]-2.7)<1e-6 and row[7]==7
        # Compare rotation matrices rather than non-unique Euler representations.
        def matrix(x,y,z):
            x,y,z=map(math.radians,(x,y,z));sx,cx=math.sin(x),math.cos(x);sy,cy=math.sin(y),math.cos(y);sz,cz=math.sin(z),math.cos(z)
            return [[cz*cy-sz*sx*sy,-sz*cx,cz*sy+sz*sx*cy],[sz*cy+cz*sx*sy,cz*cx,sz*sy-cz*sx*cy],[-cx*sy,sx,cx*cy]]
        original=matrix(10,20,30);actual=matrix(*row[3:6]);yaw={"-Z":0,"+Z":math.pi,"+X":math.pi/2,"-X":-math.pi/2}[forward]
        for r in range(3):
            for c in range(3):
                wanted=original[1][c] if r==1 else (math.cos(yaw)*original[0][c]+math.sin(yaw)*original[2][c] if r==0 else -math.sin(yaw)*original[0][c]+math.cos(yaw)*original[2][c])
                assert abs(actual[r][c]-wanted)<2e-5, (forward,r,c,actual[r][c],wanted)
    print("rig cameras: four axes, centering, scale, Euler orientation, lens and untouched donor rows passed")
    rig.write_text("camera 32 pos=0,0,0 turns=0,0,0 lens=1 spare=0\n",encoding="utf-8",newline="")
    run("moonmod_glbcar",[glb,result,"--rig",rig],ok=False)
    rig.write_text("camera 1 pos=0,0,0 turns=0,0,0 lens=1 spare=0\n",encoding="utf-8",newline="")
    run("moonmod_glbcar",[glb,result,"--rig",rig],ok=False) # sparse set without donor
    pristine = run("gt4car_survey",[a.disc,"--labels","ka_01"])
    ka,m5=engine("ka_01"),engine("m5_05")
    assert struct.unpack_from("<H",ka,24)[0]==37701 and struct.unpack_from("<H",m5,24)[0]==52601
    for power in [False,True]:
        base=root/("power" if power else "stock");custom=root/(base.name+"-sound")
        args=[a.disc,"ka_01","rig_sound_test",base,"ford",50,"--model-name","rigsoundtest"]
        if power:args += ["--power",666]
        run("moonmod_addcar",args)
        args[3]=custom
        run("moonmod_addcar",args+["--sound","m5_05"])
        before,after=engine("rig_sound_test",base),engine("rig_sound_test",custom)
        assert before[:24]==after[:24] and before[26:]==after[26:] and after[24:26]==m5[24:26]
        assert engine("ka_01",custom)==ka and engine("m5_05",custom)==m5
        assert run("gt4car_survey",[a.disc,"--labels","ka_01"],overlay=custom)==pristine
    run("moonmod_addcar",[a.disc,"ka_01","rig_sound_test",root/"invalid","ford",50,"--sound","no_such_disc_car"],ok=False)
    print("sound: ka_01=37701, mod=52601 (m5_05); only ENGINE +0x18..19 changed with/without --power")
    print("disc donor: survey line and both ENGINE rows identical with/without package")
