#!/usr/bin/perl
foreach (<>) {
    if(/^schema=.*\/(.*)$/) {
        print "schema=$1\n"; 
        next;
    }
    print $_;
}
