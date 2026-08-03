#!/usr/bin/env python3
"""Module for example of timer."""

import time

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink

count = 0


def fun():
    global count
    print("cb fun is called:", count)
    count += 1


def test_timer():
    autolink.init("timer_sample")
    ct = autolink.Timer(10, fun, False)  # 10ms
    ct.start()
    time.sleep(1)
    ct.stop()
    autolink.shutdown()


if __name__ == "__main__":
    test_timer()
