#!/bin/sh

# Starting from konsole 2.9.0, menubar and tabbar setttings are global instead of per profile
# This stupid script migrates those settings from default profile into konsolerc

default_profile_name=`kreadconfig5 --file konsolerc --group 'Desktop Entry' --key DefaultProfile`

# empty name implies that the user use is using the stock profile 
if test -z "$default_profile_name" ; then
    exit 0
fi

default_profile_path=""

OLD_IFS="$IFS"
IFS=":"
for path in `kde4-config --path data`; do
    if test -f "$path/konsole/$default_profile_name"; then
        default_profile_path="$path/konsole/$default_profile_name"
        break
    fi
done
IFS="$OLD_IFS"

# empty path implies that the specified default profile does not exist
if test -z "$default_profile_path" ; then
    exit 0
fi

echo "[KonsoleWindow]"

show_menu_bar=`kreadconfig5 --file "$default_profile_path" --group 'General' --key 'ShowMenuBar'`
if test -n "$show_menu_bar"; then
    echo "ShowMenuBarByDefault=$show_menu_bar"
fi

echo "[TabBar]"

# The order of enum value has changed
tab_bar_visibility=`kreadconfig5 --file "$default_profile_path" --group 'General' --key 'TabBarMode'`
if test -n "$tab_bar_visibility"; then
    new_value=""

    if test "$tab_bar_visibility" = "0" ; then
        new_value="2"
    elif test "$tab_bar_visibility" = "1" ; then
        new_value="1"
    elif test "$tab_bar_visibility" = "2" ; then
        new_value="0"
    fi

    if test -n "$new_value"; then
        echo "TabBarVisibility=$new_value"
    fi
fi

# The order of this enum value has changed
tab_bar_position=`kreadconfig5 --file "$default_profile_path" --group 'General' --key 'TabBarPosition'`
if test -n "$tab_bar_position"; then
    new_value=""
    if test "$tab_bar_position" = "0" ; then
        new_value="1"
    elif test "$tab_bar_position" = "1" ; then
        new_value="0"
    fi

    if test -n "$new_value"; then
        echo "TabBarPosition=$new_value"
    fi
fi


show_quick_buttons=`kreadconfig5 --file "$default_profile_path" --group 'General' --key 'ShowNewAndCloseTabButtons'`
if test -n "$show_quick_buttons"; then
    echo "ShowQuickButtons=$show_quick_buttons"
fi

new_tab_behavior=`kreadconfig5 --file "$default_profile_path" --group 'General' --key 'NewTabBehavior'`
if test -n "$new_tab_behavior"; then
    echo "NewTabBehavior=$new_tab_behavior"
fi

