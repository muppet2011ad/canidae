import subprocess

def test_string_literals_recognised():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_literals_recognised.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Hello, World"
    assert lines[1] == ""

def test_multiline_string_literals():
    completed = subprocess.run(["bin/canidae",  "test/strings/multiline_string_literals.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "Hello,"
    assert lines[1] == "World"
    assert lines[2] == ""

def test_unterminated_string_literal():
    completed = subprocess.run(["bin/canidae",  "test/strings/unterminated_string_literal.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0] == "[line 1] Error: Unterminated string."
    assert lines[1] == ""

def test_string_array_index_get():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_array_index_get.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "o"
    assert lines[1] == " "
    assert lines[2] == "d"
    assert lines[3] == ""

def test_string_array_index_oob_1():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_array_index_oob_1.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert "Index 100 exceeds max index of string (11)." in lines[0]
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def test_string_array_index_oob_2():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_array_index_oob_2.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert "Index is less than min index of string (-12)." in lines[0]
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def test_string_array_index_set():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_array_index_set.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert "Attempt to set at index of non-array value." in lines[0]
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def test_string_concatenation():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_concatenation.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "Hello, World"
    assert lines[1] == "Hello, World!"
    assert lines[2] == "Hello, World!"
    assert lines[3] == ""

def test_string_concat_wrong_type():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_concat_wrong_type.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 3
    assert "Unsupported operands for binary operation" in lines[0]
    assert lines[1].startswith("[line 2]")
    assert lines[2] == ""

def string_comparison():
    completed = subprocess.run(["bin/canidae",  "test/strings/string_comparison.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 25
    assert lines[0] == "true"
    assert lines[1] == "false"
    assert lines[2] == "false"
    assert lines[3] == "true"
    assert lines[4] == "false"
    assert lines[5] == "true"
    assert lines[6] == "true"
    assert lines[7] == "true"
    assert lines[8] == "false"
    assert lines[9] == "true"
    assert lines[10] == "false"
    assert lines[11] == "true"
    assert lines[12] == "false"
    assert lines[13] == "true"
    assert lines[14] == "true"
    assert lines[15] == "false"
    assert lines[16] == "false"
    assert lines[17] == "false"
    assert lines[18] == "false"
    assert lines[19] == "false"
    assert lines[20] == "true"
    assert lines[21] == "false"
    assert lines[22] == "true"
    assert lines[23] == "false"
    assert lines[24] == ""