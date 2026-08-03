#!/usr/bin/env python3

import threading
import time
import uuid

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
from autolink.proto.unit_test_pb2 import Chatter


def test_protobuf_pubsub_same_process():
    uid = uuid.uuid4().hex[:8]
    autolink.init("py_test_pb_pubsub_" + uid)
    received = []
    ev = threading.Event()
    sub = autolink.Node("pb_sub_" + uid)
    pub = autolink.Node("pb_pub_" + uid)
    channel = "py/pb_chatter_" + uid

    def on_msg(msg):
        received.append(msg)
        ev.set()

    sub.create_reader(channel, on_msg, data_type=Chatter)
    writer = pub.create_writer(channel, Chatter, qos_depth=6)
    time.sleep(0.5)
    msg = Chatter(timestamp=1, lidar_timestamp=1, seq=7, content=b"pb-hi")
    deadline = time.monotonic() + 5.0
    while not ev.is_set() and time.monotonic() < deadline:
        writer.write(msg)
        ev.wait(0.05)
    assert ev.is_set(), "no protobuf message received on " + channel
    assert received[0].seq == 7
    assert received[0].content == b"pb-hi"
    autolink.shutdown()


if __name__ == "__main__":
    test_protobuf_pubsub_same_process()
    print("ok")
