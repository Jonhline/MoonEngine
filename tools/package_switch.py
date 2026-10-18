#!/usr/bin/env python3
"""Create a private Switch ZIP from an explicitly selected, tested NRO.

No network calls or publication. Game images, player data and SDK binaries
are never copied. The included source snapshot contains tracked build inputs
from the working tree; the manifest identifies the actual binary by SHA-256.
"""
import argparse
import hashlib
import json
import re
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

README = """MOON ENGINE - SWITCH - {version}
Pacote privado para teste. Nao publicado como release oficial.

Instalacao pelo PC (Windows 10/11)
1. Extraia TODO este ZIP numa pasta. Abra "Instalar no Switch.cmd".
2. Escolha sua ISO do GT4 USA v2.00 (SCUS-97328). O assistente confere a copia.
3. No Switch: DBI > Start MTP Connection. Conecte o cabo USB e siga a janela.

O assistente copia a ISO e o aplicativo para o SD, configura o caminho e
confere o NRO instalado. Nao precisa Python nem devkitPro no PC do jogador.
Mantenha o cabo conectado ate aparecer "Pronto". Uma ISO de mais de 4 GB
precisa de exFAT neste fluxo. O assistente NAO formata o cartao.

Para jogar
Encerre o MTP. Segure R ao abrir um jogo para iniciar o Homebrew Menu em
modo aplicativo (RAM completa). Abra Moon Engine. O modo Album tem pouca RAM.
No Sphaira, tambem use o modo aplicativo. O pacote e NRO, nao NSP.

Saves e configuracoes
Saves existentes ficam preservados. Se existir switch/opengt4 e ainda nao
existir switch/moonengine, o assistente atualiza a pasta antiga, sem criar
um segundo aplicativo. Backups do NRO anterior ficam em moonengine-backups.
Nao copie saves/configs de outra pessoa. Cada jogador cria o proprio perfil.

Instalacao manual (opcional)
Copie a pasta switch deste pacote para a raiz do SD e coloque sua ISO como
switch/moonengine/gt4.iso. Na primeira abertura a configuracao sera criada.
Em instalacoes antigas com saves em switch/opengt4, prefira o assistente.

Controles: A=Circle, B=Cross, X=Triangle, Y=Square (por posicao).
Touch: um toque seleciona; outro no mesmo item confirma. Arraste listas/mapa.
As opcoes de touch ficam em Options > Screen Settings.

Estado desta versao
Build Release ARM64/deko3d, com videos decodificados no Switch.
Baseado no build testado no console do desenvolvedor. Ainda e experimental:
60 FPS constantes em toda pista nao estao comprovados; o erro de retorno
ao Sphaira apos fechar o aplicativo ainda esta em investigacao.
O transporte USB do assistente precisa de verificacao no DBI real; o motor
de copia/renomeacao e a preservacao de pastas foram testados no Windows.

Este port nao e afiliado a Sony ou Polyphony Digital. Nao inclui o jogo,
chaves, firmware ou saves. O jogador fornece sua propria copia do disco.
Gran Turismo e uma marca dos respectivos titulares.

Licenca: GPL-3.0 com os termos de NOTICE.txt. Codigo do projeto e scripts
de build: source.zip. Dependencias e fontes: THIRD_PARTY.txt.
Identificacao do binario e hashes: manifest.json.
"""

NOTICES = """Moon Engine: LICENSE.txt (GPL-3.0) and NOTICE.txt.
The project source/build scripts are in source.zip. This is a private
development package, not a public update feed or an official game release.

Switch libraries come from the devkitPro packages used by the build:
https://github.com/devkitPro/pacman-packages
https://github.com/devkitPro/portlibs
https://github.com/devkitPro/switch-tools
https://github.com/switchbrew/libnx (ISC)
https://github.com/devkitPro/deko3d (zlib)
https://github.com/libsdl-org/SDL (zlib)
https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz (this SDK build: GPL-2.0-or-later)
FFmpeg legal/build information: https://ffmpeg.org/legal.html
ps2float: LGPL-3.0-or-later; complete source and licenses in source.zip.
stb_image: MIT/public domain; source and license in source.zip.
miniz 3.0.2: MIT; https://github.com/richgel999/miniz/tree/3.0.2
The installed FFmpeg configuration reports --enable-gpl and GPL version 2
or later. See the package recipes for transitive dependencies and licenses.
The console SDK, commercial game data and keys are not distributed here.
"""


def source_names(root):
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
    roots = {"CMakeLists.txt", "LICENSE", "NOTICE", "README.md", ".clang-format"}
    prefixes = ("src/", "include/", "cmake/", "third_party/", "tools/", "tests/", "config/")
    private, private_prefixes = set(), ()  # the shared tree: nothing private is in it
    export = Path(__file__).resolve().parent / "export_public.py"
    if export.is_file():  # the development tree: leave out what its public export leaves out
        import importlib.util
        spec = importlib.util.spec_from_file_location("export_public", export)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        private, private_prefixes = module.EXCLUDE, module.EXCLUDE_PREFIXES
    for name in names:
        if name in private or name.startswith(private_prefixes):
            continue
        if name in roots or name.startswith(prefixes):
            path = root / name
            if path.is_file():
                yield name


def build_package(nro, output, version, root=ROOT):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", version):
        raise ValueError("Invalid version label")
    data = nro.read_bytes()
    if len(data) < 0x20 or data[0x10:0x14] != b"NRO0":
        raise ValueError("Not a Switch NRO")
    if output.exists():
        raise FileExistsError(f"Refusing to overwrite {output}")
    output.mkdir(parents=True)
    sd = output / "switch/moonengine"
    installer = output / "installer"
    sd.mkdir(parents=True)
    installer.mkdir()
    (sd / "moonengine.nro").write_bytes(data)
    for name in ("switch_install.ps1", "switch_transfer.cs"):
        (installer / name).write_bytes((root / "tools" / name).read_bytes())
    cmd = ('@echo off\r\n'
           'powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0installer\\switch_install.ps1" -PackageRoot "%~dp0."\r\n'
           'if errorlevel 1 pause\r\n')
    (output / "Instalar no Switch.cmd").write_bytes(cmd.encode("ascii"))
    for name in ("LICENSE", "NOTICE"):
        (output / (name + ".txt")).write_bytes((root / name).read_bytes())
    (output / "LEIA-ME.txt").write_text(README.format(version=version), encoding="utf-8", newline="")
    (output / "THIRD_PARTY.txt").write_text(NOTICES, encoding="utf-8", newline="")
    discs = json.loads((root / "config/supported_discs.json").read_text())
    disc = next(d for d in discs["discs"] if d.get("serial") == "SCUS-97328" and d.get("version") == "2.00")
    manifest = {
        "version": version,
        "private": True,
        "nro_sha256": hashlib.sha256(data).hexdigest(),
        "disc_sha1": disc["sha1"],
        "source_snapshot_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "source_snapshot_worktree": True,
        "usb_hardware_validated": False,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="")
    with zipfile.ZipFile(output / "source.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        for name in source_names(root):
            archive.write(root / name, name)
    zip_path = Path(str(output) + ".zip")
    if zip_path.exists():
        raise FileExistsError(f"Refusing to overwrite {zip_path}")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(output))
    return zip_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--nro", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    print(build_package(args.nro, args.out, args.version))
