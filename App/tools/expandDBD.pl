#!/usr/bin/env perl

use warnings;
use strict;
use 5.010;

use File::Basename qw/basename/;
use File::Spec::Functions qw/catfile/;

my $epicsversion = 3.14;
my $quiet = 0;
my @searchpath = ();
my %filesDone = ();
my @filesInput = ();

while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg =~ /^-\d/) { $epicsversion = substr($arg, 1); }
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
    my $name = shift;

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
    my $name = shift; # dbd file to read
    my $includer = shift; # dbd file and lineno where this dbd file is included.
    my $lineno = shift;   # they are undef for files from command line.

    my $base = basename($name);

    if (exists($filesDone{$base})) {
        if (!$quiet) {
            if ($includer) {
                say STDERR "Info: skipping duplicate file $name included from $includer line $lineno";
            }
            else {
                say STDERR "Info: skipping duplicate file $name from command line";
            }
        }
        return 1;
    }

    if ($base ne "dbCommon.dbd") {
        $filesDone{$base} = 1;
    }

    my $fh;
    if (!(open $fh, "<", finddbd($name))) {
        if ($includer) {
            say STDERR "ERROR: file $name not found in path \"@searchpath\" called from $includer line $lineno";
        }
        else {
            say STDERR "ERROR: file $name not found in path \"@searchpath\"";
        }
        return 0;
    }

    foreach (my $n=1;<$fh>;$n++) {
        chomp;

        if (/^[ \t]*(#|%|$)/) {
            # skip
        }
        elsif (/include[ \t]+"?([^"]*)"?/) {
            return 0 unless scanfile($1, $name, $n);
        }
        elsif (/(registrar|variable|function)[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)/) {
            say "$1($2)" if $epicsversion gt 3.13;
        }
        elsif (/variable[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*,[ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)/) {
            say "variable($1,$2)" if $epicsversion gt 3.13;
        }
        else {
            say;
        }
    }

    return 1;
}

foreach my $name (@filesInput) {
    exit 1 unless scanfile($name);
}
