#compdef konsole konsoleprofile

# SPDX-FileCopyrightText: 2022 ivan tkachenko <me@ratijas.tk>
#
# SPDX-License-Identifier: GPL-2.0-or-later

function _konsole_profiles() {
  local -a profiles profiles_descr profiles_comp

  # Technically, built-in profile's "path" is `FALLBACK/`. Let's suggest it just for completeness.
  profiles=( ${(f)"$(_call_program konsole-profiles konsole --list-profiles)"} )
  profiles_descr=( $profiles Built-in )
  profiles_comp=( $profiles FALLBACK/ )

  _describe -t profile 'profile' profiles_descr profiles_comp -o nosort
}

function _konsole_profile_properties() {
  local -A properties
  local -a prop_names prop_types
  local property prop_type ret=1

  # first split by lines, then split in `key : type` pairs, and stuff that into an associative array
  properties=( ${(@s. : .)${(f)"$(_call_program konsole-properties-list konsole --list-profile-properties)"}} )
  prop_names=( ${(@k)properties} )
  prop_types=( ${(@v)properties} )

  # skip all property=value pair up to the last one
  while compset -P 1 '*=*;' ; do done

  if compset -P 1 '*='; then
    property=${${IPREFIX%=}##*;}
    prop_type=${properties[$property]}

    case "$prop_type" in
      (bool)
        _describe -t bool bool '(true false 1 0)' && ret=0
        ;;
      (int)
        _numbers && ret=0
        ;;
      *)
        # XXX: what about other types? QString, QStringList, QColor, QFont
        _describe -t property-value "$prop_type" '()' && ret=0
        ;;
    esac
  else
    _describe -t property-name 'property=value' prop_names -S '=' && ret=0
  fi
  return $ret
}

# Note: '-' denotes mutually exclusive sets of options. '-e' trick was copied from _xterm completions.

case "$service" in
  (konsoleprofile)
    _arguments \
      '1:properties:_konsole_profile_properties'
    ;;
  (konsole)
    _arguments -s \
      '(- *)'{-h,--help}'[Displays help on commandline options]' \
      '(- *)'{-v,--version}'[Displays version information]' \
      \
      - list-profiles \
        '--list-profiles[List the available profiles]' \
      \
      - list-profile-properties \
        '--list-profile-properties[List all the profile properties names and their type (for use with -p)]' \
      \
      - main \
        '(--profile --builtin-profile)--profile=[Name of profile to use for new Konsole instance]:profile:_konsole_profiles' \
        '(--profile --builtin-profile)--builtin-profile[Use the built-in profile instead of the default profile]' \
        '*-p[Change the value of a profile property.]:property=value:_konsole_profile_properties' \
        \
        '--layout=[json layoutfile to be loaded to use for new Konsole instance]:file:_files -g "*.json"' \
        '--workdir=[Set the initial working directory of the new tab or window]:dir:_files -/' \
        '--tabs-from-file=[Create tabs as specified in given tabs configuration file]:file:_files' \
        '(--hold --noclose)'{--hold,--noclose}'[Do not close the initial session automatically when it ends.]' \
        '--new-tab[Create a new tab in an existing window rather than creating a new window]' \
        '--background-mode[Start Konsole in the background and bring to the front when Ctrl+Shift+F12 (by default) is pressed]' \
        '(--separate --nofork)'{--separate,--nofork}'[Run in a separate process]' \
        '--fullscreen[Start Konsole in fullscreen mode]' \
        '--notransparency[Disable transparent backgrounds, even if the system supports them.]' \
        '--force-reuse[Force re-using the existing instance even if it breaks functionality.]' \
        \
        '(--show-menubar --hide-menubar)'--{show,hide}-menubar'[Toggle the menubar, overriding the default setting]' \
        '(--show-tabbar --hide-tabbar)'--{show,hide}-tabbar'[Toggle the tabbar, overriding the default setting]' \
        \
        '-e[Command to execute. This option will catch all following arguments, so use it as the last option.]:program: _command_names -e:*::program arguments: _normal'
    ;;
esac
