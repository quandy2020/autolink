#!/usr/bin/env python3

import time

import autolink


def test_service_roundtrip():
    autolink.init("py_test_srv")
    node = autolink.Node("srv_node")

    def handler(req: bytes) -> bytes:
        return b"echo:" + req

    node.create_service("/py/echo", handler, req_type="RawData",
                        res_type="RawData")
    client = node.create_client("/py/echo", req_type="RawData",
                                res_type="RawData")
    time.sleep(0.3)
    resp = client.send_request(b"ping", timeout_sec=5)
    assert resp == b"echo:ping", resp
    autolink.shutdown()


if __name__ == "__main__":
    test_service_roundtrip()
    print("ok")
