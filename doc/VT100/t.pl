#!/usr/bin/perl

# This script is here only as a pattern for maintainance works.
# It has changing contents and is of no use to anyone but me.

$source = "Table.Codes";

open(CODES, $source) || die "cannot open file '" . $source . "'.";

while (<CODES>) 
{
  if (/^attr/)
  {
    s/VT100/VT100 ANSI/ if (!/DEC/);
  }
  print $_;
}
