"""Exercise the built host inspection tool with synthetic files only."""
import pathlib
import struct
import subprocess
import sys
import tempfile

binary = sys.argv[1]
with tempfile.TemporaryDirectory() as temp:
    path = pathlib.Path(temp) / "fixture.bin"
    path.write_bytes(struct.pack("<IIHHHHIIII", 36, 32, 2, 7, 99, 3, 1, 4, 32, 0) + b"test")
    result = subprocess.run([binary, str(path)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert "Declared IP: 99.3" in result.stdout
    assert "not verified" in result.stdout
    assert "GPU initialization/display/Metal: unavailable" in result.stdout
    path.write_bytes(path.read_bytes()[:-1])
    result = subprocess.run([binary, str(path)], capture_output=True, text=True)
    assert result.returncode == 1 and "size mismatch" in result.stderr, result
    path.write_bytes(b"")
    assert subprocess.run([binary, str(path)], capture_output=True).returncode == 1
    assert subprocess.run([binary, str(path) + ".missing"], capture_output=True).returncode == 2
    assert subprocess.run([binary], capture_output=True).returncode == 2
