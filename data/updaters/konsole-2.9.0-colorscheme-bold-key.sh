#!/bin/sh

# 'Bold=false' is non-functioning before konsole 2.9, but it might
# become functional in the future versions, so remove those entries as
# preparation for future changes.

local_path=`kde4-config --localprefix`
konsole_data_path="${local_path}/share/apps/konsole"

# do nothing when no user data exsit at all
if ! test -d "${konsole_data_path}" ; then
    exit 0
fi

cd "${konsole_data_path}"

for i in *.colorscheme ; do
    # no match? 
    if test "$i" = "*.colorscheme" ; then
        break
    fi

    sed -i '/Bold=false/d' "$i"
done

