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
# NAME.head <Title Line>
# NAME.attr { lots ... }
# NAME.dflt Value ...
# NAME.code <Typ>|<Ide>|<Parm>
# NAME.text
#   <text with some special tricks>
# NAME.table.TAB
#   <"|"-separated head line>
#   <"|"-separated data rows>
#
# Section.html
#   <html-text>
#
# TABs
# - .XPS, used for instructions with subcodes
#   Subcode|Emulation|Scope|Operation|Parameter|Meaning

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
    print "<tr><td width=10%%><td><td><td><td><td><td width=40%%>\n";
  }
}

sub tail
{
  if ($html)
  {
    print "</table>\n";
  }
}

my $color1 = " bgcolor=\"#D0D0D0\""; # table head // section head
my $color2 = " bgcolor=\"#5BA5B2\""; # table body (even)
my $color3 = " bgcolor=\"#5188B2\""; # table body (odd)
my $color4 = ""; # code, default
my $color5 = ""; # text

sub txt2Html
{
  my ($Doku) = @_;
  $_ = $Doku;
  s/</&lt;/g;
  s/>/&gt;/g;
  s/\\ref:([A-Z0-9]+)/<a href=#$1>$1<\/a>/g;
  s/\n  \.\n/\n  <p>\n/g;
  return $_;
}

sub layout 
{ my ($Name, $Head, $Code, $Doku, $Dflt, $Attr) = @_;
  if ($html) 
  {

    print "<tr><td><p></td></tr>\n";

    print "<tr><td $color1><a name=$Name>$Name</a>\n";
    if ($Attr eq '')
    {
      print "    <td $color1 colspan=6><b>$Head</b>\n";
    }
    else
    {
      print "    <td colspan=5 $color1><b>$Head</b>\n";
      print "    <td $color1>$Attr\n";
    }

    if ($Code ne '')
    {
      my @Part = split('\|',$Code);
      my $Type = $Part[0];
      my $Indi = $#Part > 0 ? $Part[1] : "";
      my $Parm = $#Part > 1 ? $Part[2] : "";
      $Code = $Parm                 if $Type eq 'PRN';
      $Code = $Indi                 if $Type eq 'CTL';
      $Code = "ESC $Indi"           if $Type eq 'ESC';
      $Code = "0x7f"                if $Type eq 'DEL';
      $Code = "ESC # $Indi"         if $Type eq 'HSH';
      $Code = "ESC $Parm"           if $Type eq 'SCS';
      $Code = "ESC Y $Parm"         if $Type eq 'VT5';
      $Code = "ESC [ $Parm $Indi"   if $Type eq 'CSI';
      $Code = "ESC [ ? $Parm $Indi" if $Type eq 'PRI';
      print "<tr><td><p></td>\n";
      print "<tr><td>\n";
      print "    <td colspan=5 $color4>", codeToHtml($Code), "\n";
      print "    <td $color4>Default: $Dflt\n" if ($Dflt ne '');
    }

    print "<tr><td><p></td>\n";
    print "<tr><td></td>\n";
#   $_ = $Doku;
#   s/</&lt;/g;
#   s/>/&gt;/g;
#   s/\\ref:([A-Z0-9]+)/<a href=#$1>$1<\/a>/g;
#   s/\n  \.\n/\n  <p>\n/g;
    print "    <td $color5 colspan=6>";
    print txt2Html($Doku);
    print "\n";
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

sub secthead
{ my ($Title) = @_;
print "<h2>\n";
print "<table width=100%>\n";
print "<tr><td align=center bgcolor=#d0d0d0></td></tr>\n";
print "<tr><td align=center bgcolor=#d0d0d0>$Title</td></tr>\n";
print "<tr><td align=center bgcolor=#d0d0d0></td></tr>\n";
print "</table>\n";
print "</h2>\n";
}

sub layout2
{ my ($Name, $Head, $Code) = @_;
  my @Part = split('\|',$Code);
  my $Type = $#Part > -1 ? $Part[0] : "";
  my $Indi = $#Part > 0 ? $Part[1] : "";
  my $Parm = $#Part > 1 ? $Part[2] : "";
  if ($Type eq 'CTL')
  {
    $_ = $Indi;
    s/0x00/@/; s/0x01/A/; s/0x02/B/; s/0x03/C/;
    s/0x04/D/; s/0x05/E/; s/0x06/F/; s/0x07/G/;
    s/0x08/H/; s/0x09/I/; s/0x0a/J/; s/0x0b/K/;
    s/0x0c/L/; s/0x0d/M/; s/0x0e/N/; s/0x0f/O/;
    s/0x10/P/; s/0x11/Q/; s/0x12/R/; s/0x13/S/;
    s/0x14/T/; s/0x15/U/; s/0x16/V/; s/0x17/W/;
    s/0x18/X/; s/0x19/Y/; s/0x1a/Z/; s/0x1b/[/;
    s/0x1c/\\/; s/0x1d/]/; s/0x1e/^/; s/0x1f/_/;
    $Indi = $_;
  }
  print "<tr>\n";
  print "<td $color1><a href=#$Name>$Name</a>\n";
  print "<td $color1>$Type\n";
  print "<td $color1>$Indi\n";
  print "<td $color1>$Parm\n";
  print "<td $color1>$Head\n";
}

sub layoutTable
{
  my ($Head, $t, $Include) = @_;
  my $p;
print "<tr><td colspan=5><h3>$Head</h3>\n";
foreach $p (sort keys %$t)
{
  my @Fld = split('\.', $p);
  if ($#Fld == 1 && $Fld[1] eq 'head')
  {
    my $name = $Fld[0];
    my $head = $t->{$p};
    my $attr = exists $t->{"$name.sect"}?$t->{"$name.sect"}:"";
    if ($attr =~ /$Include/)
    {
    layout2( $name, $head, exists $t->{"$name.code"}?$t->{"$name.code"}:"");
    }
  }
}
}

sub sortTest
{
  my ($t) = @_;
  my $p;
  my $s = {};
  my $n = {};
  my $curr = "";
  foreach $p (keys %$t)
  {
    my @Fld = split('\.', $p);
    if ($#Fld == 1 && $Fld[1] eq 'head')
    {
      my $name = $Fld[0];
      if (exists $t->{"$name.code"})
      {
        $s->{$t->{"$name.code"}} = $name;
      }
    }
  }
  print "<table>\n";
  foreach $p (sort keys %$s)
  {
    my $name = $s->{$p};
    my @Fld = split('\|', $p);
    if ($Fld[0] ne $curr)
    {
      print "<tr><td colspan=5><h3>$Fld[0] codes</h3>\n";
    }
    $curr = $Fld[0];
    layout2($name,$t->{"$name.head"},$p);
  }
  print "</table>\n";
}

sub htmlsect
{
  my ($h) = @_;
  $_ = $all->{"$h.html"};
  s/\n  \.\n/\n  <p>\n/g;
  print "$_\n";
}

# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# MAIN ------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

my $t = parse();
my $p;
my $table = 0;

# -------------------------------
htmlsect("Introduction");
# -------------------------------
secthead("Control Sequences");
htmlsect("Sequences");
# -------------------------------
secthead("Host to Terminal (Instructions by Code)");
sortTest($t);
# -------------------------------
secthead("Host to Terminal (Instructions by Group)");
htmlsect("Operations");
print "<table>\n";
layoutTable("Commands (Character Display Operation)",$t,"Command\.Display");
layoutTable("Commands (Rendition related status)",$t,"Command\.RenderMode");
layoutTable("Commands (Cursor)",$t,"Command\.Cursor");
layoutTable("Commands (Cursor related status)",$t,"Command\.CursMode");
layoutTable("Commands (Edit)",$t,"Command\.Erase|Command\.Insert|Command.\Delete");
layoutTable("Commands (Miscellaneous)",$t,"Command[^.]|Command\$");
layoutTable("Commands (General mode setting)",$t,"Command\.SetMode");
layoutTable("Commands (Miscellaneous status)",$t,"Command\.Mode");
layoutTable("Commands (VT52)",$t,"Command\.VT52");
layoutTable("Commands (Not implemented)",$t,"Command\.NoImp");
layoutTable("Commands (Ignored)",$t,"Command\.Ignored");
layoutTable("Commands (Requests)",$t,"Command\.Request");
print "</table>\n";
# -------------------------------
secthead("Terminal to Host");
print "<table>\n";
layoutTable("Replies",$t,"Reply");
layoutTable("Events",$t,"Event");
# -------------------------------
print "</table>\n";
secthead("Modes");
print "<table>\n";
layoutTable("Modes",$t,"Mode");
#print "<h3>Other Codes</h3>\n";
print "</table>\n";
# -------------------------------
secthead("Appendix A - Notion Details");
htmlsect("ConceptDB");
# -------------------------------

head();
foreach $p (sort keys %$t)
{
  my @Fld = split('\.', $p);
  if ($#Fld == 1 && $Fld[1] eq 'head')
  {
#   print "</table>\n" if ($table);
    my $name = $Fld[0];
    my $head = $t->{$p};
    layout( $name, $head,
            exists $t->{"$name.code"}?$t->{"$name.code"}:"", 
            exists $t->{"$name.text"}?$t->{"$name.text"}:"",
            exists $t->{"$name.dflt"}?$t->{"$name.dflt"}:"",
            exists $t->{"$name.emus"}?$t->{"$name.emus"}:"" );
    $table = 0;
  }
  if ($html && $#Fld == 2 && $Fld[1] eq 'table')
  {
    my $lines = $t->{$p};
    my $line;
    my $field;
    my @fldspan = ();
    my $ln = 0;
    print "<tr><td><p></td></tr>\n";
#   print "<tr><td $color1>$Fld[2]</td><td $color1>Meaning</td></tr>\n";
    foreach $line (split('\n', $lines))
    {
      my $fn = 0; 
      @fldspan =  split('\|',$line) if ($ln == 0);
      print "<tr>\n";
      print "<td></td>\n";
      foreach $field (split('\|',$line))
      {
        if ($ln == 0)
        {
          my @Parts = split(":",$field);
          $field = $Parts[0];
          $fldspan[$fn] = ($#Parts > 0) ? $Parts[1] : 1;
        }
        print "<td";
        printf(" colspan=%s",$fldspan[$fn]);
        print " $color1" if ($ln == 0);
        print " $color2" if ($ln > 0 && $ln % 2 == 0);
        print " $color3" if ($ln > 0 && $ln % 2 == 1);
        print ">";
        print txt2Html($field);
        print "</td>";
        $fn += 1;
      }
      print "</tr>\n";
      $ln += 1;
    }
    $table = 1;
  }
}
tail();
