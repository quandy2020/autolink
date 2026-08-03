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

#include "bind_record.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "autolink/record/record_message.hpp"
#include "autolink/record/record_reader.hpp"
#include "autolink/record/record_writer.hpp"

namespace autolink::python {
namespace {

struct BagMessage {
    uint64_t timestamp = 0;
    std::string channel_name;
    pybind11::bytes data;
    std::string data_type;
    bool end = true;
};

}  // namespace

void BindRecord(pybind11::module_& module) {
    namespace py = pybind11;

    py::class_<BagMessage>(module, "BagMessage")
        .def_readonly("timestamp", &BagMessage::timestamp)
        .def_readonly("channel_name", &BagMessage::channel_name)
        .def_readonly("data", &BagMessage::data)
        .def_readonly("data_type", &BagMessage::data_type)
        .def_readonly("end", &BagMessage::end);

    py::class_<record::RecordReader, std::shared_ptr<record::RecordReader>>(
        module, "RecordReader")
        .def(py::init<const std::string&>(), py::arg("file"))
        .def(
            "read_message",
            [](record::RecordReader& self, uint64_t begin_time,
               uint64_t end_time) {
                BagMessage out;
                record::RecordMessage record_message;
                if (!self.ReadMessage(&record_message, begin_time, end_time)) {
                    out.end = true;
                    return out;
                }
                out.end = false;
                out.channel_name = record_message.channel_name;
                out.data = py::bytes(record_message.content);
                out.timestamp = record_message.time;
                out.data_type = self.GetMessageType(record_message.channel_name);
                return out;
            },
            py::arg("begin_time") = 0,
            py::arg("end_time") = std::numeric_limits<uint64_t>::max())
        .def("get_message_number", &record::RecordReader::GetMessageNumber,
             py::arg("channel_name"))
        .def("get_message_type", &record::RecordReader::GetMessageType,
             py::arg("channel_name"))
        .def("get_proto_desc", &record::RecordReader::GetProtoDesc,
             py::arg("channel_name"))
        .def("reset", &record::RecordReader::Reset)
        .def("get_channel_list", &record::RecordReader::GetChannelList);

    py::class_<record::RecordWriter>(module, "RecordWriter")
        .def(py::init([](uint64_t file_segmentation_size_kb,
                         uint64_t file_segmentation_interval_sec) {
                 auto writer = std::make_unique<record::RecordWriter>();
                 // Match legacy Python API: 0 disables path suffix segmentation.
                 writer->SetSizeOfFileSegmentation(file_segmentation_size_kb);
                 writer->SetIntervalOfFileSegmentation(
                     file_segmentation_interval_sec);
                 return writer.release();
             }),
             py::arg("file_segmentation_size_kb") = 0,
             py::arg("file_segmentation_interval_sec") = 0)
        .def("open", &record::RecordWriter::Open, py::arg("path"))
        .def("close", &record::RecordWriter::Close)
        .def("write_channel", &record::RecordWriter::WriteChannel,
             py::arg("channel"), py::arg("type"), py::arg("proto_desc"))
        .def(
            "write_message",
            [](record::RecordWriter& self, const std::string& channel,
               const py::bytes& raw, uint64_t time,
               const std::string& /*proto_desc*/) {
                return self.WriteMessage(channel, static_cast<std::string>(raw),
                                         time);
            },
            py::arg("channel"), py::arg("rawmessage"), py::arg("time"),
            py::arg("proto_desc") = "")
        .def("set_size_of_file_segmentation",
             &record::RecordWriter::SetSizeOfFileSegmentation,
             py::arg("size_kilobytes"))
        .def("set_interval_of_file_segmentation",
             &record::RecordWriter::SetIntervalOfFileSegmentation,
             py::arg("time_sec"));
}

}  // namespace autolink::python
