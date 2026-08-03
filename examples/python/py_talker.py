#!/usr/bin/env python3
"""Talker example (protobuf Chatter)."""

import sys
import time

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

try:
    import google.protobuf  # noqa: F401
except ImportError:
    sys.exit("pip install -r autolink/python/requirements.txt")

import autolink
from autolink.proto.unit_test_pb2 import Chatter


def test_talker_class():
    test_node = autolink.Node("node_name1")
    g_count = 1
    writer = test_node.create_writer("channel/chatter", Chatter, qos_depth=6)
    rate = autolink.Rate(1.0)

    print("Starting to publish at 1 Hz (Chatter protobuf)")
    while not autolink.is_shutdown():
        msg = Chatter()
        msg.timestamp = autolink.Time.now().to_nsec()
        msg.lidar_timestamp = msg.timestamp
        msg.seq = g_count
        msg.content = b"I am python talker."
        print("=" * 80)
        print("[%.3f] write msg -> seq: %d, content: %s" %
              (time.time(), msg.seq, msg.content))
        writer.write(msg)
        g_count += 1
        rate.sleep()


if __name__ == "__main__":
    autolink.init("talker_sample")
    test_talker_class()
    autolink.shutdown()
