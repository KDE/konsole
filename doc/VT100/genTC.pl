#!/usr/bin/perl -w
use strict;

my $source = "Table.Codes";

my $html = 1;
my $test = 0;

# Syntax -----------------------------------------------------------------------
#
# Dotted.Name Text
# Dotted.Name
#   Text Lines
#
# The dotted names have to be unique. Conceptually, they form a tree.
#


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
# Html Layout -----------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

sub head
{
  if ($html)
  {
    print "<table>\n";
    print "<tr><td width=10%%><td width=50%%><td width=40%%>\n";
  }
}

sub tail
{
  if ($html)
  {
    print "</table>\n";
  }
}

my $color1 = " bgcolor=\"#D0D0D0\"";
my $color2 = "";
my $color3 = "";

sub layout 
{ my ($Name, $Head, $Code, $Doku, $Dflt, $Attr) = @_;
  if ($html) 
  {

    print "<tr><td><p></td></tr>\n";

    print "<tr><td $color1><a name=$Name>$Name</a>\n";
    if ($Attr eq '')
    {
      print "    <td $color1 colspan=2><b>$Head</b>\n";
    }
    else
    {
      print "    <td $color1><b>$Head</b>\n";
      print "    <td $color1>$Attr\n";
    }

    if ($Code ne '')
    {
      print "<tr><td><p></td>\n";
      print "<tr><td>\n";
      if ($Dflt eq '')
      {
        print "    <td $color2 colspan=2>", codeToHtml($Code), "\n";
      }
      else
      {
        print "    <td $color2>", codeToHtml($Code), "\n";
        print "    <td $color2>Default: $Dflt\n";
      }
    }

    print "<tr><td><p></td>\n";
    print "<tr><td></td>\n";
    $_ = $Doku;
    s/</&lt;/g;
    s/>/&gt;/g;
    s/\\ref:([A-Z0-9]+)/<a href=#$1>$1<\/a>/g;
    s/\n  \.\n/\n  <p>\n/g;
    print "    <td $color3 colspan=2>\n$_";
  }
  if ($test)
  {
    print "NAME: $Name\n";
    print "TEXT: $Head\n";
    print "CODE: $Code\n";
    print "ATTR: $Attr\n";
    print "DFLT: $Dflt\n";
    # print "DOCU: $Doku\n";
  }
}

sub codeToHtml
{ my ($code) = @_;
  my $res = '<code>';
  foreach (split(' ', $code))
  {
    /^\{(.*)\}$/ && do { $res .= " <em>$1</em>";   next; };
    /^<$/        && do { $res .= ' <b>&lt;</b>'; next; };
    /^>$/        && do { $res .= ' <b>&gt;</b>'; next; };
    $res .= " <b>$_</b>";
  }
  return $res . '</code>';
}

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# MAIN ------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

my $t = parse();
my $p;
my $table = 0;

head();
foreach $p (sort keys %$t)
{
  my @Fld = split('\.', $p);
  if ($#Fld == 1 && $Fld[1] eq 'head')
  {
    print "</table>\n" if ($table);
    my $name = $Fld[0];
    my $head = $t->{$p};
    layout( $name, $head,
            exists $t->{"$name.code"}?$t->{"$name.code"}:"", 
            exists $t->{"$name.text"}?$t->{"$name.text"}:"",
            exists $t->{"$name.dflt"}?$t->{"$name.dflt"}:"",
            exists $t->{"$name.attr"}?$t->{"$name.attr"}:"" );
    $table = 0;
  }
  if ($html && $#Fld == 2 && $Fld[1] eq 'table')
  {
    my $lines = $t->{$p};
    my $line;
    my $field;
    print "<p>\n";
    print "<tr><td></td><td><table width=100%>\n" if (!$table); 
    print "<tr><td $color1>$Fld[2]</td><td $color1>Meaning</td></tr>\n";
    foreach $line (split('\n', $lines))
    {
      print "<tr>\n";
      foreach $field (split('\|',$line))
      {
        print "<td>$field</td>" ;
      }
      print "</tr>\n";
    }
    $table = 1;
  }
}
print "</table>\n" if ($table);
tail();
