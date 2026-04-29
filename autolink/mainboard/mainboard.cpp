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

#include <string.h>

#include <list>
#include <string>

#include "autolink/common/global_data.hpp"
#include "autolink/common/log.hpp"
#include "autolink/init.hpp"
#include "autolink/mainboard/module_argument.hpp"
#include "autolink/mainboard/module_controller.hpp"
#include "autolink/state.hpp"

using autolink::mainboard::ModuleArgument;
using autolink::mainboard::ModuleController;

int main(int argc, char** argv) {
    // parse the argument
    ModuleArgument module_args;
    module_args.ParseArgument(argc, argv);

    auto dag_list = module_args.GetDAGConfList();

    std::string dag_info;
    for (auto&& i = dag_list.begin(); i != dag_list.end(); i++) {
        size_t pos = 0;
        for (size_t j = 0; j < (*i).length(); j++) {
            pos = ((*i)[j] == '/') ? j : pos;
        }
        if (i != dag_list.begin())
            dag_info += "_";

        if (pos == 0) {
            dag_info += *i;
        } else {
            dag_info += (pos == (*i).length() - 1) ? (*i).substr(pos)
                                                   : (*i).substr(pos + 1);
        }
    }

    if (module_args.GetProcessGroup() !=
        autolink::mainboard::DEFAULT_process_group_) {
        dag_info = module_args.GetProcessGroup();
    }

    // initialize autolink
    autolink::Init(argv[0], dag_info);

    // start module
    ModuleController controller(module_args);
    if (!controller.Init()) {
        controller.Clear();
        AERROR << "module start error.";
        return -1;
    }

    autolink::WaitForShutdown();
    controller.Clear();
    AINFO << "exit mainboard.";

    return 0;
}
