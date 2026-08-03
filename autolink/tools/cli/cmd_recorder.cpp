/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
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

#include "autolink/tools/cli/cmd_recorder.hpp"

#include <csignal>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <CLI/CLI.hpp>

#include "autolink/common/environment.hpp"
#include "autolink/common/file.hpp"
#include "autolink/common/time_conversion.hpp"
#include "autolink/init.hpp"
#include "autolink/state.hpp"
#include "autolink/tools/recorder/info.hpp"
#include "autolink/tools/recorder/player/player.hpp"
#include "autolink/tools/recorder/recorder.hpp"
#include "autolink/tools/recorder/recoverer.hpp"
#include "autolink/tools/recorder/spliter.hpp"

using autolink::common::UnixSecondsToString;
using autolink::record::HeaderBuilder;
using autolink::record::Info;
using autolink::record::Player;
using autolink::record::PlayParam;
using autolink::record::Recorder;
using autolink::record::Recoverer;
using autolink::record::Spliter;

namespace {

std::string ResolvePath(const std::string& path) {
    if (path.empty() || autolink::common::PathIsAbsolute(path)) {
        return path;
    }
    auto abs = autolink::common::GetEnv("PWD") + "/" + path;
    if (std::filesystem::exists(abs) || autolink::common::PathExists(abs)) {
        return abs;
    }
    return path;
}

std::string ResolveOutputPath(const std::string& path) {
    if (path.empty() || autolink::common::PathIsAbsolute(path)) {
        return path;
    }
    return autolink::common::GetEnv("PWD") + "/" + path;
}

void Fail(const std::string& msg) {
    std::cout << msg << std::endl;
    throw CLI::RuntimeError(-1);
}

}  // namespace

namespace autolink {
namespace tools {

void SetupRecorder(CLI::App& app) {
    auto* recorder =
        app.add_subcommand("recorder", "Record and play Autolink records");
    recorder->require_subcommand(1);

    auto* info = recorder->add_subcommand("info", "Show record information");
    auto info_file = std::make_shared<std::string>();
    info->add_option("file", *info_file, "Record file")->required();
    info->callback([info_file]() {
        std::string path = ResolvePath(*info_file);
        ::autolink::Init("autolink");
        Info info_tool;
        if (!info_tool.Display(path)) {
            throw CLI::RuntimeError(-1);
        }
    });

    auto* play = recorder->add_subcommand("play", "Play a record");
    auto play_files = std::make_shared<std::vector<std::string>>();
    auto play_all = std::make_shared<bool>(false);
    auto play_loop = std::make_shared<bool>(false);
    auto play_rate = std::make_shared<float>(1.0f);
    auto play_begin = std::make_shared<uint64_t>(0);
    auto play_end =
        std::make_shared<uint64_t>(std::numeric_limits<uint64_t>::max());
    auto play_start = std::make_shared<double>(0);
    auto play_delay = std::make_shared<uint64_t>(0);
    auto play_preload = std::make_shared<uint32_t>(3);
    auto play_white = std::make_shared<std::vector<std::string>>();
    auto play_black = std::make_shared<std::vector<std::string>>();
    auto play_begin_str = std::make_shared<std::string>();
    auto play_end_str = std::make_shared<std::string>();
    play->add_option("-f,--files", *play_files, "Input record file(s)")
        ->required();
    play->add_flag("-a,--all", *play_all, "Play all channels");
    play->add_flag("-l,--loop", *play_loop, "Loop playback");
    play->add_option("-r,--rate", *play_rate, "Playback rate");
    play->add_option("-b,--begin", *play_begin_str, "Begin time");
    play->add_option("-e,--end", *play_end_str, "End time");
    play->add_option("-s,--start", *play_start, "Start offset seconds");
    play->add_option("-d,--delay", *play_delay, "Delay seconds");
    play->add_option("-p,--preload", *play_preload, "Preload seconds");
    play->add_option("-c,--white-channel", *play_white, "White channel(s)");
    play->add_option("-k,--black-channel", *play_black, "Black channel(s)");
    play->callback([=]() {
        if (!play_begin_str->empty()) {
            *play_begin = autolink::common::StringToUnixSeconds(*play_begin_str) *
                          1000ULL * 1000ULL * 1000ULL;
        }
        if (!play_end_str->empty()) {
            *play_end = autolink::common::StringToUnixSeconds(*play_end_str) *
                        1000ULL * 1000ULL * 1000ULL;
        }
        std::vector<std::string> files;
        for (const auto& f : *play_files) {
            files.push_back(ResolvePath(f));
        }
        ::autolink::Init("autolink", "recorder");
        PlayParam play_param;
        play_param.is_play_all_channels =
            *play_all || play_white->empty();
        play_param.is_loop_playback = *play_loop;
        play_param.play_rate = *play_rate;
        play_param.begin_time_ns = *play_begin;
        play_param.end_time_ns = *play_end;
        play_param.start_time_s = *play_start;
        play_param.delay_time_s = *play_delay;
        play_param.preload_time_s = *play_preload;
        play_param.files_to_play.insert(files.begin(), files.end());
        play_param.black_channels.insert(play_black->begin(), play_black->end());
        play_param.channels_to_play.insert(play_white->begin(),
                                           play_white->end());
        Player player(play_param);
        if (!(player.Init() && player.Start())) {
            throw CLI::RuntimeError(-1);
        }
    });

    auto* record = recorder->add_subcommand("record", "Record channels");
    auto rec_output = std::make_shared<std::string>();
    auto rec_all = std::make_shared<bool>(false);
    auto rec_white = std::make_shared<std::vector<std::string>>();
    auto rec_black = std::make_shared<std::vector<std::string>>();
    auto rec_interval = std::make_shared<int>(-1);
    auto rec_size_mb = std::make_shared<int>(-1);
    record->add_option("-o,--output", *rec_output, "Output record file");
    record->add_flag("-a,--all", *rec_all, "Record all channels");
    record->add_option("-c,--white-channel", *rec_white, "White channel(s)");
    record->add_option("-k,--black-channel", *rec_black, "Black channel(s)");
    record->add_option("-i,--segment-interval", *rec_interval,
                       "Segment interval seconds");
    record->add_option("-m,--segment-size", *rec_size_mb,
                       "Segment size megabytes");
    record->callback([=]() {
        if (rec_white->empty() && !*rec_all) {
            Fail(
                "MUST specify channels option (-c) or all channels option "
                "(-a).");
        }
        std::string output = *rec_output;
        if (output.empty()) {
            output = autolink::common::GetEnv("PWD") + "/" +
                     UnixSecondsToString(time(nullptr), "%Y%m%d%H%M%S") +
                     ".record";
        } else {
            output = ResolveOutputPath(output);
        }
        auto opt_header = HeaderBuilder::GetHeader();
        if (*rec_interval >= 0) {
            opt_header.set_segment_interval(
                static_cast<uint64_t>(*rec_interval) * 1000000000ULL);
        }
        if (*rec_size_mb >= 0) {
            opt_header.set_segment_raw_size(
                static_cast<uint64_t>(*rec_size_mb) * 1024ULL * 1024ULL);
        }
        ::autolink::Init("autolink");
        auto recorder_ptr = std::make_shared<Recorder>(
            output, *rec_all, *rec_white, *rec_black, opt_header);
        std::signal(SIGTERM, [](int sig) { autolink::OnShutdown(sig); });
        std::signal(SIGINT, [](int sig) { autolink::OnShutdown(sig); });
        bool ok = recorder_ptr->Start();
        if (ok) {
            while (!::autolink::IsShutdown()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            ok = recorder_ptr->Stop();
        }
        if (!ok) {
            throw CLI::RuntimeError(-1);
        }
    });

    auto* split = recorder->add_subcommand("split", "Split a record");
    auto split_files = std::make_shared<std::vector<std::string>>();
    auto split_output = std::make_shared<std::string>();
    auto split_white = std::make_shared<std::vector<std::string>>();
    auto split_black = std::make_shared<std::vector<std::string>>();
    auto split_begin_str = std::make_shared<std::string>();
    auto split_end_str = std::make_shared<std::string>();
    split->add_option("-f,--files", *split_files, "Input record file")
        ->required();
    split->add_option("-o,--output", *split_output, "Output record file");
    split->add_option("-c,--white-channel", *split_white, "White channel(s)");
    split->add_option("-k,--black-channel", *split_black, "Black channel(s)");
    split->add_option("-b,--begin", *split_begin_str, "Begin time");
    split->add_option("-e,--end", *split_end_str, "End time");
    split->callback([=]() {
        if (split_files->empty()) {
            Fail("Must specify file option (-f).");
        }
        if (split_files->size() > 1) {
            Fail("Too many input/output file option (-f/-o).");
        }
        std::string input = ResolvePath((*split_files)[0]);
        std::string output = *split_output;
        if (output.empty()) {
            output = input + ".split";
        } else {
            output = ResolveOutputPath(output);
        }
        uint64_t begin = 0;
        uint64_t end = std::numeric_limits<uint64_t>::max();
        if (!split_begin_str->empty()) {
            begin = autolink::common::StringToUnixSeconds(*split_begin_str) *
                    1000ULL * 1000ULL * 1000ULL;
        }
        if (!split_end_str->empty()) {
            end = autolink::common::StringToUnixSeconds(*split_end_str) *
                  1000ULL * 1000ULL * 1000ULL;
        }
        ::autolink::Init("autolink");
        Spliter spliter(input, output, *split_white, *split_black, begin, end);
        if (!spliter.Proc()) {
            throw CLI::RuntimeError(-1);
        }
    });

    auto* recover = recorder->add_subcommand("recover", "Recover a record");
    auto recover_files = std::make_shared<std::vector<std::string>>();
    auto recover_output = std::make_shared<std::string>();
    recover->add_option("-f,--files", *recover_files, "Input record file")
        ->required();
    recover->add_option("-o,--output", *recover_output, "Output record file");
    recover->callback([=]() {
        if (recover_files->empty()) {
            Fail("MUST specify file option (-f).");
        }
        if (recover_files->size() > 1) {
            Fail("TOO many input/output file option (-f/-o).");
        }
        std::string input = ResolvePath((*recover_files)[0]);
        std::string output = *recover_output;
        if (output.empty()) {
            output = input + ".recover";
        } else {
            output = ResolveOutputPath(output);
        }
        ::autolink::Init("autolink");
        Recoverer recoverer(input, output);
        if (!recoverer.Proc()) {
            throw CLI::RuntimeError(-1);
        }
    });
}

}  // namespace tools
}  // namespace autolink
