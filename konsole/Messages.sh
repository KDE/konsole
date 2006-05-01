#! /bin/sh
rm -f schemas.cpp
(cd .. && $PREPARETIPS > konsole/tips.cpp)
for i in ../other/*.schema; do
grep "^title" $i | cut -d':' -f2- | sed -e 's#^title \(.*\)$#i18n(\"\1\")#' >> schemas.cpp
done
for i in ../other/*.Keytab ../other/*.keytab; do
grep "^keyboard" $i | sed -e 's#^keyboard \"\(.*\)\"$#i18n(\"\1\")#' >> schemas.cpp
done
$XGETTEXT *.cpp -o $podir/konsole.pot
rm -f schemas.cpp
rm -f tips.cpp
