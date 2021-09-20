import subprocess

def test_equality():
    completed = subprocess.run(["bin/canidae",  "test/logic/equality.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 29
    assert lines[0] == "false"
    assert lines[1] == "true"
    assert lines[2] == "false"
    assert lines[3] == "false"
    assert lines[4] == "true"
    assert lines[5] == "true"
    assert lines[6] == "false"
    assert lines[7] == "true"
    assert lines[8] == "true"
    assert lines[9] == "false"
    assert lines[10] == "true"
    assert lines[11] == "true"
    assert lines[12] == "true"
    assert lines[13] == "false"
    assert lines[14] == "true"
    assert lines[15] == "false"
    assert lines[16] == "true"
    assert lines[17] == "true"
    assert lines[18] == "false"
    assert lines[19] == "false"
    assert lines[20] == "true"
    assert lines[21] == "false"
    assert lines[22] == "false"
    assert lines[23] == "true"
    assert lines[24] == "false"
    assert lines[25] == "false"
    assert lines[26] == "false"
    assert lines[27] == "true"
    assert lines[28] == ""

def test_not():
    completed = subprocess.run(["bin/canidae",  "test/logic/not.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 10
    assert lines[0] == "false"
    assert lines[1] == "true"
    assert lines[2] == "true"
    assert lines[3] == "false"
    assert lines[4] == "true"
    assert lines[5] == "false"
    assert lines[6] == "true"
    assert lines[7] == "false"
    assert lines[8] == "true"
    assert lines[9] == ""

def test_inequality():
    completed = subprocess.run(["bin/canidae",  "test/logic/inequality.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 13
    assert lines[0] == "true"
    assert lines[1] == "false"
    assert lines[2] == "false"
    assert lines[3] == "true"
    assert lines[4] == "false"
    assert lines[5] == "true"
    assert lines[6] == "false"
    assert lines[7] == "true"
    assert lines[8] == "true"
    assert lines[9] == "false"
    assert lines[10] == "true"
    assert lines[11] == "false"
    assert lines[12] == ""

def test_inequality_wrong_type_1():
    completed = subprocess.run(["bin/canidae",  "test/logic/inequality_wrong_type_1.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 4
    assert "Cannot perform comparison on values of different type" in lines[0]
    assert lines[2].startswith("\t[line 1]")
    assert lines[3] == ""

def test_inequality_wrong_type_2():
    completed = subprocess.run(["bin/canidae",  "test/logic/inequality_wrong_type_2.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 4
    assert "Cannot perform comparison on objects of different type" in lines[0]
    assert lines[2].startswith("\t[line 1]")
    assert lines[3] == ""

def test_logic_operators():
    completed = subprocess.run(["bin/canidae",  "test/logic/logic_operators.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 17
    assert lines[0] == "true"
    assert lines[1] == "false"
    assert lines[2] == "false"
    assert lines[3] == "false"
    assert lines[4] == "true"
    assert lines[5] == "true"
    assert lines[6] == "true"
    assert lines[7] == "false"
    assert lines[8] == "false"
    assert lines[9] == "true"
    assert lines[10] == "0"
    assert lines[11] == "1"
    assert lines[12] == "true"
    assert lines[13] == "1"
    assert lines[14] == "false"
    assert lines[15] == "1"
    assert lines[16] == ""
    