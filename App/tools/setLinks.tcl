#!/usr/bin/tclsh

package require Tclx

set dir [lindex $argv 0]
set ext [lindex $argv 1]
set names [lrange $argv 2 end]
if {![file isdirectory $dir]} exit

if {[lindex [file split $dir] 1] == "prod"} {
    proc mklink {target link} {
        global dir
        puts "Linking $link -> $target"
        puts "repository -H pc770 slink $target $dir/$link"
    }
    proc rmlink {link} {
    }
} else {
    proc mklink {target link} {
        puts "Linking $link -> $target"
        link -sym $target $link
    }
    proc rmlink {link} {
        #puts "removing link $link"
        file delete $link
    }
}

cd $dir
foreach name $names {
    set links [glob -nocomplain -type l $name-*$ext]
    lappend links $name$ext
    foreach file $links {
        rmlink $file
    }
    set files [glob -nocomplain -types f $name-*$ext]
    set files [lsort -decreasing -dictionary $files]
    set oldmajor ""
    set oldminor ""
    set first 1
    foreach file $files {
        if {[regexp {(.*)-([0-9]+)\.([0-9]+)\.([0-9]+)(.*)} $file \
            match head major minor patch tail] && \
            $head == $name && $tail == $ext} {
            if {$first} {
                mklink $file $name$ext
                set first 0
            }
            if {$major != $oldmajor} {
                mklink $file $name-$major$ext
                set oldmajor $major
                set oldminor ""
            }
            if {$minor != $oldminor} {
                mklink $file $name-$major.$minor$ext
                set oldminor $minor
            }
        }
    }
}
