#!/usr/bin/env python3
"""Action server example aligned with examples/cpp/action_listener.cpp."""

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
FEEDBACK_STEPS = 5
FEEDBACK_PERIOD_SEC = 0.2


def main() -> int:
    autolink.init("py_action_server")
    node = autolink.Node("py_simple_action_server")

    def execute() -> None:
        goal = server.get_current_goal()
        print(f"Executing goal text={goal.text!r}", flush=True)
        for i in range(FEEDBACK_STEPS):
            if server.is_cancel_requested():
                server.terminate_current(Result(success=False))
                print("Goal terminated (cancel)", flush=True)
                return
            if server.is_preempt_requested():
                server.accept_pending_goal()
                print("Goal preempted; switching", flush=True)
                return
            server.publish_feedback(Feedback(index=i))
            print(f"feedback index={i}", flush=True)
            time.sleep(FEEDBACK_PERIOD_SEC)
        server.succeeded_current(Result(success=True))
        print("Goal succeeded", flush=True)

    server = autolink.SimpleActionServer(
        node,
        ACTION,
        execute,
        goal_type=Goal,
        feedback_type=Feedback,
        result_type=Result,
    )
    print(f"SimpleActionServer ready on {ACTION}", flush=True)
    try:
        while autolink.ok():
            time.sleep(0.2)
    except KeyboardInterrupt:
        pass
    finally:
        autolink.shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
