#!/usr/bin/perl -w
use strict;

my $source = "Table.Codes";

my $html = 1;
my $test = 0;

# Syntax -----------------------------------------------------------------------
#
# Dotted.Name Text
# Dotted.Name
#   Text
#   Lines
#
# The dotted names have to be unique. Conceptually, they form a tree.
#

# Data Model ------------------------------------------------------------------

# This is currently pretty wierd.
#
# Empirically, we have
#
# NAME.head TitleLine
# NAME.emus { EmuName ... }
# NAME.dflt { Number|'ScreenLines' ... }
# NAME.sect DottedWord
#
# NAME.code <Typ>|<Ide>|<Parm>
# NAME.text
#   <text with some special tricks>
# NAME.table.TAB
#   <"|"-separated head line>
#   <"|"-separated data rows>
#
# TABs
# - .XPS, used for instructions with subcodes
#   Subcode|Emulation|Scope|Operation|Parameter|Meaning
# - .XEX, used for individual codes
#   Instruction|Scope|Operation|Parameter
#
# Alternative
# - .impl Scope|Operation|Parameters
# - .subc.SUBCODE.impl
# - .subc.SUBCODE.attr
# - .subc.SUBCODE.head

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Analyze Source --------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

my $all = {};

my $state = 0;
my $name = '';
my $value = '';

sub parse
{
  sub chkstate
  { my ($expect,$lineno,$line) = @_;
    if ($state != $expect)
    {
      print STDERR "$source($lineno): line unexpected in state $state. : $line\n";
    }
  }

  sub reduce
  {
    $all->{$name} = $value;
    $state = 0;
    $value = '';
  }

  open(CODES, $source) || die "cannot open file '" . $source . "'.";
  while (<CODES>) 
  {
    chop;        # strip record separator
    my @Fld = split(' ', $_);

    if ($#Fld == -1)
    { 
      reduce() if $state != 0;
    }
    elsif (substr($_, 0, 1) eq '#')
    {
      ; #ignore
    }
    elsif (substr($_, 0, 1) eq ' ')
    { &chkstate(1,$.,$_);
      $value .= ($value eq "" ? "" : "\n") . $_; #FIXME: unchop
    }
    else
    {
      reduce() if $state != 0;
      $name = $Fld[0];
      if ($#Fld == 0)
      {
        $state = 1;
      }
      else
      {
        $value = join ' ', @Fld[1..$#Fld];
        reduce();
      }
    }
  }
  reduce() if ($state == 1);
  chkstate(0,$.,$_);

  return $all;
}

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Analyze Source --------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

sub clausify
{
  my ($t) = @_;
  my $p;
  my $word;
  foreach $p (keys %$t)
  {
    my $flag = 0;
    print("dict([");
    foreach $word (split('\.', $p))
    {
      print(", ") if ($flag);
      print("\'$word\'");
      $flag = 1;
    }
    print("], ");
    my @Fld = split('\.', $p);
    #
    # head
    #
    if ($#Fld == 1 && $Fld[1] eq 'head')
    {
      print "\"",$t->{$p},"\"";
    }
    #
    # emus
    #
    elsif ($#Fld == 1 && $Fld[1] eq 'emus')
    {
      my $emu;
      $flag = 0;
      print("[");
      foreach $emu (split(' ', $t->{$p}))
      {
        print ", " if ($flag);
        print "\'$emu\'";
      $flag = 1;
      }
      print("]");
    }
    #
    # dflt
    #
    elsif ($#Fld == 1 && $Fld[1] eq 'dflt')
    {
      my $dflt;
      $flag = 0;
      print("[");
      foreach $dflt (split(' ', $t->{$p}))
      {
        print ", " if ($flag);
        print $dflt if (length($dflt) == 1); 
        print "\'$dflt\'" if (length($dflt) > 1); 
      $flag = 1;
      }
      print("]");
    }
    #
    # sect
    #
    elsif ($#Fld == 1 && $Fld[1] eq 'sect')
    {
      my $sect;
      $flag = 0;
      print("[");
      foreach $sect (split('\.', $t->{$p}))
      {
        print ", " if ($flag);
        print "\'$sect\'";
      $flag = 1;
      }
      print("]");
    }
    #
    # code
    #
    elsif ($#Fld == 1 && $Fld[1] eq 'code')
    {
      my @Code = split('\|', $t->{$p});
      print("[");
      print("\'$Code[0]\', ");
      if ($#Code > 0 && $Code[1] ne '')
      {
        print("\"$Code[1]\", ") if ($Code[0] ne 'CTL');
        printf("%d, ",eval($Code[1])) if ($Code[0] eq 'CTL');
      }
      else
      {
        print("none, ")
      }
      if ($#Code == 2 && $Code[2] ne '' && $Code[0] ne 'PRN')
      {
        $_ = $Code[2];
        s/{/['/;
        s/}/']/;
        s/;/','/g;
        s/'([0-9]+)'/$1/g;
        print $_;
      }
      else
      {
        print "[]";
      }
      print("]");
    }
    #
    # text
    #
    elsif ($#Fld == 1 && $Fld[1] eq 'text')
    {
      my $text;
      $flag = 0;
      print("[");
      foreach $text (split('\n', $t->{$p}))
      {
        print ", " if ($flag);
        $_ = $text;
        s/^  //;
        s/"/\\"/g;
        s/\\ref:([A-Z0-9]+)/", ref('$1'), "/g;
        print "\n  \"$_\"" if ($_ ne '.');
        print "\n  nl" if ($_ eq '.');
      $flag = 1;
      }
      print("]");
    }
    #
    # table.* - subcodes
    #
    #elsif ($#Fld == 2 && $Fld[1] eq 'table' && $Fld[2] eq 'XPS')
    elsif ($#Fld >= 1 && $Fld[1] eq 'table')
    {
      my $text;
      $flag = 0;
      print("[");
      foreach $text (split('\n', $t->{$p}))
      {
        print ",\n  " if ($flag);
        $_ = $text;
        s/^  //;
        my $flag2 = 0;
        my $col;
        print("[");
        foreach $col (split('\|', $_))
        {
          print ", " if ($flag2);
          $_ = $col;
          s/'/\\'/g;
          print "\'$_\'";
          $flag2 = 1;
        }
        print("]");
        $flag = 1;
      }
      print("]");
    }
    #
    # other (text, tables)
    #
    else
    {
      print("other");
    }
    print(").\n");
  }
}

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# MAIN ------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

my $t = parse();
my $p;
my $table = 0;

clausify($t);
