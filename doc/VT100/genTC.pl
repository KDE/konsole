#!/usr/bin/perl -w
use strict;

my $source = "Table.Codes";

my $html = 1;
my $test = 0;

# Syntax -----------------------------------------------------------------------
#
# defn NAME Text
#?attr Host-to-VT100 VT100-to-Host Mode Editor-Function Format-Effector
#?code Code
#?dflt Value
# html
#   Text
#
# New Syntax
#
# DEFINEWORD NAME Headline     --> (Type,Name,Head)
# ATTRIBUTE Line               --> (Attr)
# ATTRIBUTE                    --> (Attr)
#   Text
#
# { 'defn' ==> { 'code' ==> 0, 'attr' ==> 0, 'dflt' ==> 0, 'html' ==> 1 } }
#
# Transition to new form.
# 1) parser generates new form && unparser uses this.
# 2) new parser using generic scheme.
#


# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# Analyze Source --------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

sub parse
{
  my $state = 0;
  my $all = {};
  my $rec = {};
  my $docu = '';

  sub chkstate
  { my ($expect,$lineno,$line) = @_;
    if ($state != $expect)
    {
      print STDERR "$source($lineno): line unexpected in state $state. : $line\n";
    }
  }
  sub reduce
  { my ($txt) = @_;
    $rec->{'html'} = $docu;
    $all->{$rec->{'name'}} = $rec;
    $rec = {};
    $state = 0;
    $docu = '';
  }

  open(CODES, $source) || die "cannot open file '" . $source . "'.";
  while (<CODES>) 
  {
    chop;        # strip record separator
    my @Fld = split(' ', $_);

    if ($#Fld == -1)
    { if ($state != 0 && $state != 2)
      { print STDERR "$source($.): empty line in state $state. : $_\n"; }
    }
    elsif (substr($_, 1, 1) eq ' ')
    { &chkstate(2,$.,$_);
      $docu .= ($docu eq "" ? "" : "\n") . $_; #FIXME: unchop
    }
    elsif ($Fld[0] eq 'defn')
    { reduce() if ($state == 2);
      &chkstate(0,$.,$_);
      $rec->{'type'} = $Fld[0];
      $rec->{'name'} = $Fld[1];
      $rec->{'head'} = join ' ', @Fld[2..$#Fld];
      $state = 1;
    }
    elsif (($Fld[0] eq 'code' || $Fld[0] eq 'attr' || $Fld[0] eq 'dflt' ) && 
            $state == 1) 
    { $rec->{$Fld[0]} = join ' ',@Fld[1..$#Fld]; }
    elsif ($Fld[0] eq 'html' && $state == 1) { $state = 2; }
    else
    {
      print STDERR "$source($.): $_\n";
    }
  }

  reduce() if ($state == 2);
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
#   print "<html>\n";
#   print "<body>\n";
    print "<table>\n";
    print "<tr><td width=10%%><td width=50%%><td width=40%%>\n";
  }
}

sub tail
{
  if ($html)
  {
    print "</table>\n";
#   print "</body>\n";
#   print "</html>\n";
  }
}

sub layout 
{ my ($Name, $Head, $Code, $Doku, $Dflt, $Attr) = @_;
  if ($html) 
  {
    my $color1 = " bgcolor=\"#D0D0D0\"";
    my $color2 = "";
    my $color3 = "";

    print "<tr><td><p></td>\n";

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
    print "<tr><td>\n";
    print "    <td $color3 colspan=2>$Doku";
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
    /^ESC$/      && do { $res  = 'ESC';          next; };
    /^\{(.*)\}$/ && do { $res .= " <em>$1</em>"; next; };
    /^<$/        && do { $res .= '&lt;';         next; };
    /^>$/        && do { $res .= '&gt;';         next; };
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

head();
foreach $p (sort keys %$t)
{ my $r = $t->{$p};
  layout( $p, 
          $r->{'head'},
          exists $r->{'code'}?$r->{'code'}:"", 
          exists $r->{'html'}?$r->{'html'}:"",
          exists $r->{'dflt'}?$r->{'dflt'}:"",
          exists $r->{'attr'}?$r->{'attr'}:"" );
}
tail();
