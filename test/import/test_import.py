import subprocess

def test_basic():
    completed = subprocess.run(["bin/canidae",  "test/import/basic.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "(1, 2)"
    assert lines[1] == "(3, 4, 5)"
    assert lines[2] == ""

def test_nested():
    completed = subprocess.run(["bin/canidae",  "test/import/nested.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "(1, 2)"
    assert lines[1] == "(3, 4, 5)"
    assert lines[2].startswith("3.14")
    assert lines[3] == ""

def test_badfile():
    completed = subprocess.run(["bin/canidae",  "test/import/badfile.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0].startswith("Could not open file")
    assert lines[1].startswith("[line 1]")
    assert lines[2] == ""

def test_needs_identifier():
    completed = subprocess.run(["bin/canidae",  "test/import/needs_identifier.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 1] Error at ';'")
    assert lines[1] == ""