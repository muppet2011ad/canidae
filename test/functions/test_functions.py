import subprocess

def test_basic():
    completed = subprocess.run(["bin/canidae",  "test/functions/basic.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "Hello, World!"
    assert lines[1] == "<function print_function>"
    assert lines[2] == "Hello, World!"
    assert lines[3] == ""

def test_recursion():
    completed = subprocess.run(["bin/canidae",  "test/functions/recursion.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "6765"
    assert lines[1] == "false"
    assert lines[2] == "true"
    assert lines[3] == ""

def test_return():
    completed = subprocess.run(["bin/canidae",  "test/functions/return.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "9"
    assert lines[1] == ""

def test_incorrect_args():
    completed = subprocess.run(["bin/canidae",  "test/functions/incorrect_args.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert "Function 'add' expects 2 arguments (got 1)" in lines[0]
    assert lines[1].startswith("[line 6]")
    assert lines[2] == ""

def test_native(): # Not meant to be a full test of canidae's standard library, but just call one function to make sure that the mechanism works
    completed = subprocess.run(["bin/canidae",  "test/functions/native.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "3"
    assert lines[1] == ""

def test_closure_open():
    completed = subprocess.run(["bin/canidae",  "test/functions/closure_open.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "outside"
    assert lines[1] == ""

def test_closure_open_shared():
    completed = subprocess.run(["bin/canidae",  "test/functions/closure_open_shared.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "0"
    assert lines[1] == "42"
    assert lines[2] == ""

def test_closure_closed():
    completed = subprocess.run(["bin/canidae",  "test/functions/closure_closed.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "Hello, World!"
    assert lines[1] == "Assignment is behaving as you'd expect."
    assert lines[2] == ""

def test_closure_closed_shared():
    completed = subprocess.run(["bin/canidae",  "test/functions/closure_closed_shared.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "Hello, World!"
    assert lines[1] == "Assignment is behaving as you'd expect."
    assert lines[2] == "Goodbye, World!"
    assert lines[3] == ""