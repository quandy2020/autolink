#!/usr/bin/env python3
"""Client example (protobuf ChatterBenchmark)."""

import sys
import time

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

try:
    import google.protobuf  # noqa: F401
except ImportError:
    sys.exit("pip install -r autolink/python/requirements.txt")

import autolink
from autolink.proto.unit_test_pb2 import ChatterBenchmark


def test_client_class():
    node = autolink.Node("client_node")
    client = node.create_client("server_01", req_type=ChatterBenchmark,
                                res_type=ChatterBenchmark)
    req = ChatterBenchmark()
    req.content = "clt:Hello service!"
    req.seq = 0
    count = 0
    while not autolink.is_shutdown():
        time.sleep(1)
        count += 1
        req.seq = count
        print("-" * 80)
        response = client.send_request(req)
        print("get Response [ ", response, " ]")


if __name__ == "__main__":
    autolink.init("client_sample")
    test_client_class()
    autolink.shutdown()
