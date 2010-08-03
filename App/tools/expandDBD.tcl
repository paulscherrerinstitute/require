#!/usr/bin/tclsh

package require Tclx

set epicsversion 3.13
set global_context [scancontext create]

proc opendbd {name} {
    global seachpatch
    foreach dir $seachpatch {
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

scanmatch $global_context {include[ \t]+"(.*)"} {
    global FileName
    if [catch {
        includeFile $global_context $matchInfo(submatch0)
    } msg] {
        puts stderr "ERROR: $msg in $FileName($matchInfo(handle)) line $matchInfo(linenum)"
        exit 1
    }
    continue
}

scanmatch $global_context {include[ \t]+(.*)} {
    if [catch {
        includeFile $global_context $matchInfo(submatch0)
    } msg] {
        puts stderr "ERROR: $msg in $FileName($matchInfo(handle)) line $matchInfo(linenum)"
        exit 1
    }
    continue
}

scanmatch $global_context {(registrar|variable|function)[ \t]*\(} {
    global epicsversion
    if {$epicsversion == 3.14} {puts $matchInfo(line)}
}

scanmatch $global_context {
    puts $matchInfo(line)
}

proc includeFile {context name} {
    global global_context FileName
    set file [opendbd $name]
    set FileName($file) $name
    scanfile $context $file
    close $file
}   

if {[lindex $argv 0] == "-3.14"} {
    set epicsversion 3.14
    set argv [lreplace $argv 0 0]
}

set seachpatch {}
while {[lindex $argv 0] == "-I"} {
    lappend seachpatch [lindex $argv 1]
    set argv [lreplace $argv 0 1]
}

foreach filename $argv {
    set file [open $filename]
    set FileName($file) $filename
    scanfile $global_context $file
    close $file
}

# $Header: /cvs/G/DRV/misc/App/tools/expandDBD.tcl,v 1.2 2010/08/03 08:42:40 zimoch Exp $
