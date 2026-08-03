#!/usr/bin/env python3
"""Action client example aligned with examples/cpp/action_talker.cpp."""

from __future__ import annotations

import sys
import time

import _bootstrap_autolink  # noqa: F401

try:
    import google.protobuf  # noqa: F401
except ImportError:
    sys.exit("pip install -r autolink/python/requirements.txt")

import autolink
from autolink.examples.examples_pb2 import SimpleMessageAction

Goal = SimpleMessageAction.Goal
Feedback = SimpleMessageAction.Feedback
Result = SimpleMessageAction.Result

ACTION = "examples/simple_message_action"


def main() -> int:
    autolink.init("py_action_client")
    node = autolink.Node("py_simple_action_client")
    client = autolink.ActionClient(
        node,
        ACTION,
        goal_type=Goal,
        feedback_type=Feedback,
        result_type=Result,
    )

    print(f"Waiting for action server {ACTION!r}...", flush=True)
    if not client.wait_for_server(timeout_sec=30.0):
        print("Action server not ready", flush=True)
        autolink.shutdown()
        return 1

    def on_feedback(fb, _handle):
        print(f"feedback index={fb.index}", flush=True)

    goal = Goal(text="hello from py_action_client")
    print(f"Sending goal text={goal.text!r}", flush=True)
    handle = client.send_goal(goal, feedback_cb=on_feedback, timeout_sec=30.0)
    if handle is None:
        print("Goal rejected / timeout", flush=True)
        autolink.shutdown()
        return 1

    print(f"Goal accepted id={handle.goal_id} status={handle.status}",
          flush=True)
    result = handle.get_result(timeout_sec=60.0)
    print(
        f"Result success={result.success} "
        f"succeeded={handle.is_succeeded()} "
        f"canceled={handle.is_canceled()} "
        f"aborted={handle.is_aborted()}",
        flush=True,
    )
    # brief pause so late feedback threads settle before shutdown
    time.sleep(0.2)
    autolink.shutdown()
    return 0 if handle.is_succeeded() and result.success else 1


if __name__ == "__main__":
    raise SystemExit(main())
