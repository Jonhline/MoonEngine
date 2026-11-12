"""Exercise GLB/rig to original brake branches; refuse missing/duplicate bindings."""
import json, os, pathlib, struct, subprocess, sys, tempfile
converter, probe = map(pathlib.Path, sys.argv[1:3])
negative = "--negative" in sys.argv[3:]
with tempfile.TemporaryDirectory(prefix="moon-functional-") as temp:
    root=pathlib.Path(temp)
    binary=struct.pack("<18f",-1,0,-1,1,0,-1,0,1,1,0,1,0,0,1,0,0,1,0)+struct.pack("<4H",0,1,2,0)
    doc={"asset":{"version":"2.0"},"images":[],"textures":[],"materials":[{}, {"extensions":{"KHR_materials_unlit":{}}}],"buffers":[{"byteLength":len(binary)}],
         "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":6}],
         "accessors":[{"bufferView":i,"componentType":5126,"count":3,"type":"VEC3"} for i in range(2)]+[{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],
         "meshes":[{"primitives":[{"indices":2,"material":i,"attributes":{"POSITION":0,"NORMAL":1}}]} for i in range(2)],
         "nodes":[{"name":n,"mesh":int(n=="lamp_on")} for n in ["body","lamp_off","lamp_on"]]}
    text=json.dumps(doc).encode();text+=b" "*(-len(text)%4)
    glb=root/"source.glb";out=root/"car4";rig=root/"rig.json"
    glb.write_bytes(struct.pack("<III",0x46546c67,2,28+len(text)+len(binary))+struct.pack("<II",len(text),0x4e4f534a)+text+struct.pack("<II",len(binary),0x004e4942)+binary)
    env=dict(os.environ,SDL_AUDIODRIVER="dummy")
    def convert(binding):
        rig.write_text(binding,encoding="utf-8",newline="")
        return subprocess.run([str(converter),str(glb),str(out),"--forward","-Z","--rig",str(rig)],env=env,capture_output=True,text=True)
    result=convert("brake released part=lamp_off\nbrake pressed part=lamp_on\n" if not negative else "")
    assert result.returncode==0,result.stdout+result.stderr
    check=subprocess.run([str(probe),"--model",str(out)],env=env,capture_output=True,text=True)
    print(check.stdout,flush=True);assert check.returncode==0,check.stderr
    for bad in ["brake pressed part=missing\n","brake released part=lamp_off\nbrake pressed part=lamp_off\n"]:
        result=convert(bad);assert result.returncode!=0,"invalid rig accepted"
    print("PASS exported body/released/pressed geometry and missing/duplicate binding rejection")
