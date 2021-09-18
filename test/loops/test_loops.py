import subprocess

def test_loop_while_basic_loop():
    completed = subprocess.run(["bin/canidae",  "test/loops/while/basic_loop.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "10"
    assert lines[1] == "11"
    assert lines[2] == "12"
    assert lines[3] == ""

def test_loop_while_no_cond():
    completed = subprocess.run(["bin/canidae",  "test/loops/while/no_cond.can"], text=True, capture_output=True)
    assert completed.returncode == 65
    lines = completed.stderr.split("\n")
    assert len(lines) == 2
    assert lines[0] == "[line 3] Error at 'do': Expect expression."
    assert lines[1] == ""

def test_loop_while_break():
    completed = subprocess.run(["bin/canidae",  "test/loops/while/break.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "3"
    assert lines[1] == "5"
    assert lines[2] == ""

def test_loop_while_continue():
    completed = subprocess.run(["bin/canidae",  "test/loops/while/continue.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 4
    assert lines[0] == "9"
    assert lines[1] == "5"
    assert lines[2] == "5"
    assert lines[3] == ""

def test_loop_while_infinite():
    completed = subprocess.run(["bin/canidae",  "test/loops/while/infinite.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "1000"
    assert lines[1] == ""

def test_loop_do_basic_loop():
    completed = subprocess.run(["bin/canidae",  "test/loops/do/basic_loop.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "11"
    assert lines[1] == ""

def test_loop_do_break():
    completed = subprocess.run(["bin/canidae",  "test/loops/do/break.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 3
    assert lines[0] == "3"
    assert lines[1] == "10"
    assert lines[2] == ""

def test_loop_do_continue():
    completed = subprocess.run(["bin/canidae",  "test/loops/do/continue.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 6
    assert lines[0] == "1"
    assert lines[1] == "2"
    assert lines[2] == "4"
    assert lines[3] == "5"
    assert lines[4] == "10"
    assert lines[5] == ""

def test_loop_do_infinite():
    completed = subprocess.run(["bin/canidae",  "test/loops/do/infinite.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "1000"
    assert lines[1] == ""

def test_loop_for_basic_loop():
    completed = subprocess.run(["bin/canidae",  "test/loops/for/basic_loop.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 17
    assert lines == ["0", "1", "2", "3", "4", "1", "2", "3", "4", "5", "0", "1", "2", "3", "4", "5", ""]

def test_loop_for_break():
    completed = subprocess.run(["bin/canidae",  "test/loops/for/break.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 5
    assert lines[0] == "0"
    assert lines[1] == "1"
    assert lines[2] == "2"
    assert lines[3] == "5"
    assert lines[4] == ""

def test_loop_for_continue():
    completed = subprocess.run(["bin/canidae",  "test/loops/for/continue.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 10
    assert lines == ["0", "1", "2", "4", "0", "1", "2", "3", "4", ""]

def test_loop_scope_correct():
    completed = subprocess.run(["bin/canidae",  "test/loops/for/scope_correct.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    lines = completed.stderr.split("\n")
    assert len(lines) == 4
    assert "Undefined variable" in lines[0]
    assert lines[2].startswith("\t[line 3]")
    assert lines[3] == ""

def test_loop_for_infinite():
    completed = subprocess.run(["bin/canidae",  "test/loops/for/infinite.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 10
    assert lines == ["0", "1", "2", "3", "4", "1", "2", "3", "4", ""]