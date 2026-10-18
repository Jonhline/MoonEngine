"""Run the installer's real ISO serial reader against independent ISO9660 fixtures."""
import pathlib, struct, subprocess, sys, tempfile
exe=pathlib.Path(sys.argv[1]).resolve()
def iso(serial='SCUS_973.28',version='2.00',extra=''):
    data=bytearray(24*2048)
    data[16*2048:16*2048+7]=b'\x01CD001\x01'
    root=16*2048+156
    data[root]=34
    struct.pack_into('<I',data,root+2,21)
    struct.pack_into('<I',data,root+10,2048)
    cnf=f'BOOT2 = cdrom0:\\{serial};1\nVER = {version}\nVMODE = NTSC\n{extra}'.encode()
    name=b'SYSTEM.CNF;1';record=21*2048
    data[record]=46
    struct.pack_into('<I',data,record+2,22)
    struct.pack_into('<I',data,record+10,len(cnf))
    data[record+32]=len(name);data[record+33:record+33+len(name)]=name
    data[22*2048:22*2048+len(cnf)]=cnf
    return data
cases=[('revision1',iso(version='1.00'),True),('revision2',iso(),True),
       ('other-revision',iso(version='9.99',extra='unrelated hash-changing bytes'),True),
       ('wrong-serial',iso(serial='SCES_517.19',extra='SCUS_973.28'),False),
       ('wrong-number',iso(serial='SCUS_973.29'),False),('junk',bytearray(100),False)]
missing=iso();missing[21*2048+33]=ord('X');cases.append(('missing-system-cnf',missing,False))
with tempfile.TemporaryDirectory() as directory:
    for label,data,expected in cases:
        path=pathlib.Path(directory)/(label+'.iso');path.write_bytes(data)
        result=subprocess.run([str(exe),'--check-disc',str(path)],timeout=15)
        assert (result.returncode==0)==expected,(label,result.returncode)
        print('PASS',label)
if len(sys.argv)>2:
    subprocess.run([str(exe),'--check-disc',sys.argv[2]],timeout=15,check=True)
    print('PASS user disc serial (no whole-image hash)')
