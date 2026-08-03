#!/usr/bin/env python3

import threading
import time
import uuid

import pytest

import autolink


@pytest.fixture(scope="module", autouse=True)
def _autolink_lifecycle():
    autolink.init("py_test_action_suite_" + uuid.uuid4().hex[:8])
    yield
    autolink.shutdown()


def test_simple_action_bytes_roundtrip():
    uid = uuid.uuid4().hex[:8]
    node = autolink.Node("action_node_" + uid)
    name = "py/test_action_" + uid
    seen = {"n": 0}

    def execute():
        seen["n"] += 1
        goal = simple.get_current_goal()
        simple.publish_feedback(b"fb")
        simple.succeeded_current(b"ok:" + goal)

    simple = autolink.SimpleActionServer(node, name, execute)
    client = autolink.ActionClient(node, name)
    assert client.wait_for_server(timeout_sec=5.0)
    handle = client.send_goal(b"g1", timeout_sec=5.0)
    assert handle is not None
    result = handle.get_result(timeout_sec=10.0)
    assert result == b"ok:g1", result
    time.sleep(0.3)
    assert seen["n"] == 1, seen["n"]


def test_action_async_feedback_and_result():
    uid = uuid.uuid4().hex[:8]
    node = autolink.Node("action_async_" + uid)
    name = "py/test_action_async_" + uid
    feedbacks = []
    done = threading.Event()
    got_handle = threading.Event()
    box = {}

    def execute():
        goal = simple.get_current_goal()
        # Wait briefly so the client's feedback subscription is discovered.
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline and not simple.has_feedback_subscriber():
            time.sleep(0.05)
        simple.publish_feedback(b"fb1")
        time.sleep(0.05)
        simple.succeeded_current(b"async:" + goal)

    simple = autolink.SimpleActionServer(node, name, execute)
    client = autolink.ActionClient(node, name)
    assert client.wait_for_server(timeout_sec=5.0)

    def on_goal(handle):
        box["handle"] = handle
        got_handle.set()

    def on_fb(fb, handle):
        feedbacks.append(fb)

    def on_result(result, code):
        box["result"] = result
        box["code"] = code
        done.set()

    client.send_goal_async(b"g2", goal_response_cb=on_goal, feedback_cb=on_fb,
                           result_cb=on_result)
    assert got_handle.wait(5.0)
    assert box.get("handle") is not None
    assert done.wait(10.0)
    assert box["result"] == b"async:g2"
    assert feedbacks, "expected at least one feedback"


def test_action_cancel():
    uid = uuid.uuid4().hex[:8]
    node = autolink.Node("action_cancel_" + uid)
    name = "py/test_action_cancel_" + uid
    started = threading.Event()

    def execute():
        started.set()
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if simple.is_cancel_requested():
                simple.terminate_current(b"canceled")
                return
            time.sleep(0.05)
        simple.succeeded_current(b"too-late")

    simple = autolink.SimpleActionServer(node, name, execute)
    client = autolink.ActionClient(node, name)
    assert client.wait_for_server(timeout_sec=5.0)
    handle = client.send_goal(b"cancel-me", timeout_sec=5.0)
    assert handle is not None
    assert started.wait(5.0)
    assert handle.cancel(timeout_sec=5.0)
    result = handle.get_result(timeout_sec=10.0)
    assert handle.is_canceled() or handle.is_aborted()
    assert result in (b"canceled", b"")


def test_low_level_action_server():
    uid = uuid.uuid4().hex[:8]
    node = autolink.Node("ll_action_" + uid)
    name = "py/test_ll_action_" + uid

    def handle_goal(_goal_id, _goal):
        return autolink.GoalResponse.ACCEPT_AND_EXECUTE

    def handle_cancel(_handle):
        return autolink.CancelResponse.ACCEPT

    def handle_accepted(handle):
        def run():
            handle.execute()
            handle.publish_feedback(b"ll-fb")
            handle.succeed(b"ll:" + handle.get_goal())

        threading.Thread(target=run, daemon=True).start()

    _server = autolink.ActionServer(node, name, handle_goal, handle_cancel,
                                    handle_accepted)
    client = autolink.ActionClient(node, name)
    assert client.wait_for_server(timeout_sec=5.0)
    handle = client.send_goal(b"g3", timeout_sec=5.0)
    assert handle is not None
    assert handle.get_result(timeout_sec=10.0) == b"ll:g3"


def test_protobuf_action_roundtrip():
    pytest.importorskip("google.protobuf")
    from autolink.examples.examples_pb2 import SimpleMessageAction

    Goal = SimpleMessageAction.Goal
    Feedback = SimpleMessageAction.Feedback
    Result = SimpleMessageAction.Result

    uid = uuid.uuid4().hex[:8]
    node = autolink.Node("pb_action_" + uid)
    name = "py/test_pb_action_" + uid

    def execute():
        simple.publish_feedback(Feedback(index=1))
        simple.succeeded_current(Result(success=True))

    simple = autolink.SimpleActionServer(
        node, name, execute, goal_type=Goal, feedback_type=Feedback,
        result_type=Result)
    client = autolink.ActionClient(
        node, name, goal_type=Goal, feedback_type=Feedback, result_type=Result)
    assert client.wait_for_server(timeout_sec=5.0)
    handle = client.send_goal(Goal(text="hello"), timeout_sec=5.0)
    assert handle is not None
    result = handle.get_result(timeout_sec=10.0)
    assert result.success is True


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__, "-q"]))
