#!/usr/bin/tclsh

package require Tclx

set global_context [scancontext create]

set epicsversion 3.14
set quiet 0
set recordtypes 0
set seachpath {}
set filesDone {}

while {[llength $argv]} {
    switch -glob -- [lindex $argv 0] {
        "-[0-9]*" { set epicsversion [string range [lindex $argv 0] 1 end]}
        "-q"      { set quiet 1 }
        "-r"      { set recordtypes 1; set quiet 1 }
        "-I"      { lappend seachpath [lindex $argv 1]; set argv [lreplace $argv 0 1]; continue }
        "-I*"     { lappend seachpath [string range [lindex $argv 0] 2 end] }
        "--"      { set argv [lreplace $argv 0 0]; break }
        "-*"      { puts stderr "Warning: Unknown option [lindex $argv 0] ignored" }
        default   { break }
    }
    set argv [lreplace $argv 0 0]
}

proc opendbd {name} {
    global seachpath
    foreach dir $seachpath {
        if ![catch {
            set file [open [file join $dir $name]]
        }] {
            return $file
        }
    }
    return -code error "file $name not found"
}

scanmatch $global_context {^[ \t]*(#|%|$)} {
    continue
} 

if {$recordtypes} {
    scanmatch $global_context {include[ \t]+"?((.*)Record.dbd)"?} {
    if ![catch {
        close [opendbd $matchInfo(submatch0)]
    }] {
        puts $matchInfo(submatch1)
    }
    continue
}

} else {

    scanmatch $global_context {(registrar|variable|function)[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)} {
        global epicsversion
        if {$epicsversion > 3.13} {puts $matchInfo(submatch0)($matchInfo(submatch1))}
    }
    scanmatch $global_context {variable[ \t]*\([ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*,[ \t]*"?([a-zA-Z0-9_]+)"?[ \t]*\)} {
        global epicsversion
        if {$epicsversion > 3.13} {puts variable($matchInfo(submatch0),$matchInfo(submatch1))}
    }

    scanmatch $global_context {
        puts $matchInfo(line)
    }
}

scanmatch $global_context {include[ \t]+"?([^"]*)"?} {
    global seachpath
    global FileName
    global quiet
    if [catch {
        includeFile $global_context $matchInfo(submatch0)
    } msg] {
        if {!$quiet} {
            puts stderr "ERROR: $msg in path \"$seachpath\" called from $FileName($matchInfo(handle)) line $matchInfo(linenum)"
            exit 1
        }
    }
    continue
}

proc includeFile {context filename} {
    global global_context FileName filesDone matchInfo quiet
    set basename [file tail $filename]
    if {[lsearch $filesDone $basename ] != -1} {
        if {!$quiet} {
            puts stderr "Info: skipping duplicate file $basename included from $FileName($matchInfo(handle))"
        }
        return
    }
    if {$filename != "dbCommon.dbd"} { lappend filesDone [file tail $filename] }
    set file [opendbd $filename]
    set FileName($file) $filename
    #puts "#include $filename from $FileName($matchInfo(handle))"
    scanfile $context $file
    close $file
}   

foreach filename $argv {
    global filesDone quiet
    set basename [file tail $filename]
    if {[lsearch $filesDone $basename] != -1} {
        if {!$quiet} {
            puts stderr "Info: skipping duplicate file $basename from command line"
        }
        continue
    }
    if {$basename != "dbCommon.dbd"} { lappend filesDone $basename }
    set file [open $filename]
    set FileName($file) $filename
    scanfile $global_context $file
    close $file
}
