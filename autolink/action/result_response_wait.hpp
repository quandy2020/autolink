/**
 * Copyright 2026 The Openbot Authors
 *
 * @file result_response_wait.hpp
 * @brief Interrupt an outstanding result-response wait on local monitor release.
 *
 * Leaving this wait does not cancel a remote action or invent terminal evidence.
 * The service future must already represent an asynchronous operation; deferred
 * execution is rejected because calling get() could otherwise block this worker.
 */

#pragma once

#include <chrono>
#include <future>
#include <stdexcept>

namespace autolink::action::internal {

/**
 * @brief Wait for an asynchronous response while its local monitor is active.
 * @tparam ResponseT Value stored by the native service's shared future.
 * @tparam Monitoring Predicate which is true while result monitoring is retained.
 * @param response An already started asynchronous service response.
 * @param monitoring Nonblocking predicate; must not execute a remote operation.
 * @param cadence Maximum duration of one wait before rechecking local release.
 * @return True when ready, false when released without a ready response.
 * @throws std::invalid_argument For invalid futures or nonpositive cadence.
 * @throws std::runtime_error For deferred work, which is not an async response.
 *
 * Readiness wins over local release so an already available real terminal
 * response can still be delivered. The response is never consumed here and its
 * exception, if any, remains intact. There is no overall execution deadline:
 * long-running, retained actions remain monitored. This helper bounds only the
 * polling wait after local release, not service submission or callback duration.
 */
template <typename ResponseT, typename Monitoring>
bool WaitForResultResponse(const std::shared_future<ResponseT>& response,
                           Monitoring monitoring,
                           std::chrono::milliseconds cadence =
                               std::chrono::milliseconds(20)) {
    if (!response.valid() || cadence.count() <= 0)
        throw std::invalid_argument("result wait requires an async future and positive cadence");
    for (;;) {
        const auto ready = response.wait_for(std::chrono::milliseconds(0));
        if (ready == std::future_status::ready) return true;
        if (ready == std::future_status::deferred)
            throw std::runtime_error("deferred result response cannot be monitored asynchronously");
        if (!monitoring()) return false;
        const auto waited = response.wait_for(cadence);
        if (waited == std::future_status::ready) return true;
        if (waited == std::future_status::deferred)
            throw std::runtime_error("deferred result response cannot be monitored asynchronously");
    }
}

}  // namespace autolink::action::internal
