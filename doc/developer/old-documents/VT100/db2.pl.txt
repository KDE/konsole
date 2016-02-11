#!/usr/bin/perl -w

$ops = "db.trans";
$src = "../../src/TEScreen.C";
$res1 = ">TEScreen.p1";
$res2 = ">TEScreen.p2";

open(OPS, $ops) || die "cannot open file '" . $ops . "'.";

my $tbl = {};
while (<OPS>) 
{
  chop;        # strip record separator
  my @Fld = split('\|', $_);
  if ($Fld[2] && $Fld[2] eq 'scr')
  {
    $tbl->{$Fld[3]} = 1;
  }
}
#foreach $p (sort keys %$tbl)
#{ 
#  print $p, "\n";
#}

open(SRC, $src) || die "cannot open file '" . $src . "'.";
open(RES1, $res1) || die "cannot open file '" . $res1 . "'.";
open(RES2, $res2) || die "cannot open file '" . $res2 . "'.";
my $control = 0;
while (<SRC>) 
{
  chop;
  if ( /void TEScreen::(.*)\((.*)\)/ && exists $tbl->{$1} )
  {
    print RES1 "\n";
    $control = 1;
  }
  if ($control)
  {
    print RES1 $_, "\n";
  }
  else
  {
    print RES2 $_, "\n";
  }
  if ( /^}$/ )
  {
    $control = 0;
  }
}
