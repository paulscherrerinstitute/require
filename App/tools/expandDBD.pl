#!/usr/bin/env perl

use warnings;
use strict;
use 5.010;

use File::Basename qw/basename/;
use File::Spec::Functions qw/catfile/;

my $epicsversion = 3.14;
my $quiet = 0;
my @searchpath = ();
my @filesDone = ();
my @filesInput = ();

while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg =~ /^-(\d*(\.\d*)?)$/) { $epicsversion = $1; }
    elsif ($arg eq "-q") { $quiet = 1; }
    elsif ($arg =~ /^-I$/) { push @searchpath, shift @ARGV; }
    elsif ($arg =~ /^-I(.*)$/) { push @searchpath, $1; }
    else { push @filesInput, $arg; }
}
##
## Search given dbd file from @searchpath list.
## If it is not found, return the file name as it is.
## 
sub finddbd {
    my $name = $_[0];

    foreach my $dir (@searchpath) {
        my $fullname = catfile($dir, $name);
        if ( -f $fullname) {
            return $fullname;
        }
    }

    return $name;
}

##
## Read a dbd file and dump its contents to stdout.
## File after "include" directive is read in too.
## 
sub scanfile {
    my $fname = $_[0];
    my $name = basename($fname);

    if (grep $name eq $_, @filesDone) {
        if (!$quiet) {
            say STDERR "Info: skipping duplicate file \"$name\" from command line";
        }
        return;
    }

    if ($name ne "dbCommon.dbd") {
        push @filesDone, $name;
    }

    if (!(open FILE, "<", finddbd($name))) {
        say STDERR "Error openning file \"$name\" for reading: $!";
        return;
    }

    foreach my $line (<FILE>) {
        if ($line =~ /^[ \t]*(#|%|$)/) {
            # skip
        }
        elsif ($line =~ /include[ \t]+"?([^"]*)"?/) {
            scanfile($1);
        }
        elsif ($line =~ /(registrar|variable|function)[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)/) {
            if ($epicsversion > 3.13) {
                say "$1($2)";
            }
        }
        elsif ($line =~ /variable[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*,[ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)/) {
            if ($epicsversion > 3.13) {
                say "variable($1,$2)"
            }
        }
        else {
            print $line;
        }
    }

    close FILE;
}

foreach my $fname (@filesInput) {
    scanfile $fname;
}
