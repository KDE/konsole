# konsole-activation-token.bash
#
# Give GUI applications launched from this interactive shell a fresh
# xdg-activation token, so they can take keyboard focus on Wayland without
# tripping the compositor's focus-stealing prevention.
#
# Requires a Konsole that implements the ActivationToken OSC. Konsole itself
# decides whether a command maps to a GUI application (from its installed
# .desktop files) and only then mints a token, so this client stays simple:
# it asks once per distinct command and remembers the ones that are not GUI
# apps. That keeps the common ls/cd/git workflow free of any per-command
# terminal round-trip after the first time each command is seen.
#
# Source it from ~/.bashrc, e.g.:
#   . /usr/share/konsole/shell-integration/konsole-activation-token.bash

# Commands already known not to map to a GUI app. Skipping these avoids the
# terminal round-trip for the common ls/cd/grep/... case.
declare -gA __konsole_not_gui=()

# Ask Konsole for an activation token for $1 and export it if one comes back.
# An empty reply means "$1 is not a GUI app": cache that and stop asking.
__konsole_request_activation_token() {
    local cmd=$1 saved reply token
    saved=$(stty -g 2>/dev/null) || return
    stty -echo -icanon min 0 time 2          # up to 0.2s, never blocks the prompt
    printf '\033]6969;?;%s\033\\' "$cmd" > /dev/tty
    IFS= read -r -d '\' reply < /dev/tty
    stty "$saved"
    token=${reply#*$'\033]6969;'}            # strip the OSC prefix
    token=${token%$'\033'}                   # strip the trailing ESC of ST
    if [ -n "$token" ]; then
        export XDG_ACTIVATION_TOKEN=$token
    else
        __konsole_not_gui[$cmd]=1
        unset XDG_ACTIVATION_TOKEN
    fi
}

# Runs before each command via the DEBUG trap. Pull the executable name out of
# the command line, skip the negative cache, and otherwise ask Konsole.
__konsole_activation_preexec() {
    local w cmd=
    set -f                                   # no globbing while splitting
    local words=($BASH_COMMAND)
    set +f
    for w in "${words[@]}"; do
        case $w in
            *=*) continue ;;                                        # VAR=value
            sudo|doas|env|nohup|setsid|nice|stdbuf|systemd-run) continue ;; # wrappers
            *) cmd=$w; break ;;
        esac
    done
    [ -n "$cmd" ] || return
    cmd=${cmd##*/}
    [ -n "${__konsole_not_gui[$cmd]}" ] && return   # known non-GUI: no round-trip
    __konsole_request_activation_token "$cmd"
}

# Probe once: only install the hook when the terminal actually answers the OSC.
# This makes the file a no-op in other terminals, so it is safe to source
# unconditionally (an unsupported terminal costs one timed-out read at startup).
__konsole_activation_probe() {
    [ -t 1 ] || return 1
    local saved reply
    saved=$(stty -g 2>/dev/null) || return 1
    stty -echo -icanon min 0 time 2
    printf '\033]6969;?;\033\\' > /dev/tty
    IFS= read -r -d '\' reply < /dev/tty
    stty "$saved"
    [[ $reply == *$'\033]6969;'* ]]
}

if __konsole_activation_probe; then
    trap '__konsole_activation_preexec' DEBUG
fi
unset -f __konsole_activation_probe
