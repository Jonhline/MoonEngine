"""Exercise the native installer's FTP transaction against a local test server.

The server exposes only a disposable temporary directory, never the Switch.
"""
import argparse
import os
import socket
import socketserver
import subprocess
import tempfile
import threading
from pathlib import Path


class Handler(socketserver.StreamRequestHandler):
    def reply(self, message):
        self.wfile.write((message + "\r\n").encode())
        self.wfile.flush()

    def resolve(self, name):
        value = name if name.startswith("/") else self.cwd + "/" + name
        result = (self.server.root / value.lstrip("/")).resolve()
        if result != self.server.root and self.server.root not in result.parents:
            raise ValueError("Outside fixture root")
        return result

    def handle(self):
        self.cwd = "/"
        self.passive = None
        self.rename_from = None
        self.reply("220 Moon Engine disposable FTP fixture")
        try:
            while raw := self.rfile.readline():
                command, _, argument = raw.decode().strip().partition(" ")
                if command == "USER":
                    self.reply("331 Password required")
                elif command == "PASS":
                    self.reply("230 Logged in")
                elif command == "SYST":
                    self.reply("215 UNIX Type: L8")
                elif command in ("TYPE", "OPTS", "NOOP"):
                    self.reply("200 OK")
                elif command == "FEAT":
                    self.reply("211 No extensions")
                elif command == "PWD":
                    self.reply(f'257 "{self.cwd}"')
                elif command == "CWD":
                    path = self.resolve(argument)
                    if path.is_dir():
                        self.cwd = "/" + path.relative_to(self.server.root).as_posix().strip(".")
                        self.reply("250 Changed directory")
                    else:
                        self.reply("550 Missing directory")
                elif command == "MKD":
                    self.resolve(argument).mkdir()
                    self.reply("257 Created")
                elif command == "PASV":
                    self.passive = socket.socket()
                    self.passive.bind(("127.0.0.1", 0))
                    self.passive.listen()
                    self.passive.settimeout(10)
                    port = self.passive.getsockname()[1]
                    self.reply(f"227 Entering Passive Mode (127,0,0,1,{port // 256},{port % 256})")
                elif command in ("LIST", "NLST", "RETR", "STOR"):
                    path = self.resolve(argument or self.cwd)
                    if command != "STOR" and not path.exists():
                        self.reply("550 Missing file")
                        continue
                    self.reply("150 Opening data connection")
                    conn, _ = self.passive.accept()
                    with conn:
                        if command in ("LIST", "NLST"):
                            paths = list(path.iterdir()) if path.is_dir() else [path]
                            for entry in paths:
                                row = entry.name if command == "NLST" else (
                                    ("drwxr-xr-x" if entry.is_dir() else "-rw-r--r--") +
                                    f" 1 user group {entry.stat().st_size} Oct 17 2026 {entry.name}")
                                conn.sendall((row + "\r\n").encode())
                        elif command == "RETR":
                            conn.sendall(path.read_bytes())
                        else:
                            with path.open("wb") as output:
                                while data := conn.recv(65536):
                                    output.write(data)
                    self.passive.close()
                    self.passive = None
                    self.reply("226 Transfer complete")
                elif command == "SIZE":
                    path = self.resolve(argument)
                    self.reply(f"213 {path.stat().st_size}" if path.is_file() else "550 Missing file")
                elif command == "RNFR":
                    self.rename_from = self.resolve(argument)
                    self.reply("350 Rename target")
                elif command == "RNTO":
                    target = self.resolve(argument)
                    if target.exists():
                        self.reply("550 Destination exists")
                    else:
                        self.rename_from.rename(target)
                        self.reply("250 Renamed")
                elif command == "QUIT":
                    self.reply("221 Bye")
                    break
                else:
                    self.reply("502 Unsupported")
        finally:
            if self.passive:
                self.passive.close()


def test(exe):
    with tempfile.TemporaryDirectory() as folder:
        root = Path(folder)
        server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), Handler)
        server.daemon_threads = True
        server.root = root.resolve()
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        report = root / "result.txt"
        try:
            result = subprocess.run([str(exe.resolve()), "--ftp-self-test", "127.0.0.1",
                                     str(server.server_address[1]), str(report)], timeout=60)
            text = report.read_text() if report.exists() else "No report"
            assert result.returncode == 0, text
            assert text.startswith("PASS:"), text
            files = list((root / "moonengine-backups").glob("setup-test-*/file.bin"))
            assert len(files) == 1
            assert files[0].read_bytes() == b"Moon Engine replacement fixture"
            assert files[0].with_name("old.bin").read_bytes() == b"Moon Engine native FTP fixture"
            assert not files[0].with_name("should-not-exist.bin").exists()
            print(text.strip())
        finally:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    args = parser.parse_args()
    test(args.exe)
