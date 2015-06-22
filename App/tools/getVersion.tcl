#!/usr/bin/tclsh

package require Tclx

set debug 0

set global_context [scancontext create]
set file_context [scancontext create]
set skip_context [scancontext create]

scanmatch $global_context {there is no version here} {
    return
}

scanmatch $global_context {cvs status: failed} {
    puts stderr "Error: $matchInfo(line)"
    return
}

scanmatch $global_context {no such directory `(.*)'} {
    puts stderr "checking directory $matchInfo(submatch0): so such directory"
    return
}

scanmatch $global_context {cvs [status aborted]: there is no version here} {
    return
}

scanmatch $global_context {^File: .*Up-to-date} {
    set file [lindex $matchInfo(line) 1]
    puts -nonewline stderr "checking $file: "
    catch {unset major minor patch}
    scanfile $file_context $matchInfo(handle)
    if {![info exists major]} {
        puts stderr "revision $rev($file) not tagged => version test"
        set version test
        continue
    }
    puts stderr "revision $rev($file) tag $tag($file) => version $major.$minor.$patch"
    if {![info exists version]} {
        set version $major.$minor.$patch
    } else {
        if ![cequal $major.$minor.$patch $version] {
            set version test
            continue
        }
    }
    continue
}

scanmatch $global_context {^File: .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "checking $file: [lrange $matchInfo(line) 3 end] => version test"
    set version test
    continue
}

scanmatch $global_context {^\? .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "checking $file: not in cvs => version test"
    set version test
    continue
}     

scanmatch $file_context {Working revision:} {
    set rev($file) [lindex $matchInfo(line) 2]
}

scanmatch $file_context {Sticky Tag:.*_([0-9]+)_([0-9]+)_([0-9]+)[ \t]+\(revision: } {
    set major $matchInfo(submatch0)
    set minor $matchInfo(submatch1)
    set patch $matchInfo(submatch2)
    set tag($file) "[lindex $matchInfo(line) 2] (sticky)"
    scanfile $skip_context $matchInfo(handle)
    return
}

scanmatch $file_context {Sticky Tag:.*_([0-9]+)_([0-9]+)[ \t]+\(revision: } {
    set major $matchInfo(submatch0)
    set minor $matchInfo(submatch1)
    set patch 0
    set tag($file) "[lindex $matchInfo(line) 2] (sticky)"
    scanfile $skip_context $matchInfo(handle)
    return
}

scanmatch $file_context {_([0-9]+)_([0-9]+)(_([0-9]+))?[ \t]+\(revision: ([\.0-9]+)\)} {
    if [cequal $rev($file) $matchInfo(submatch4)] {
        set Major $matchInfo(submatch0)
        set Minor $matchInfo(submatch1)
        set Patch [expr $matchInfo(submatch3) + 0]
        if {![info exists major] ||
            $Major>$major ||
            ($Major==$major && ($Minor>$minor
                || ($Minor==$minor && $Patch>$patch)))} {
            set major $Major
            set minor $Minor
            set patch $Patch
            set tag($file) [lindex $matchInfo(line) 0]
        }
    }
}

scanmatch $skip_context {=================} {
    return
}

scanmatch $file_context {=================} {
    return
}

set git_context [scancontext create]

scanmatch $git_context {fatal: Not a git repository} {
    return
}

scanmatch $git_context {^\?\? .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "$file: not in git => version test"
    set version test
    continue
}     

scanmatch $git_context {^ M .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "$file: locally modified => version test"
    set version test
    continue
}     

scanmatch $git_context {^D  .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "$file: deleted (or renamed) but not committed => version test"
    set version test
    continue
}     

scanmatch $git_context {^ D .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "$file: locally deleted => version test"
    set version test
    continue
}     

scanmatch $git_context {^([ MADRCU][ MADRCU]) .*} {
    set file [lindex $matchInfo(line) 1]
    puts stderr "$file: $matchInfo(submatch0) (whatever that means) => version test"
    set version test
    continue
}     

scanmatch $git_context {fatal: No names found} {
    puts stderr "no tag on this version => version test"
    set version test
}

scanmatch $git_context {([0-9]+)[_.]([0-9]+)([_.]([0-9]+))?$} {
    set major $matchInfo(submatch0)
    set minor $matchInfo(submatch1)
    set patch [expr $matchInfo(submatch3) + 0]
    set version $major.$minor.$patch
    puts stderr "checking tag $matchInfo(line) => version $version"
}

scanmatch $git_context {(.*[0-9]+[_.][0-9]+([_.][0-9]+)?)-([0-9]+)-g} {
    set version test
    puts stderr "tag $matchInfo(submatch0) is $matchInfo(submatch2) commits old => version test"
}

if {[lindex $argv 0] == "-d"} {
    set debug 1
    set argv [lrange $argv 1 end]
}

# Check all files in top directory and all files specified explicitly in subdirectories

set topfiles [glob -nocomplain GNUmakefile makefile Makefile *.c *.cc *.cpp *.h *.dbd *.st *.stt *.gt]

if {$debug} {
    puts stderr "checking $topfiles $argv"
}


if {[catch {
    # fails if we have no git:
    set statusinfo [open "|git status --porcelain $topfiles $argv 2>@ stdout"]
    scanfile $git_context $statusinfo
    # fails if this is no git repo
    close $statusinfo
    
    if [info exists version] {
        puts $version
        exit
    }   
    
    set statusinfo [open "|git describe --tags HEAD 2>@ stdout"]
    scanfile $git_context $statusinfo
    catch {close $statusinfo}

    if ![info exists version] {
        puts stderr "Could not find out version tag => version test"
        set version test
    }
    puts $version
    exit
}] && $debug} { puts stderr "git: $errorInfo" }


if {[catch {
# cvs bug: calling cvs status for files in other directories spoils status
# information for local files.
# fix: check local and non local files separately

    # fails if we have no cvs or server has a problem
    set statusinfo [open "|cvs status -l -v $topfiles $argv 2>@ stdout"]
    scanfile $global_context $statusinfo
    # fails if this is no cvs repo
    close $statusinfo

#    set files {}
#    foreach file $argv {
#        if {[file tail $file] != $file} {
#            lappend files $file
#        }
#    }
#    if [llength $files] {
#        set statusinfo [open "|cvs status -l -v $files 2>@ stdout"]
#        scanfile $global_context $statusinfo
#        close $statusinfo
#    }

    puts $version
    exit
}] && $debug} { puts stderr "cvs: $errorInfo" }

puts stderr "No repository found => version test"
puts "test"

# $Header: /cvs/G/DRV/misc/App/tools/getVersion.tcl,v 1.4 2015/06/22 07:42:01 zimoch Exp $
