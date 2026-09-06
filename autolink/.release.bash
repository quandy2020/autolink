#! /usr/bin/env bash
TOP_DIR="$(cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd -P)"
source ${TOP_DIR}/scripts/autolink.bashrc

export AUTOLINK_PATH="${AUTOLINK_ROOT_DIR}/autolink"

pathprepend "${TOP_DIR}/bin"

# Used by RTPS when AUTOLINK_ENABLE_FASTDDS=ON. Topology backend default is local file.
export AUTOLINK_DOMAIN_ID=80
export AUTOLINK_IP=127.0.0.1
# export AUTOLINK_TOPOLOGY_BACKEND=local

export GLOG_log_dir="${AUTOLINK_ROOT_DIR}/data/log"
export GLOG_alsologtostderr=0
export GLOG_colorlogtostderr=1
export GLOG_minloglevel=0

export sysmo_start=0

# for DEBUG log
#export GLOG_v=4
