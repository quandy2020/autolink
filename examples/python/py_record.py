#!/usr/bin/env python3
"""Record read/write example (raw bytes)."""

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink


def test_record_writer(writer_path):
    fwriter = autolink.RecordWriter()
    if not fwriter.open(writer_path):
        print("Failed to open record writer!")
        return
    print("+++ Begin to writer +++")
    fwriter.write_channel("chatter_a", "RawData", "")
    fwriter.write_message("chatter_a", b"hello-1", 992)
    fwriter.write_message("chatter_a", b"hello-2", 993)
    fwriter.close()


def test_record_reader(reader_path):
    freader = autolink.RecordReader(reader_path)
    print("+" * 80)
    print("+++ Begin to read +++")
    count = 0
    while True:
        bag = freader.read_message()
        if bag.end:
            break
        count += 1
        print("=" * 80)
        print("read [%d] channel=%s time=%d type=%s data=%r" %
              (count, bag.channel_name, bag.timestamp, bag.data_type, bag.data))


if __name__ == "__main__":
    test_record_file = "/tmp/test_writer.record"
    print("Begin to write record file: {}".format(test_record_file))
    test_record_writer(test_record_file)
    print("Begin to read record file: {}".format(test_record_file))
    test_record_reader(test_record_file)
