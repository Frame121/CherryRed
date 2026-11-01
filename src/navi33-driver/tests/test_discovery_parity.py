"""Compare the kernel-capable decoder to the existing strict Python parser."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools" / "ipdisc"))
from ipdiscovery import IPDiscovery
from mkfixture import FixtureGenerator

binary = sys.argv[1]
generator = FixtureGenerator()
fixtures = generator.generate_all()
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp) / "input.bin"
    for name, data in fixtures.items():
        path.write_bytes(data)
        reference = IPDiscovery()
        valid, _ = reference.init(data)
        result = subprocess.run([binary, str(path)], capture_output=True, text=True)
        assert (result.returncode == 0) == valid, (name, result.returncode, result.stderr)
        if not valid:
            assert result.returncode == 1 and not result.stdout, (name, result)
            continue
        parsed = json.loads(result.stdout)
        assert parsed["validation_scope"] == "STRUCTURE_ONLY"
        assert parsed["hardware_initialization_allowed"] is False
        assert len(parsed["ips"]) == reference.ip_count()
        for actual, expected in zip(parsed["ips"], reference.to_dict()["ips"]):
            for field in ("hw_id", "instance", "version", "num_bases", "bases"):
                assert actual[field] == expected[field], (name, field, actual, expected)
    # Resource/input contracts are independent of parser fixtures.
    path.write_bytes(b"\0" * (1024 * 1024 + 1))
    assert subprocess.run([binary, str(path)], capture_output=True).returncode == 2
    assert subprocess.run([binary, str(path) + ".missing"], capture_output=True).returncode == 2
    assert subprocess.run([binary], capture_output=True).returncode == 2
print(f"C++/strict Python parity passed for {len(fixtures)} synthetic fixtures")
