#!/usr/bin/env python3
"""Module for example of parameter."""

import time

from _bootstrap_autolink import setup_autolink_pythonpath

setup_autolink_pythonpath()

import autolink


PARAM_SERVICE_NAME = "global_parameter_service"


def print_param_srv():
    param1 = autolink.Parameter("author_name", "WanderingEarth")
    param2 = autolink.Parameter("author_age", 5000)
    param3 = autolink.Parameter("author_score", 888.88)

    test_node = autolink.Node(PARAM_SERVICE_NAME)
    srv = autolink.ParameterServer(test_node)
    time.sleep(0.3)

    clt = autolink.ParameterClient(test_node, PARAM_SERVICE_NAME)
    clt.set_parameter(param1)
    clt.set_parameter(param2)
    clt.set_parameter(param3)

    param_list = clt.list_parameters()
    print("clt param lst len is ", len(param_list))
    for param in param_list:
        print(param.debug_string())

    print("")
    param_list = srv.list_parameters()
    print("srv param lst len is ", len(param_list))
    for param in param_list:
        print(param.debug_string())


if __name__ == "__main__":
    autolink.init("parameter_sample")
    print_param_srv()
    autolink.shutdown()
