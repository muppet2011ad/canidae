import subprocess

def test_read_file_success():
    completed = subprocess.run(["bin/canidae", "test/stdlib/read_file_success.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "hello from fixture"
    assert lines[1] == ""

def test_read_file_missing_raises_catchable_value_error():
    completed = subprocess.run(["bin/canidae", "test/stdlib/read_file_missing.can"], text=True, capture_output=True)
    assert completed.returncode == 0
    lines = completed.stdout.split("\n")
    assert len(lines) == 2
    assert lines[0] == "Could not open file 'test/stdlib/nonexistent_file.txt'."
    assert lines[1] == ""

def test_read_file_missing_uncaught_exits_with_error():
    completed = subprocess.run(["bin/canidae", "test/stdlib/read_file_uncaught.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    assert "ValueError" in completed.stderr
    assert "Could not open file 'test/stdlib/nonexistent_file.txt'." in completed.stderr

def test_read_file_wrong_type_raises_type_error():
    completed = subprocess.run(["bin/canidae", "test/stdlib/read_file_wrong_type.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    assert "TypeError" in completed.stderr
    assert "expects a string filename" in completed.stderr

def test_read_file_wrong_arg_count_raises_argument_error():
    completed = subprocess.run(["bin/canidae", "test/stdlib/read_file_wrong_args.can"], text=True, capture_output=True)
    assert completed.returncode == 70
    assert "ArgumentError" in completed.stderr
    assert "expects 1 argument (got 0)" in completed.stderr
