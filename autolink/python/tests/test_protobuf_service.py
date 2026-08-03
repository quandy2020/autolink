#!/usr/bin/env python3

import time

try:
    import google.protobuf  # noqa: F401
except ImportError:
    try:
        import pytest
        pytest.skip("protobuf not installed; pip install -r autolink/python/requirements.txt",
                    allow_module_level=True)
    except ImportError:
        raise SystemExit(
            "protobuf not installed; pip install -r autolink/python/requirements.txt")

import autolink
from autolink.proto.unit_test_pb2 import ChatterBenchmark


def test_protobuf_service_roundtrip():
    autolink.init("py_test_pb_srv")
    node = autolink.Node("pb_srv_node")

    def handler(req):
        return ChatterBenchmark(content="echo:" + req.content, seq=req.seq + 1)

    node.create_service("/py/pb_echo", handler, req_type=ChatterBenchmark,
                        res_type=ChatterBenchmark)
    client = node.create_client("/py/pb_echo", req_type=ChatterBenchmark,
                                res_type=ChatterBenchmark)
    time.sleep(0.3)
    resp = client.send_request(ChatterBenchmark(content="ping", seq=1),
                               timeout_sec=5)
    assert resp.content == "echo:ping"
    assert resp.seq == 2
    autolink.shutdown()


if __name__ == "__main__":
    test_protobuf_service_roundtrip()
    print("ok")
