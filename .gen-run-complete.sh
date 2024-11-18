#!/bin/bash
_run_sh_completions()
{
  local cur prev opts
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  # Main commands
  local commands="docker-build build inspect exec update teardown topology sync"
  # Options/flags
  local flags="--daemon --debug --as-host --clean --host-thread-ctrl --help"
  # All options combined
  local opts="$commands $flags"

  if [[ ${cur} == -* ]]; then
    # If current word starts with a dash, complete with flags
    COMPREPLY=( $(compgen -W "${flags}" -- ${cur}) )
    return 0
  fi

  case "${prev}" in
    inspect)
      # Container names for inspect command
      local containers="fsw gds"
      COMPREPLY=( $(compgen -W "${containers}" -- ${cur}) )
      return 0
      ;;
    exec)
      # Executable targets
      local targets="gds FlightComputer test"
      COMPREPLY=( $(compgen -W "${targets}" -- ${cur}) )
      return 0
      ;;
    *)
      # Default to main commands and flags
      COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
      return 0
      ;;
  esac
}

complete -F _run_sh_completions run.sh
