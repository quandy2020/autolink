#!/usr/bin/env python3
"""Listener example (protobuf Chatter)."""

import sys

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

try:
    import google.protobuf  # noqa: F401
except ImportError:
    sys.exit("pip install -r autolink/python/requirements.txt")

import autolink
from autolink.proto.unit_test_pb2 import Chatter


def callback(data):
    print("=" * 80)
    print("py:reader callback msg->:")
    print("seq:", data.seq)
    print("timestamp:", data.timestamp)
    print("content:", data.content)
    print("=" * 80)


def test_listener_class():
    print("=" * 120)
    test_node = autolink.Node("listener")
    test_node.create_reader("channel/chatter", callback, data_type=Chatter)
    test_node.spin()


if __name__ == "__main__":
    autolink.init("listener_sample")
    test_listener_class()
    autolink.shutdown()
