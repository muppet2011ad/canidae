import subprocess

def test_print():
    completed = subprocess.run(["bin/canidae",  "test/basic/print.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 5
    assert lines[0] == "5"
    assert lines[1] == "true"
    assert lines[2] == "null"
    assert lines[3] == "Hello World!"
    assert lines[4] == ""

def test_print_invalid():
    completed = subprocess.run(["bin/canidae",  "test/basic/print_invalid.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 1] Error at ';'")
    assert lines[1] == ""

def test_basic_arithmetic():
    completed = subprocess.run(["bin/canidae",  "test/basic/basic_arithmetic.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 8
    assert lines[0] == "2"
    assert lines[1] == "3"
    assert lines[2] == "18"
    assert lines[3] == "4"
    assert lines[4] == "3125"
    assert lines[5] == "-4"
    assert lines[6][:5] == "3.833"
    assert lines[7] == ""

def test_more_complicated_expressions():
    completed = subprocess.run(["bin/canidae",  "test/basic/more_complicated_expressions.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 6
    assert lines[0] == "63"
    assert lines[1] == "12"
    assert lines[2].startswith("-0.5555")
    assert lines[3] == "-4"
    assert lines[4] == "16"
    assert lines[5] == ""

def test_binop_type_checking():
    completed = subprocess.run(["bin/canidae",  "test/basic/binop_type_checking.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0].startswith("Unsupported operands for binary operation")
    assert lines[1].startswith("[line 1]")