/******************************************************************************
 * Copyright 2026 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "autolink/transport/rtps/attributes_filler.hpp"

#include "autolink/common/log.hpp"
#include "autolink/transport/qos/qos_profile_conf.hpp"
#include "fastdds/dds/core/policy/QosPolicies.hpp"

namespace autolink {
namespace transport {

namespace {
using eprosima::fastdds::dds::BEST_EFFORT_RELIABILITY_QOS;
using eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
using eprosima::fastdds::dds::KEEP_LAST_HISTORY_QOS;
using eprosima::fastdds::dds::RELIABLE_RELIABILITY_QOS;
using eprosima::fastdds::dds::TRANSIENT_LOCAL_DURABILITY_QOS;
using eprosima::fastdds::dds::VOLATILE_DURABILITY_QOS;

template <typename QosT>
bool FillHistoryReliabilityDurability(const QosProfile& qos, QosT* out) {
    RETURN_VAL_IF_NULL(out, false);

    switch (qos.history()) {
        case QosHistoryPolicy::HISTORY_KEEP_LAST:
            out->history().kind = KEEP_LAST_HISTORY_QOS;
            break;
        case QosHistoryPolicy::HISTORY_KEEP_ALL:
            out->history().kind = KEEP_ALL_HISTORY_QOS;
            break;
        default:
            break;
    }

    if (qos.depth() != QosProfileConf::QOS_HISTORY_DEPTH_SYSTEM_DEFAULT) {
        out->history().depth = static_cast<int32_t>(qos.depth());
    }
    if (out->history().depth < 0) {
        return false;
    }

    switch (qos.durability()) {
        case QosDurabilityPolicy::DURABILITY_TRANSIENT_LOCAL:
            out->durability().kind = TRANSIENT_LOCAL_DURABILITY_QOS;
            break;
        case QosDurabilityPolicy::DURABILITY_VOLATILE:
            out->durability().kind = VOLATILE_DURABILITY_QOS;
            break;
        default:
            break;
    }

    switch (qos.reliability()) {
        case QosReliabilityPolicy::RELIABILITY_BEST_EFFORT:
            out->reliability().kind = BEST_EFFORT_RELIABILITY_QOS;
            break;
        case QosReliabilityPolicy::RELIABILITY_RELIABLE:
            out->reliability().kind = RELIABLE_RELIABILITY_QOS;
            break;
        default:
            break;
    }

    return true;
}
}  // namespace

bool AttributesFiller::FillInPubQos(
        const QosProfile& qos, eprosima::fastdds::dds::DataWriterQos* wqos) {
    return FillHistoryReliabilityDurability(qos, wqos);
}

bool AttributesFiller::FillInSubQos(
        const QosProfile& qos, eprosima::fastdds::dds::DataReaderQos* rqos) {
    return FillHistoryReliabilityDurability(qos, rqos);
}

}  // namespace transport
}  // namespace autolink
