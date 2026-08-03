#!/usr/bin/env python3

import os
import tempfile

import autolink


def test_record_write_read():
    path = os.path.join(tempfile.mkdtemp(prefix="autolink_py_record_"),
                        "t.record")
    writer = autolink.RecordWriter()
    assert writer.open(path)
    assert writer.write_channel("ch", "type.A", "")
    assert writer.write_message("ch", b"abc", 123)
    writer.close()

    reader = autolink.RecordReader(path)
    msg = reader.read_message()
    assert not msg.end
    assert msg.channel_name == "ch"
    assert msg.data == b"abc"
    assert msg.timestamp == 123


if __name__ == "__main__":
    test_record_write_read()
    print("ok")
