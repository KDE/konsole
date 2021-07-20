Manual testcases
----------------

This directory contains various files and programs for manual testing of issues
discovered in the past.

The legacy directory contains some tests whose purpose have been lost to time.
If you know how to use them please update this README.


Text files
==========

Just run `cat filename` in Konsole to verify that they are working. If
rendering related, open in a competent graphical text editor to compare and/or
in other terminal emulators.

 - [Various URLs](text-files/cat_test_urls.txt)
 - [Emojis](text-files/emoji_test.txt)
 - [Box characters](text-files/boxes.txt)
 - [URL escape sequences](text-files/url-escape-sequences.txt): Hides the real URL that is opened if clicked.
 - [Characters available in 9x15, encoded in UTF-8](text-files/9x15.repertoire-utf8.txt)
 - [UTF-8 decomposed hangul character](textfiles/text-files/decomposed-hangul.txt): Testcase for commit 43744097.
 - [UTF-8 test file from the Kermit project](text-files/GLASS.utf8.txt)
 - [Markus Kuhn's UTF-8 sample](text-files/UTF-8-demo.txt)
 - [UTF-8 stress test](text-files/UTF-8-test.txt)


Features
========

 - [SGR formatting](features/sgr2-8-9-53.sh)
 - [Eterm ANSI formatting test, full](features/colortest.sh)
 - [Eterm ANSI formatting test, simplified](features/ct2.sh)
 - [Line block characters](features/line_block_characters_table.py)
 - [Signal tester](features/signaltests.c): Prints the signal it receives.
 - [Set window title](features/titletest.sh)
 - [Turn UTF-8 on or off](features/utf8.sh)


Bugs
====

Running these might trigger hanging or crashing.

 - [Long lines](bugs/bulktest.sh): Prints an infinitely long line.
 - [Combining characters](bugs/combining.py): Prints infinitely long combining character.
 - [Reflow crash](bugs/resize-test.sh): Fills a single line exactly to width.
 - [Spam stderr](bugs/spam-stderr.c): Prints to stderr as fast as possible. Testcase for commit ac59cc7e, bug 230184, "konsole hangs for a long time when lots of stderr output is performed".
 - [Problematic unicode characters](bugs/unicode.py): Testcase for bug 210329, "some unicode characters are displayed as whitespace".


Various
=======
 - [Ripple or reflow related](misc/ripple.c): Fills a specified width x height with text.
