#!/usr/bin/env python3

import time

import autolink


def test_parameter_native_types():
    autolink.init("py_test_param")
    node = autolink.Node("param_node")
    server = autolink.ParameterServer(node)
    # Keep server alive (also referenced by local var).
    client = autolink.ParameterClient(node, "param_node")
    time.sleep(0.5)
    p = autolink.Parameter("speed", 1.5)
    assert client.set_parameter(p)
    g = client.get_parameter("speed")
    assert abs(g.as_double() - 1.5) < 1e-6
    listed = client.list_parameters()
    assert any(item.name() == "speed" for item in listed)
    autolink.shutdown()


if __name__ == "__main__":
    test_parameter_native_types()
    print("ok")
