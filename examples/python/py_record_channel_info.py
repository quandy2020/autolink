#!/usr/bin/env python3
"""Module for example of record channel info."""

import sys

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink


def print_channel_info(file_path):
    freader = autolink.RecordReader(file_path)
    channels = freader.get_channel_list()

    print("\n++++++++++++Begin Channel Info Statistics++++++++++++++")
    print("-" * 40)
    print("channel count: %d" % len(channels))
    print("-" * 40)
    for i, channel in enumerate(channels, 1):
        desc = freader.get_proto_desc(channel)
        msg_type = freader.get_message_type(channel)
        msg_num = freader.get_message_number(channel)
        print("Channel: %s, #%d, type=%s, msg_num=%d, desc_size=%d" %
              (channel, i, msg_type, msg_num, len(desc)))
    print("++++++++++++Finish Channel Info Statistics++++++++++++++\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: %s record_file" % sys.argv[0])
        sys.exit(0)
    print_channel_info(sys.argv[1])
