import subprocess

def test_declare_global():
    completed = subprocess.run(["bin/canidae",  "test/variables/declare_global.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "5"
    assert lines[1] == ""

def test_use_unset_global():
    completed = subprocess.run(["bin/canidae",  "test/variables/use_unset_global.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "null"
    assert lines[1] == ""

def test_global_assignment():
    completed = subprocess.run(["bin/canidae",  "test/variables/global_assignment.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "true"
    assert lines[1] == ""

def test_declare_local():
    completed = subprocess.run(["bin/canidae",  "test/variables/declare_local.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "5"
    assert lines[1] == ""

def test_use_unset_local():
    completed = subprocess.run(["bin/canidae",  "test/variables/use_unset_local.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "null"
    assert lines[1] == ""

def test_local_assignment():
    completed = subprocess.run(["bin/canidae",  "test/variables/local_assignment.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "true"
    assert lines[1] == ""

def test_variables_and_expressions():
    completed = subprocess.run(["bin/canidae",  "test/variables/variables_and_expressions.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "15"
    assert lines[1] == ""

def test_shadowing():
    completed = subprocess.run(["bin/canidae",  "test/variables/shadowing.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 6
    assert lines[0] == "2"
    assert lines[1] == "3"
    assert lines[2] == "4"
    assert lines[3] == "3"
    assert lines[4] == "2"
    assert lines[5] == ""

def test_use_in_own_dec_1():
    completed = subprocess.run(["bin/canidae",  "test/variables/use_in_own_dec_1.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0].startswith("Undefined variable")
    assert lines[1].startswith("[line 1]")
    assert lines[2] == ""

def test_use_in_own_dec_2():
    completed = subprocess.run(["bin/canidae",  "test/variables/use_in_own_dec_2.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 4] Error at 'x': Can't read local variable")
    assert lines[1] == ""

def test_redefine():
    completed = subprocess.run(["bin/canidae",  "test/variables/redefine.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 3] Error at 'x': Already a variable with this name in this scope")
    assert lines[1] == ""

def test_assignment_operators():
    completed = subprocess.run(["bin/canidae",  "test/variables/assignment_operators.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 8
    assert lines[0] == "12"
    assert lines[1] == "-10"
    assert lines[2] == "-120"
    assert lines[3] == "12"
    assert lines[4] == "144"
    assert lines[5] == "145"
    assert lines[6] == "144"
    assert lines[7] == ""

def test_assignment_operator_type_check_1():
    completed = subprocess.run(["bin/canidae",  "test/variables/assignment_operator_type_check_1.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0].startswith("Unsupported operands for binary operation")
    assert lines[1].startswith("[line 4]")
    assert lines[2] == ""

def test_assignment_operator_type_check_2():
    completed = subprocess.run(["bin/canidae",  "test/variables/assignment_operator_type_check_2.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0].startswith("Unsupported operands for binary operation")
    assert lines[1].startswith("[line 3]")
    assert lines[2] == ""