#!/usr/bin/env perl

use warnings;
use strict;
use 5.010;

use File::Glob qw/bsd_glob/;
use IPC::Open3 qw/open3/;
use Symbol qw/gensym/;

my $version;
my $tag;
my $remote;
my $branch;
my $commit;
my $remotetagcommit;
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
    if ($debug) {
        say STDERR "\$ $command";
    }

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
                else {
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
    if ($debug) {
        say STDERR $output;
    }

    return split(/\n/, $output);
}

sub parse_git_output {
    my @output = @{$_[0]}; 

    foreach my $line (@output) {
        chomp($line);
        if ($line =~ /^fatal: Not a git repository/) {
            return;
        }
        elsif ($line =~ /^\?\? (.*)/) {
            say STDERR "$1: Not in git => version test";
            $version = "test";
        }
        elsif ($line =~ /^ M (.*)/) {
            say STDERR "$1: Locally modified => version test";
            $version = "test";
        }
        elsif ($line =~ /^D  (.*)/) {
            say STDERR "$1: Deleted (or renamed) but not committed => version test";
            $version = "test";
        }
        elsif ($line =~ /^ D (.*)/) {
            say STDERR "$1: Locally deleted => version test";
            $version = "test";
        }
        elsif ($line =~ /^A  (.*)/) {
            say STDERR "$1: Locally added => version test";
            $version = "test";
        }
        elsif ($line =~ /^AM (.*)/) {
            say STDERR "$1: Locally added and modified => version test";
            $version = "test";
        }
        elsif ($line =~ /^([ MADRCU][ MADRCU]) (.*)/) {
            say STDERR "$2: $1 (Whatever that means) => version test";
            $version = "test";
        }
        elsif ($line =~ /^fatal: No names found/) {
            say STDERR "No tag on this version => version test";
            $version = "test";
        }
        elsif ($line =~ /^On branch/) {
        }
        elsif ($line =~ /([0-9a-fA-F]+)[ \t]+refs\/tags\//) {
            $remotetagcommit = $1;
            if ($debug) {
                say STDERR "Remote commit $remotetagcommit";
            }
        }
        elsif ($line =~ /^([0-9]+)\.([0-9]+)(\.([0-9]+))?$/) {
            $tag = $line;
            my $major = $1;
            my $minor = $2;
            my $patch = $4 || "0";
            $version = "$major.$minor.$patch";
            say STDERR "Checking tag $line => version $version";
        }
        elsif ($line =~ /^([a-zA-Z][a-zA-Z0-9]*_)+([0-9]+)_([0-9]+)(_([0-9]+))?$/) {
            $tag = $line;
            my $major = $2;
            my $minor = $3;
            my $patch = $5 || "0";
            $version = "$major.$minor.$patch";
            say STDERR "Checking tag $line => version $version";
        }
        elsif ($line =~ /^(.*[0-9]+[_.][0-9]+([_.][0-9]+)?)-([0-9]+)-g[0-9a-fA-F]+$/) {
            $version = "test";
            my $s = $3 != 1 ? "s" : "";
            say STDERR "Tag $1 is $3 commit$s old => version test";
        }
        elsif ($line =~ /^Your branch is ahead of '(.*)\/(.*)'/) {
            say STDERR "Branch \"$2\" not yet pushed to remote \"$1\" => version test";
            say STDERR "Try: git push --tags $1 $2";
            $version = "test";
        }
        elsif ($line =~ /^Your branch and '(.*)\/(.*)' have diverged/) {
            say STDERR "Branch \"$2\" diverged from remote \"$1\" => version test";
            say STDERR "Try to merge or rebase your changes.";
            $version = "test";
        }
        elsif ($line =~ /^Your branch is up to date with '(.*)\/(.*)'/) {
            $remote = $1;
            $branch = $2;
        }
        elsif ($line =~ /^\* [^ ]+ +([0-9a-fA-F]+) \[(.*)\/([^:]*).*\]/) {
            $commit = $1;
            $remote = $2;
            $branch = $3;
            if ($debug) {
                say STDERR "Commit $commit on branch $branch on remote $remote";
            }
        }
        elsif ($debug) {
            say STDERR "unparsed: $line";
        }
    }
}

if ($debug) {
    say STDERR "checking $files";
}

eval {
    # fails if git command exits with error
    @statusinfo = check_output("git status --porcelain $files");
    parse_git_output(\@statusinfo);
    if ($version) {
        say $version;
        exit;
    }

    @statusinfo = check_output("git describe --tags");
    parse_git_output(\@statusinfo);
    if (!defined($version)) {
        say STDERR "Could not find out version tag => version test";
        $version = "test";
    }

    if ($version ne "test") {
        @statusinfo = check_output("git branch -vv");
        parse_git_output(\@statusinfo);
    }

    if ($version ne "test") {
        @statusinfo = check_output("git status");
        parse_git_output(\@statusinfo);
    }

    if ($debug) {
        if (defined($remote)) {
            say STDERR "remote = $remote";
        } else {
            say STDERR "remote undefined";
        }
        if (defined($tag)) {
            say STDERR "tag = $tag";
        } else {
            say STDERR "tag undefined";
        }
        if (defined($commit)) {
            say STDERR "commit = $commit";
        } else {
            say STDERR "commit undefined";
        }
        if (defined($branch)) {
            say STDERR "branch = $branch";
        } else {
            say STDERR "branch undefined";
        }
    }
    if (defined($remote) && defined($tag) && defined($commit)) {
        my $err = eval {
            @statusinfo = check_output("git ls-remote --tags '$remote' '$tag' '$tag^*'");
            parse_git_output(\@statusinfo);
            if (!$remotetagcommit) {
                say STDERR "Tag $tag not yet pushed to remote \"$remote\" => version test";
                say STDERR "Try: git push --tags $remote $branch";
                $version = "test";
            } else {
                $remotetagcommit = substr($remotetagcommit,0,length($commit));
                if ($remotetagcommit ne $commit) {
                    say STDERR "Tag $tag differs from the same tag on remote \"$remote\" => version test";
                    say STDERR "It is commit $commit locally but commit $remotetagcommit on $remote.";
                    say STDERR "Try a new tag after fixing your $tag tag: git tag --force $tag $remotetagcommit";
                    say STDERR "Or overwrite \"$remote\": git push --force --tags $remote $branch";
                    $version = "test";
                }
            }
        };
        if ($@) {
            $@ =~ s/\n/\n| /g;
            say STDERR "Cannot check tag $tag on remote \"$remote\":";
            say STDERR "| $@";
            say STDERR "I hope you know what you're doing.";
        }
    }

    say $version;
    exit;
};

say STDERR "No repository found => version test";
say "test";
