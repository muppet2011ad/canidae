import subprocess

def test_trycatch_basic():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_basic.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Caught"
    assert lines[1] == ""

def test_trycatch_nested():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_nested.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Inner"
    assert lines[1] == ""

def test_trycatch_in_function():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_in_function.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "ruh roh"
    assert lines[1] == ""

def test_trycatch_in_specific():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_specific.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    good_lines = completed.stdout.split("\n")
    assert len(good_lines) == 2
    bad_lines = completed.stderr.split("\n")
    assert len(bad_lines) > 1
    assert good_lines[0] == "Caught error 1"
    assert "Unsupported operands for binary operation." in bad_lines[0]

def test_trycatch_bind_exception():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_bind_exception.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert "<exception TypeError" in lines[0]

def test_trycatch_multiple_types():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/trycatch_multiple_types.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Caught"
    assert lines[1] == ""

def test_exception_creation():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/exception_creation.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert "<exception TypeError" in lines[0]

def test_exception_access():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/exception_access.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "<errortype TypeError>"
    assert lines[1] == "new exception"

def test_raise_basic():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/raise_basic.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 4
    assert "Exceptional as ever" in lines[0]
    assert lines[1] == "Raised at:"
    assert lines[2].startswith("\t[line 2]")

def test_raise_fancy():
    completed = subprocess.run(["bin/canidae",  "test/exceptions/raise_fancy.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "ruh roh"
    assert "<exception TypeError" in lines[1]