import threading
import time

import autolink


def test_bytes_pubsub_same_process():
    autolink.init("py_test_pubsub")
    reader = None
    try:
        received = []
        received_event = threading.Event()
        subscriber = autolink.Node("py_pubsub_subscriber")
        publisher = autolink.Node("py_pubsub_publisher")

        def on_message(data):
            received.append(data)
            received_event.set()

        reader = subscriber.create_reader("py_test_pubsub/channel", on_message)
        writer = publisher.create_writer("py_test_pubsub/channel", "RawData")

        deadline = time.monotonic() + 2.0
        while not received_event.is_set() and time.monotonic() < deadline:
            writer.write(b"hello from pybind11")
            received_event.wait(0.02)

        assert received_event.is_set()
        assert received == [b"hello from pybind11"]
    finally:
        reader = None
        autolink.shutdown()


if __name__ == "__main__":
    test_bytes_pubsub_same_process()
    print("ok")
