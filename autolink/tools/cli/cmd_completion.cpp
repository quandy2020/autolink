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

#include "autolink/tools/cli/cmd_completion.hpp"

#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

namespace {

// Keep in sync with scripts/completion/autolink.bash
constexpr const char kBashCompletion[] = R"bash(# autolink bash completion
# Usage: eval "$(autolink completion bash)"
#    or: source /path/to/scripts/completion/autolink.bash

_autolink_completions() {
  local cur prev words cword
  if declare -F _init_completion >/dev/null 2>&1; then
    _init_completion -n : || return
  else
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
  fi

  local top="channel node service action param recorder launch monitor doctor completion"
  local i=1 cmd="" sub=""
  while [[ $i -lt ${#COMP_WORDS[@]} ]]; do
    local w="${COMP_WORDS[$i]}"
    ((i++))
    [[ "$w" == -* ]] && continue
    if [[ -z "$cmd" ]]; then
      cmd="$w"
      continue
    fi
    if [[ -z "$sub" ]]; then
      sub="$w"
      break
    fi
  done

  if [[ -z "$cmd" ]]; then
    COMPREPLY=( $(compgen -W "--wait --help -h ${top}" -- "$cur") )
    return
  fi

  if [[ -z "$sub" ]]; then
    case "$cmd" in
      channel) COMPREPLY=( $(compgen -W "list info echo hz bw type pub" -- "$cur") ) ;;
      node) COMPREPLY=( $(compgen -W "list info" -- "$cur") ) ;;
      service) COMPREPLY=( $(compgen -W "list info call" -- "$cur") ) ;;
      action) COMPREPLY=( $(compgen -W "list info send_goal" -- "$cur") ) ;;
      param) COMPREPLY=( $(compgen -W "list get set" -- "$cur") ) ;;
      recorder) COMPREPLY=( $(compgen -W "info play record split recover" -- "$cur") ) ;;
      launch) COMPREPLY=( $(compgen -W "start stop list" -- "$cur") ) ;;
      monitor|doctor) COMPREPLY=( $(compgen -W "--help -h" -- "$cur") ) ;;
      completion) COMPREPLY=( $(compgen -W "bash zsh" -- "$cur") ) ;;
      *) COMPREPLY=() ;;
    esac
    return
  fi

  case "$cmd $sub" in
    "channel list") COMPREPLY=( $(compgen -W "-v --verbose --help -h" -- "$cur") ) ;;
    "channel info") COMPREPLY=( $(compgen -W "-a --all --help -h" -- "$cur") ) ;;
    "channel echo") COMPREPLY=( $(compgen -W "--once -n --times --help -h" -- "$cur") ) ;;
    "channel pub"|"service call"|"action send_goal")
      COMPREPLY=( $(compgen -W "--type --descriptor-set --help -h" -- "$cur") ) ;;
    "channel hz"|"channel bw") COMPREPLY=( $(compgen -W "-w --window --help -h" -- "$cur") ) ;;
    "service call") COMPREPLY=( $(compgen -W "--type --descriptor-set --timeout --help -h" -- "$cur") ) ;;
    "recorder play"|"recorder record"|"recorder info"|"recorder split"|"recorder recover")
      COMPREPLY=( $(compgen -W "-f --file --help -h" -- "$cur") ) ;;
    "monitor") COMPREPLY=( $(compgen -W "-c --channel --help -h" -- "$cur") ) ;;
    *) COMPREPLY=( $(compgen -W "--help -h" -- "$cur") ) ;;
  esac
}

complete -F _autolink_completions autolink
)bash";

// Keep in sync with scripts/completion/autolink.zsh
constexpr const char kZshCompletion[] = R"zsh(#compdef autolink
# Usage: eval "$(autolink completion zsh)"
#    or: fpath+=(/path/to/scripts/completion); autoload -U compinit; compinit

_autolink() {
  local -a top
  top=(channel node service action param recorder launch monitor doctor completion)

  local cmd sub
  cmd=${words[2]}
  sub=${words[3]}

  if (( CURRENT == 2 )); then
    _arguments \
      '--wait[Seconds to wait for discovery]:seconds:' \
      '(-h --help)'{-h,--help}'[Show help]' \
      "1:command:(${top})"
    return
  fi

  case "$cmd" in
    channel)
      if (( CURRENT == 3 )); then
        _values 'subcommand' list info echo hz bw type pub
      else
        case "$sub" in
          list) _arguments '(-v --verbose)'{-v,--verbose}'[Show message type]' ;;
          info) _arguments '(-a --all)'{-a,--all}'[Show all channels]' ;;
          echo) _arguments '--once[Exit after one message]' '(-n --times)'{-n,--times}'[Exit after N]:n:' ;;
          pub) _arguments '--type[Protobuf type]:type:' '--descriptor-set[FileDescriptorSet]:file:_files' ;;
          hz|bw) _arguments '(-w --window)'{-w,--window}'[Window size]:n:' ;;
        esac
      fi
      ;;
    node)
      (( CURRENT == 3 )) && _values 'subcommand' list info
      ;;
    service)
      if (( CURRENT == 3 )); then
        _values 'subcommand' list info call
      elif [[ "$sub" == call ]]; then
        _arguments '--type[Request type]:type:' '--descriptor-set[FileDescriptorSet]:file:_files' '--timeout[Seconds]:n:'
      fi
      ;;
    action)
      if (( CURRENT == 3 )); then
        _values 'subcommand' list info send_goal
      elif [[ "$sub" == send_goal ]]; then
        _arguments '--type[Goal type]:type:' '--descriptor-set[FileDescriptorSet]:file:_files'
      fi
      ;;
    param)
      (( CURRENT == 3 )) && _values 'subcommand' list get set
      ;;
    recorder)
      (( CURRENT == 3 )) && _values 'subcommand' info play record split recover
      ;;
    launch)
      (( CURRENT == 3 )) && _values 'subcommand' start stop list
      ;;
    completion)
      (( CURRENT == 3 )) && _values 'shell' bash zsh
      ;;
    monitor)
      _arguments '(-c --channel)'{-c,--channel}'[Channel filter]:channel:'
      ;;
  esac
}

compdef _autolink autolink
)zsh";

}  // namespace

namespace autolink {
namespace tools {

void SetupCompletion(CLI::App& app) {
    auto* completion =
        app.add_subcommand("completion", "Print shell completion script");
    completion->require_subcommand(1);

    completion->add_subcommand("bash", "Print bash completion script")
        ->callback([]() { std::cout << kBashCompletion; });

    completion->add_subcommand("zsh", "Print zsh completion script")
        ->callback([]() { std::cout << kZshCompletion; });
}

}  // namespace tools
}  // namespace autolink
