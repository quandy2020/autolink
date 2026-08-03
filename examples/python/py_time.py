#!/usr/bin/env python3
"""Module for example of autolink time."""

import time

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink


def test_time():
    ct = autolink.Time(123)
    print(ct.to_nsec())
    print(ct.now().to_sec())
    time.sleep(1)
    print(autolink.Time.now().to_sec())
    print(autolink.Time.mono_time().to_sec())


def test_duration():
    td1 = autolink.Duration(111)
    td2 = autolink.Duration(601000000000)
    print(td1, td1.to_nsec())
    print(td2, td2.to_nsec())
    print(td2.to_sec())
    print(td2.is_zero())


def test_rate():
    rt1 = autolink.Rate(111.0)
    rt2 = autolink.Rate(0.2)
    print(rt1)
    print(rt2)


if __name__ == "__main__":
    print("test time", "-" * 50)
    test_time()
    print("test duration", "-" * 50)
    test_duration()
    print("test rate", "-" * 50)
    test_rate()
