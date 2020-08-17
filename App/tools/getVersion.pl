#!/usr/bin/env perl

use warnings;
use strict;
use 5.010;

use File::Glob qw/bsd_glob/;
use IPC::Open3 qw/open3/;
use Symbol qw/gensym/;

# cvs status parsing state
use constant {
    GLOBAL => 0,
    FILE => 1,
    SKIP => 2
};

my $version;
my $debug = 0;

# Check all files in top directory and all files specified explicitly in subdirectories
my @files =  glob("GNUmakefile makefile Makefile *.c *.cc *.cpp *.h *.dbd *.st *.stt *.gt");
# perl glob keeps non-wildcard names even they don't match, which are removed manually.
@files = grep(-f, @files);
my @statusinfo;

while (@ARGV) {
    my $arg = shift @ARGV;
    if ($arg eq "-d") {
        $debug = 1;
    } else {
        push @files, $arg;
    }
}
# concatenate the files list to one space separatd string
my $files = join(' ', @files);

sub check_output {
    my ($child_stdout, $child_stderr);
    $child_stderr = gensym();

    chomp(my $command = $_[0]);

    # start the child process capturing stdout and stderr
    my $child_pid = open3(undef, $child_stdout, $child_stderr, $command);

    # filehandles' bitmask for read
    my $out_set = '';
    vec($out_set, fileno($child_stdout), 1) = 1;
    vec($out_set, fileno($child_stderr), 1) = 1;
    my $num_bits = 2;

    # output and error from the child process
    my $output = '';
    my $error = '';

    while ($num_bits and select(my $rout=$out_set, undef, undef, 5)) {
        for my $fh ($child_stdout, $child_stderr) {
            next unless vec($rout, fileno($fh), 1);

            my $bytes_read = sysread($fh, my $line, 4096);
            if ($bytes_read) {
                if ($fh == $child_stdout) {
                    $output .= $line;
                }
                elsif ($fh == $child_stdout) {
                    $error .= $line;
                }
            }
            else {

                vec($out_set, fileno($fh), 1) = 0;
                $num_bits -= 1;
            }
        }
    }

    waitpid($child_pid, 0);
    die $error if $?;

    return split(/\n/, $output);
}

sub parse_cvs_output {
    my @output = @{$_[0]}; 
    my $scope = GLOBAL;
    my $file;
    my %rev;
    my %tag;
    my ($major, $minor, $patch);

    foreach my $line (@output) {
        chomp($line);
        if ($scope == SKIP) {
            if ($line =~ /=================/) {
                    $scope = GLOBAL;
            }
        }
        elsif ($scope == FILE) {
            if ($line =~ /Working revision:/) {
                $rev{$file} = (split " ", $line)[2];
            }
            elsif ($line =~ /Sticky Tag:.*_([0-9]+)_([0-9]+)(_([0-9]+))?[ \t]+\(revision: /) {
                $major = $1;
                $minor = $2;
                $patch = $4 || 0;
                $tag{$file} = (split " ", $line)[2] . " (sticky)";
                $scope = SKIP;
            }
            elsif ($line =~ /_([0-9]+)_([0-9]+)(_([0-9]+))?[ \t]+\(revision: ([\.0-9]+)\)/) {
                if ($rev{$file} eq $5) {
                    my $Major = $1;
                    my $Minor = $2;
                    my $Patch = $4 || 0;
                    if (!defined($major) ||
                        $Major > $major ||
                        ($Major == $major && ($Minor > $minor
                                || ($Minor == $minor && $Patch > $patch)))) {
                        $major = $Major;
                        $minor = $Minor;
                        $patch = $Patch;
                        $tag{$file} = (split " ", $line)[0];
                    }
                }
            }
            elsif ($line =~ /=================/) {
                if (!defined($major)) {
                    say STDERR "checking $file: revision $rev{$file} not tagged => version test";
                    $version = "test";
                } else {
                    say STDERR "checking $file: revision $rev{$file} tag $tag{$file} => version $major.$minor.$patch";
                    if (!defined($version)) {
                        $version = "$major.$minor.$patch";
                    } else {
                        if ($version ne "$major.$minor.$patch") {
                            $version = "test";
                        }
                    }
                }
                $scope = GLOBAL;
            }
        }
        elsif ($scope == GLOBAL) {
            if ($line =~ /there is no version here/) {
                return;
            }
            elsif ($line =~ /cvs status: failed/) {
                say STDERR "Error: $line";
                return;
            }
            elsif ($line =~ /no such directory `(.*)'/) {
                say STDERR "checking directory $1: no so such directory";
                return;
            }
            elsif ($line =~ /cvs \[status aborted\]: there is no version here/) {
                return;
            }
            elsif ($line =~ /^File: (\S+)\s+Status: Up-to-date/) {
                $file = $1;
                $major = undef();
                $minor = undef();
                $patch = undef();
                $scope = FILE;
            }
            elsif ($line =~ /^File: (\S+)\s+Status: (.*)/) {
                $file = $1;
                say STDERR "checking $file: $2 => verson test";
                $version = "test";
            }
            elsif ($line =~ /^\? .*/) {
                $file = (split " ", $line)[1];
                say STDERR "checking $file: not in cvs => version test";
                $version = "test";
            }
        }
    }
}

sub parse_git_output {
    my @output = @{$_[0]}; 

    foreach my $line (@output) {
        chomp($line);
        if ($line =~ /fatal: Not a git repository/) {
            return;
        }
        elsif ($line =~ /^\?\? (.*)/) {
            say STDERR "$1: not in git => version test";
            $version = "test";
        }
        elsif ($line =~ /^ M (.*)/) {
            say STDERR "$1: locally modified => version test";
            $version = "test";
        }
        elsif ($line =~ /^D  (.*)/) {
            say STDERR "$1: deleted (or renamed) but not committed => version test";
            $version = "test";
        }
        elsif ($line =~ /^ D (.*)/) {
            say STDERR "$1: locally deleted => version test";
            $version = "test";
        }
        elsif ($line =~ /^A  (.*)/) {
            say STDERR "$1: locally added => version test";
            $version = "test";
        }
        elsif ($line =~ /^AM (.*)/) {
            say STDERR "$1: locally added and modified => version test";
            $version = "test";
        }
        elsif ($line =~ /^([ MADRCU][ MADRCU]) (.*)/) {
            say STDERR "$2: $1 (whatever that means) => version test";
            $version = "test";
        }
        elsif ($line =~ /fatal: No names found/) {
            say STDERR "no tag on this version => version test";
            $version = "test";
        }
        elsif ($line =~ /^([0-9]+)\.([0-9]+)(\.([0-9]+))?$/) {
            my $major = $1;
            my $minor = $2;
            my $patch = $4 || "0";
            $version = "$major.$minor.$patch";
            say STDERR "checking tag $line => version $version";
        }
        elsif ($line =~ /[a-zA-Z]+[a-zA-Z0-9]*_([0-9]+)_([0-9]+)(_([0-9]+))?$/) {
            my $major = $1;
            my $minor = $2;
            my $patch = $4 || "0";
            $version = "$major.$minor.$patch";
            say STDERR "checking tag $line => version $version";
        }
        elsif ($line =~ /(.*[0-9]+[_.][0-9]+([_.][0-9]+)?)-([0-9]+)-g/) {
            $version = "test";
            say STDERR "tag $1 is $3 commits old => version test";
        }
        elsif ($line =~ /Your branch is ahead of '(.*)\/(.*)'/) {
            say STDERR "branch \"$2\" not yet pushed to remote \"$1\" => version test";
            say STDERR "try: git push --tags $1 $2";
            $version = "test";
        }
    }
}

if ($debug) {
    say STDERR "checking $files";
}

eval {
    # fails if git command exits with error
    if ($debug) {
        say STDERR "git status --porcelain $files"
    }
    @statusinfo = check_output("git status --porcelain $files");
    parse_git_output(\@statusinfo);
    if ($version) {
        say $version;
        exit;
    }

    if ($debug) {
        say STDERR "git describe --tags HEAD";
    }
    @statusinfo = check_output("git describe --tags HEAD");
    parse_git_output(\@statusinfo);
    if (!defined($version)) {
        say STDERR "Could not find out version tag => version test";
        $version = "test";
    }

    if ($version ne "test") {
        if ($debug) {
            say STDERR "git status";
        }
        @statusinfo = check_output("git status");
        parse_git_output(\@statusinfo);
    }

    say $version;
    exit;
};

eval {
# cvs bug: calling cvs status for files in other directories spoils status
# information for local files.
# fix: check local and non local files separately

    # fails if we have no cvs or server has a problem
    if ($debug) {
        say STDERR "cvs status -l -v $files;"
    }
    @statusinfo = check_output("cvs status -l -v $files");
    # mark the finsh of the last file for the parser
    push @statusinfo, "===================================================================";
    parse_cvs_output(\@statusinfo);
    if (!defined($version)) {
        say STDERR "Could not find out version tag => version test";
        $version = "test";
    }

    say $version;
    exit;
};

say STDERR "No repository found => version test";
say "test";
