#! /usr/bin/python
import unicodedata

# https://bugs.kde.org/show_bug.cgi?id=96536

print "The same word should be displayed 4 times."
print
u = u'Ha\u0308mikon'
u1 = unicodedata.normalize('NFC', u)
u2 = unicodedata.normalize('NFD', u)
u3 = unicodedata.normalize('NFKD', u)
u4 = unicodedata.normalize('NFKC', u)
print u1, u2, u3, u4



