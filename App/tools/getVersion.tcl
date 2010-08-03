#!/usr/bin/tclsh

package require Tclx

set global_context [scancontext create]
set file_context [scancontext create]
set skip_context [scancontext create]

scanmatch $global_context {no such directory `(.*)'} {
    puts stderr "checking directory $matchInfo(submatch0): so such directory  => version test"
    return
}

scanmatch $global_context {there is no version here} {
    puts stderr "checking current directory: not in cvs => version test"
    return
}

scanmatch $global_context {^File: .*Up-to-date} {
    set file [lindex $matchInfo(line) 1]
    puts -nonewline stderr "checking $file: "
    scanfile $file_context $matchInfo(handle)
    if {![info exists major($file)]} {
        puts stderr "revision $rev($file) not tagged => version test"
        set version test
        continue
    }
    puts stderr "revision $rev($file) tag $tag($file) => version $major($file).$minor($file).$patch($file)"
    if {![info exists version]} {
        set version $major($file).$minor($file).$patch($file)
    } else {
        if ![cequal $major($file).$minor($file).$patch($file) $version] {
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
    set major($file) $matchInfo(submatch0)
    set minor($file) $matchInfo(submatch1)
    set patch($file) $matchInfo(submatch2)
    set tag($file) "[lindex $matchInfo(line) 2] (sticky)"
    scanfile $skip_context $matchInfo(handle)
    return
}

scanmatch $file_context {Sticky Tag:.*_([0-9]+)_([0-9]+)[ \t]+\(revision: } {
    set major($file) $matchInfo(submatch0)
    set minor($file) $matchInfo(submatch1)
    set patch($file) 0
    set tag($file) "[lindex $matchInfo(line) 2] (sticky)"
    scanfile $skip_context $matchInfo(handle)
    return
}

scanmatch $file_context {_([0-9]+)_([0-9]+)(_([0-9]+))?[ \t]+\(revision: ([\.0-9]+)\)} {
    if [cequal $rev($file) $matchInfo(submatch4)] {
        set Major $matchInfo(submatch0)
        set Minor $matchInfo(submatch1)
        set Patch [expr $matchInfo(submatch3) + 0]
        if {![info exists major($file)] ||
            $Major>$major($file) ||
            ($Major==$major($file) && ($Minor>$minor($file)
                || ($Minor==$minor($file) && $Patch>$patch($file))))} {
            set major($file) $Major
            set minor($file) $Minor
            set patch($file) $Patch
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

# cvs bug: calling cvs status for files in other directories spoils status
# information for local files.
# fix: check local and non local files separately

set cvsstatus [open "|cvs status -l -v 2>@ stdout"]
scanfile $global_context $cvsstatus
if [catch {close $cvsstatus}] {set version test}

set files {}
foreach file $argv {
    if {[file tail $file] != $file} {
        lappend files $file
    }
}
if [llength $files] {
    set cvsstatus [open "|cvs status -l -v $files 2>@ stdout"]
    scanfile $global_context $cvsstatus
    if [catch {close $cvsstatus}] {set version test}
}
if {![info exists version]} {set version test}
puts $version
