#!/usr/bin/env python3
"""Service example (protobuf ChatterBenchmark)."""

import sys

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

try:
    import google.protobuf  # noqa: F401
except ImportError:
    sys.exit("pip install -r autolink/python/requirements.txt")

import autolink
from autolink.proto.unit_test_pb2 import ChatterBenchmark


def callback(data):
    print("-" * 80)
    print("get Request [ ", data, " ]")
    return ChatterBenchmark(content="svr: Hello client!", seq=data.seq + 2)


def test_service_class():
    print("=" * 120)
    node = autolink.Node("service_node")
    node.create_service("server_01", callback, req_type=ChatterBenchmark,
                        res_type=ChatterBenchmark)
    node.spin()


if __name__ == "__main__":
    autolink.init("service_sample")
    test_service_class()
    autolink.shutdown()
