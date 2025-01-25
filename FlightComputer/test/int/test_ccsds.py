#!/usr/bin/env python3

import time
from enum import Enum
from pathlib import Path

import os
import platform
import subprocess
import time
from enum import Enum
import debugpy
import logging
from dataclasses import dataclass
import pytest

from fprime_gds.common.pipeline.standard import StandardPipeline
from fprime_gds.common.testing_fw import predicates
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.utils.config_manager import ConfigManager
from fprime_gds.common.utils.event_severity import EventSeverity

def assert_and_wait_for_condition(fprime_test_api, channel, condition, assert_timeout, condition_timeout):
    """
    Asserts that telemetry is received within assert_timeout, then waits for a condition to be true within condition_timeout.

    :param fprime_test_api: The fprime test API object
    :param channel: The telemetry channel to monitor
    :param condition: A lambda function that takes the telemetry value and returns a boolean
    :param assert_timeout: Timeout for receiving initial telemetry (in seconds)
    :param condition_timeout: Timeout for the condition to become true (in seconds)
    :return: The telemetry value that satisfied the condition
    """

    # Now, wait for the condition to be true, checking periodically
    start_time = time.time()
    tlm_time = None
    while time.time() - start_time < condition_timeout:
        tlm = fprime_test_api.assert_telemetry(channel, time_pred=tlm_time, timeout=assert_timeout)
        tlm_time = tlm.time
        fprime_test_api.log(f"Received initial telemetry: {tlm.get_val()} @ {tlm.time}")
        fprime_test_api.clear_histories()
        if tlm:
            telemetry_value = tlm.get_val()
            if condition(telemetry_value):
                fprime_test_api.log(f"Condition met: {telemetry_value}")
                return telemetry_value
            else:
                fprime_test_api.log(f"Condition not yet met: {telemetry_value}")
        time.sleep(0.1)  # Wait a short time before checking again

    # If we've reached here, the condition wasn't met within the timeout
    fprime_test_api.log(f"Condition not met within {condition_timeout} seconds")
    raise AssertionError(f"Condition not met within {condition_timeout} seconds")

class TestFSWTelemetry(object):

    @pytest.mark.usefixtures('fprime_test_api')
    def test_receive_telemetry(self, fprime_test_api: IntegrationTestAPI):
        logger = logging.getLogger("int_logger")
        logger.info("waiting telem from dummyCounter")
        # check that we actualy can receive basic tlm
        result = fprime_test_api.await_telemetry("pingRcvr.PR_NumPings")
        logger.info(result)
        assert result is not None
        results = fprime_test_api.await_telemetry_count(10, "pingRcvr.PR_NumPings")
        logger.info(result)

        # Check for updates and continuity
        val = 0
        for res in results:
            tmp = res.get_val()
            logger.info(res)
            assert tmp > val
            val = tmp


    @pytest.mark.usefixtures('fprime_test_api')
    def test_send_receive_telemetry(self, fprime_test_api: IntegrationTestAPI):
        logger = logging.getLogger("int_logger")

        result = fprime_test_api.send_and_assert_telemetry("FlightComputer.ccsdsNode.PING", channels="ccsdsNode.pipelineStats")
        logger.info(result)


        fprime_test_api.send_command("FlightComputer.ccsdsNode.RUN_PIPELINE")
        result = fprime_test_api.await_telemetry("ccsdsNode.pipelineStats")
        logger.info(result)
