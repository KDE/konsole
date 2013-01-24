#!/bin/sh

# The "MenuIndex" entry in profile is expected to be absent or 0.
# However, old versions of konsole might leave some problematic
# "MenuIndex" entries due to buggy code. This script simply removes them.

local_path=`kde4-config --localprefix`
konsole_data_path="${local_path}/share/apps/konsole"

# do nothing when no user data exsit at all
if ! test -d "${konsole_data_path}" ; then
    exit 0
fi

cd "${konsole_data_path}"

for i in *.profile ; do
    # no match?
    if test "$i" = "*.profile" ; then
        break
    fi

    sed -i '/MenuIndex=/d' "$i"
done

