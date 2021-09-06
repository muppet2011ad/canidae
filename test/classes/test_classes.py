import subprocess

def test_compilation():
    completed = subprocess.run(["bin/canidae",  "test/classes/compilation.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "<class Test>"
    assert lines[1] == "<class Test2>"
    assert lines[2] == "<class Test3>"
    assert lines[3] == ""

def test_methods():
    completed = subprocess.run(["bin/canidae",  "test/classes/methods.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "I'm a basic class with no initialiser."
    assert lines[1] == "(3, 6)"
    assert lines[2] == "(5, 2)"
    assert lines[3] == ""

def test_call_function_in_field():
    completed = subprocess.run(["bin/canidae",  "test/classes/call_function_in_field.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Hello, World!"
    assert lines[1] == ""

def test_invalid_this():
    completed = subprocess.run(["bin/canidae",  "test/classes/invalid_this.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 2] Error at 'this'")
    assert lines[1] == ""

def test_nested_this():
    completed = subprocess.run(["bin/canidae",  "test/classes/nested_this.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "(1, 2)"
    assert lines[1] == "(3, 4)"
    assert lines[2] == ""

def test_inplace_operators():
    completed = subprocess.run(["bin/canidae",  "test/classes/inplace_operators.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "(1, 2)"
    assert lines[1] == "(6, 7)"
    assert lines[2] == "(6, -7)"
    assert lines[3] == ""

def test_inheritance():
    completed = subprocess.run(["bin/canidae",  "test/classes/inheritance.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 5
    assert lines[0] == "I'm a cup of orange juice!"
    assert lines[1] == "I'm a cup of empty!"
    assert lines[2] == "I'm a mug of tea (earl grey, hot)"
    assert lines[3] == "I'm a mug of empty (earl grey, hot)"
    assert lines[4] == ""

def test_bad_super1():
    completed = subprocess.run(["bin/canidae",  "test/classes/bad_super1.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 3] Error at 'super':")
    assert lines[1] == ""

def test_bad_super2():
    completed = subprocess.run(["bin/canidae",  "test/classes/bad_super2.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 4] Error at 'super':")
    assert lines[1] == ""
