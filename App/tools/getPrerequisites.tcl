#!/usr/bin/tclsh

if {[lindex $argv 0] == "-dep"} {
    set depstyle 1
    set argv [lrange $argv 1 end]
} else {
    set depstyle 0
}
set installdir [lindex $argv 0]
set prerequisites {}
foreach filename [glob -nocomplain *.d] {
    set file [open $filename]
    set contents [read $file]
    close $file
    foreach word $contents {
        set header [string trim $word]
        if [string match $installdir/* $header] {
            set file [open $header]
            while {[regexp {^#define __(.*)Lib__ ([0-9]+\.[0-9]+)$} \
                [gets $file] match lib version]} {
                if $depstyle {
                    lappend prerequisites "$lib $version"
                } else {
                    lappend prerequisites ${lib}Lib_$version
                }
            }
            close $file
        }
    }
}
set prerequisites [lsort -unique $prerequisites]
puts [join $prerequisites "\n"]
