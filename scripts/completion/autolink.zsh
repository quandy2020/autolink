#compdef autolink
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
