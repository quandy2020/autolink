#!/usr/bin/env python3

from autolink import _core


def test_import_core_attr():
    assert _core.__version__ == "0.1.0-pybind11"


if __name__ == "__main__":
    test_import_core_attr()
    print("autolink python smoke import passed")
