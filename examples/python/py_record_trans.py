#!/usr/bin/env python3
"""Module for example of record trans."""

import sys

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink

TEST_RECORD_FILE = "trans_ret.record"


def test_record_trans(reader_path):
    fwriter = autolink.RecordWriter()
    if not fwriter.open(TEST_RECORD_FILE):
        print("Failed to open record writer!")
        return
    print("+++ Begin to trans +++")

    fread = autolink.RecordReader(reader_path)
    count = 0
    while True:
        bag = fread.read_message()
        if bag.end:
            break
        desc = fread.get_proto_desc(bag.channel_name)
        fwriter.write_channel(bag.channel_name, bag.data_type, desc)
        fwriter.write_message(bag.channel_name, bag.data, bag.timestamp)
        count += 1
    fwriter.close()
    print("-" * 80)
    print("Message count: %d" % count)
    channel_list = fread.get_channel_list()
    print("Channel count: %d" % len(channel_list))
    print(channel_list)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: %s record_file" % sys.argv[0])
        sys.exit(0)
    test_record_trans(sys.argv[1])
