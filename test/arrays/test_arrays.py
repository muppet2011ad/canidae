import subprocess

def test_array_declaration_and_printing():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_declaration_and_printing.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "[1, 2, 3]"
    assert lines[1] == "[3, 7, 11]"
    assert lines[2] == ""

def test_array_wrong_dec_1():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_wrong_dec_1.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 1] Error at ';'")
    assert lines[1] == ""

def test_array_wrong_dec_2():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_wrong_dec_2.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0].startswith("[line 1] Error at '2'")
    assert lines[1] == ""

def test_array_index_get():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_index_get.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 5
    assert lines[0] == "1"
    assert lines[1] == "3"
    assert lines[2] == "2"
    assert lines[3] == "3"
    assert lines[4] == ""

def test_array_index_get_oob_1():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_index_get_oob_1.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0] == "Array index 3 exceeds max index of array (2)."
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def test_array_index_get_oob_2():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_index_get_oob_2.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0] == "Index is less than min index of array (-3)."
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def array_index_set():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_index_set.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "[0, 2, 3, 5]"
    assert lines[1] == "[0, 2, 3, 5, null, 6]"
    assert lines[2] == "[0, 2, 3, 5, null, 5]"
    assert lines[3] == ""

def test_array_index_set_oob():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_index_set_oob.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert lines[0] == "Index is less than min index of string (-3)."
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def test_array_2d():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_2d.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "[[6, 8], [10, 12]]"
    assert lines[1] == ""

def test_array_concat():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_concat.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "[1, 2, 3, 4, 5, 6]"
    assert lines[1] == ""

def test_array_assignment_operators():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_assignment_operators.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "[2, 1, 3]"
    assert lines[1] == "[6, -2, 6]"
    assert lines[2] == "[2, 4, 6]"
    assert lines[3] == ""

def test_array_many_types():
    completed = subprocess.run(["bin/canidae",  "test/arrays/array_many_types.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == '[null, 0, 1, , Hello, [0, F], true]'
    assert lines[1] == ""