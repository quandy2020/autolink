import time

import autolink


def test_time_now_and_rate():
    autolink.init("py_test_time")
    timer = None
    try:
        t = autolink.Time.now()
        assert t.to_nsec() > 0
        d = autolink.Duration(1_000_000)  # 1ms in ns
        r = autolink.Rate(100.0)
        r.sleep()

        callbacks = []
        timer = autolink.Timer(10, lambda: callbacks.append(True), True)
        timer.start()
        time.sleep(0.1)
        assert callbacks == [True]
    finally:
        if timer is not None:
            timer.stop()
        autolink.shutdown()


if __name__ == "__main__":
    test_time_now_and_rate()
    print("ok")
