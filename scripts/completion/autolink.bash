# autolink bash completion
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
