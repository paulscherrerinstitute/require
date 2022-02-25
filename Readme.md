# driver.makefile and `require`

With driver.makefile, it is possible to build **IOC software modules** such
as drivers, state notation code, records, etc. with a minimum of
configuration. Many things are detected automatically but can be changed if
necessary. The installed module can be loaded on the IOC with the `require`
command. 

The modules are built for each combination of EPICS base version (e.g.
3.13.10, 3.14.12, 7.0.4.1,...) and IOC architecture (e.g. T2-ppc604,
SL6-x86_64, RHEL7-x86_64,...). This allows to run the same software on IOCs
with different EPICS versions or operating systems.

_PSI: Some of the settings and methods described here are specific to the
setup at [PSI](https://www.psi.ch). If you use this software somewhere else,
details may differ. PSI specific information is emphasized like this._

_PSI: Best log in on one of our software development computers like
**sls-lc**, **hipa-lc**,... to make sure all needed EPICS software is
installed and write access to the installations locations is granted._

## Module Pool

A module built with driver.makefile can be installed into a module pool
common to all IOCs (of one facility) where `require` can find it. The module
pool can contain several versions of the same module and `require` can be
used to load a specific version or simply the highest one.

The default location of the pool is `/ioc/modules/` but can be changed with
the environment variable `EPICS_MODULES`. It contains one subdirectory for
each module which contains subdirectories for each version of the module.

All files of a module that are needed at run-time are installed into that
subdirectory. Thus everything needed to use the module is found at the same
place. This includes dynamically loadable libraries for all architectures
and EPICS versions, a DBD file for each EPICS version, module dependency
information, optionally C/C++ header files, DB templates, startup script
snippets, and arbitrary other files.

_PSI: A module does not need to be installed into the module pool. It is
also possible to install a (private) module with `ioc install` into the IOC
start directory. In that case, only this IOC can use it. Nevertheless
driver.makefile can be used to compile such a module and `require` can be
used to load it. Only startup scripts and templates are not supported by
`require` in this case (and not really needed)._

## Using `require`

In order to use a module, its library needs to be loaded, its .dbd file
needs to be loaded and (if using EPICS 3.14 or higher) its initialization
function needs to be called.

Some modules may depend on other modules (for example many modules depend
on asyn). In that case it is necessay that the other module is loaded first
and that conflicts are resolved in case multiple different versions of the
same module are requested.

To make loading modules easier, the `require` command can be used in IOC
startup script.
```
require "<module>" [,"<version>"] [,"<macro1>=<value2>, <macro2>=<value2>"]
```

This performs all necessary loading and initialization, avoids loading a
library twice and keeps an eye on version conflicts. If a required module
depends on an other module, `require` recursively invokes itself to load
the other module fist.

The two arguments, the version and the macro list are both optional.
The order does not matter, thus macros can be provided without a requesting
a specific version.

### Version strings

Version strings can consist of one, two or three numbers, separated by a dot.
If a version string with less than than three numbers is used, `require`
loads the highest available version that matches the given first numbers.
A `+` after a number means that the version may be higher but not lower.
If the version string is omitted, `require` loads the highest available
version of the module (which may be different for different architectures
or EPICS base versions).

Version strings that do not consist of numbers are considered test versions
and are only loaded when requested explicitly. I strongly suggest not to use
test versions in production.

If the version string is `"ifexsists"`, then the highest existing version
is loaded as if no version was given. But failure to find the module (for
the current architecture and EPICS base version) is not an error. This can
be useful for generic modules that exist only on certain architectures.

If the version string is `"none"`, then nothing is done. The module is
skipped. This can be useful if the version string is a macro which may be
set to different versions (including `"none"`) depending on external
conditions.

**Examples:**
```
# load highest version
require "module"
# load test version
require "module", "username"
# load highest version but ignore if the module does not exist
require "module", "ifexists"
# load the highest 1.2.x version
require "module", "1.2"
# load the highest 1.x version and make sure that x>=2 (used by dependency files)
require "module", "1.2+"
# load exactly version 1.2.3 and pass macros to a startup script
require "module", "1.2.3", "macro1=value1, macro2=value2"
# load highest version and pass macros to a startup script
require "module", "macro1=value1, macro2=value2"
# skip the module
require "module", "none", "macro1=value1, macro2=value2"
```

#### Version compatibility

If an already loaded module is required a second time, the requested version
is checked for compatibility with the already loaded one. If the versions
are incompatible the startup script is aborted with an error message.

An already loaded version is considered compatible if:
   * no specific version was required
   * the already loaded version matches the required version exactly
   * major and minor numbers are the same and already loaded patch number
     is equal to the required one or higher
   * major numbers are the same and already loaded minor number is higher
     than the required one
   * the already loaded version is a test version and the required version
     is not a test version

Versions with different major numbers are never compatible and different
test versions are never compatible. Modules can be built in a way that
enforece stricter compatibility rules, that is either the major and minor
version numbers must match exactly or even the patch level must match
exactly as well. See
[below](#header-files-and-dependencies-headers-_version-and-required-variables)
for details.

_PSI: In our environment with the possibility to install multiple projects
on one IOC, the same module may be required by more than one project with
the potential of conflicting version requests._

The `require` command uses the environment variable `EPICS_DRIVER_PATH` to
search for modules. Usually, the variable is set up by the `iocsh` wrapper
script such that `require` searches first locally in the current directory
(usually the IOC start directory), then in the module pool `/ioc/modules/`
(or `$EPICS_MODULES` if set).

### Version Records

For every loaded module, `require` creates one _stringin_ record with the
name `$(IOC):$(MODULE)_VERS` which contains the version of the module.
(Only if `require` is called before `iocInit` because it is not allowed to
create records after `iocInit`.)

Furthermore, the following global records are created:
   * The STRING _waveform_ record `$(IOC):MODULES` contains a list of all
     loaded modules.
   * The STRING _waveform_ record `$(IOC):VERSIONS` contains the module
     versions (indices match the former record).
   * The CHAR _waveform_ record `$(IOC):MOD_VER` contains the list of
     module and version pairs as a long string with newline separated lines.
     The list is formatted so that the string prints nicely as a table with
     fixed width fonts.
     
The `IOC` environment variable is set by the `iocsh` wrapper script.

### Environment Variables

Several environment variables are set up by `require` that can be used in
the IOC startup script or in the startup script snippets of the modules.
   * `$(EPICS_RELEASE)` is the EPICS base release in use, e.g. "3.14.12".
   * `$(EPICS_BASETYPE)` contains the first two components of the above,
     e.g. "3.13" or "3.14"
   * `$(EPICS_HOST_ARCH)` is the target architecture, e.g. "T2-ppc604".
   * `$(T_A)` the same, just a shorter name.
   * `$(OS_CLASS)` e.g. "Linux", "vxWorks", "WIN32"
   * `$(IOC_DIR)` contains the absolute path of the directory in which the
     IOC started.
   * `$(MODULE)` is set to the name of the current or most recently required
     module (even if it had been loaded already earlier).
   * `$($(MODULE)_VERSION)` is the version string of the module.
   * `$($(MODULE)_DIR)` is the absolute path of the module directory. Here
     are the startup scripts located.
   * `$(MODULE_DIR)` the same, but overwritten each time `require` is called.
   * `$(SCRIPT_PATH)` contains the directories of all loaded modules with
     scripts in reverse order (after "."). This allows to use `runScript`
     without explicit path (see [below](#startup-script-snippets)).
   * `$($(MODULE)_TEMPLATES)` is the absolute path for the db templates of a
     module (if the module has templates).
   * `$($(MODULE)_DB)` the same, just a shorter name.
   * `$(TEMPLATES)` the same, but overwritten each time `require` is called.
     This is reset to the original value at IOC start if the module has no
     templates.
   * `$(EPICS_DB_INCLUDE_PATH)` contains all the template directories of all
     loaded modules in reverse order (after ".").

In particular the `EPICS_DB_INCLUDE_PATH` variable allows to load template
files from a module with `dbLoadTemplate`, `dbLoadDatabase` or
`dbLoadRecords` without the need to have them locally in the IOC start
directory. Only the substitution file needs to be stored locally. The
directory `.` is always first in that path so that local templates can
overwrite module templates.

### Startup Script Snippets

If the module has a default startup script snippet, it is executed before
`require` returns and macros in the startup script snippet are replaced
with the substitutions passed to `require` or with environment variables
if no matching substitution was given (including the variables set by
`require`, see [above](#environment-variables)). 
Some modules use this method to load default templates or to initialize
hardware supported by the module in a standard way for or any other module
initialization.

Script snippets are searched by `require` in the following order:
   1. `$(EPICS_HOST_ARCH)-$(EPICS_RELEASE).cmd`
      (e.g. `RHEL7-x86_64-3.14.12.cmd`)
   2. `$(EPICS_HOST_ARCH)-$(EPICS_BASETYPE).cmd`
      (e.g. `RHEL7-x86_64-3.14.cmd`)
   3. `$(OS_CLASS)-$(EPICS_RELEASE).cmd` (e.g. `Linux-3.14.12.cmd`)
   4. `$(OS_CLASS)-$(EPICS_BASETYPE).cmd` (e.g. `Linux-3.14.cmd`)
   5. `startup-$(EPICS_RELEASE).cmd` (e.g. `startup-3.14.12.cmd`)
   6. `startup-$(EPICS_BASETYPE).cmd` (e.g. `startup-3.14.cmd`)
   7. `$(EPICS_HOST_ARCH).cmd` (e.g. `RHEL7-x86_64.cmd`)
   8. `$(OS_CLASS).cmd` (e.g. `Linux.cmd`)
   9. `startup.cmd`

Only the first one found is executed automatically.

:exclamation: Startup script snippets are only executed if `require` is
called before `iocInit` because it is illegal to do certain actions after
`iocInit`.
In particular no record must be created and not driver must be initialized
after `iocInit`.

If the module was already loaded, the startup script snippets is called
again if and only if substitutions are passed.

Other than the default startup scripts can be called with `runScript` which
uses the same macro substitution:
```
runScript "otherscript.cmd", "MACRO=VALUE, ..."
runScript "$(module_DIR)/somescript.cmd", "MACRO=VALUE, ..."
```

Keep in mind that the `runScript` searches `$(SCRIPT_PATH)` for scripts
without a path, which contains all loaded modules in reverse order
(after "."). That means that local scripts are always found first, followed
by the most recently loaded modules. Thus using a path is often notnecessay.
But to be safe, the variable `$(module_DIR)` can be used explicitly (with
the name of the module).

#### Local Script Variables

Scripts executed by `require` or `runScript` can use local variables and
integer arithmetic.

Local variables hold strings like macros or environment variables, but
arithmetic expressions are evaluated when the variable is assigned.
These variables can be used with the standard syntax `$(variable)` or
`${variable}` in the same script where they are defined.

To set a local variable, use the `=` character.
```
variable=value
```

The variable must consist of alphanumeric characters or underscores. 
No spaces are allowed before the `=`
and spaces after the `=` are part of the value.
The value consists of strings and integer expressions.

:bulb: To assign instead to a global variable in vxWorks use a space
before the `=`.

  * Everything in quotes is a string and copied without evaluation (but the
    quotes are removed).
  * Every sequence of unquoted words and everything inside an unquoted pair
    of parentheses that can be evaluated as an integer expression is
    substituted by the result.
  * Everything else is a string and copied without modification. That
    includes strings inside words that may look like expressions.

Integer expressions consist of integer literals in either decimal, octal or
hexadecimal notation (with the usual prefixes `0` for octal and `0x` for
hexadecimal), parentheses `()` and the operators `+`, `-`, `*`, `/`, `%`
(modulo), `**` (exponential), `<<`, `>>`, `>>>` (unsigned shift),
`<`, `<=`, `>`, `>=`, `==`, `!=` (not equal), `<=>` (comparision, results in
-1, 0 or 1), `&`, `|`, `^` (bitwise and, or, xor), `&&`, `||` (logical and,
or), `?` (not equal to 0), `?` `:` (if then else), `?:` (if else).

:exclamation: All `$(...)` or `${...}` have already been substituted with
their (string) values before arithmetic evaluation starts. That can be
confusing with operator priorities. For example `x=5*$(MACRO)` with
`MACRO="1+2"` results in 7, not 15. Use `x=5*($(MACRO))` or `MACRO="(1+2)"`
to get 15 in this case.

Division by 0 or modulo 0 gives undefined results (in fact 0, but do not
rely on it).

A value (literal integer or `()` expression) followed by `?` followed by a
first expression, `:` and a second expression computes to the first
expression if the value is not 0, else to the second expression.

A value followed by `?:` followed by an expression computes to the
value if that one is not 0, else to the expression.

A value followed by `?` computes to 0 if the value is 0 and to 1 otherwise.

An expression can be prefixed with a _printf()_ style integer format to change
formatting of the result. Valid formats are: `%`, optionally followed by any
of `+-#0` or space, optionally followed by a positive integer number,
followed by one of `diouxXc`. See
[`man printf`](https://man7.org/linux/man-pages/man3/printf.3.html)
for details. The default format is `%d`.

:exclamation: Do not quote the format string or it will simply be a string
copied literally.

:bulb: To see the result of an arithmetic expression you can add a comment
line containing the variable reference: `#$(variable)`

**Examples**: (with the result made visible as `#$(x)`).
```
x=Buy 4 + 2*3 eggs
#Buy 10 eggs

x="Buy 4 + 2*3 eggs"
#Buy 4 + 2*3 eggs

x=Buy 4 + 2*3eggs
#Buy 4 + 2*3eggs

x=Buy(4 + 2*3)eggs
#Buy10eggs

x="Buy(4 + 2*3)eggs"
#Buy(4 + 2*3)eggs

x=010
#8

x=%0o4+4,%0x4*4,%04d4**4
#10,10,0256

x=7?,7?2:3,7?:5
#1,2,7

x=0?,0?2:3,0?:5
#0,3,5
```

:exclamation: This type of arithmetic only works in local variable
assignments and thus only in scripts executed by `runScript`.



## Using driver.makefile

To use driver.makefile create a `GNUmakefile`, typically in the top level
module source directory which includes `/ioc/tools/driver.makefile`.
Often, that is the only thing to do. No knowledge about how `make` works is
needed. The idea is to make the `GNUmakefile` for a simple module as simple
as possible.

**Example GNUmakefile:**
```
include /ioc/tools/driver.makefile
```

This detects most things automatically, as long as all files are in the top
level directory of the module and use standard file extensions. But it is
possible to change the default behavior by defining variables in the
`GNUmakefile`. If doing so, keep the include line at the top and add
variable definitions below. Variables contain one word or lists of words
(e.g. file names). More words can be appended to variables. Appending to an
empty or not existing variable is like setting it.

**Examples:**
```
include /ioc/tools/driver.makefile
# build for both, vxWorks and Linux
BUILDCLASSES += Linux
# only use the following source files
SOURCES = file1.c file2.c
SOURCES += file3.c
```

Then the module can be built with `make` or `make build` in the directory
with the `GNUmakefile`. The built module can be installed with
`make install`. This automatically calls `build` if not yet done or if files
have changed since the last built. An installation can be deleted with
`make uninstall`. The created files from the build process can be cleaned up
with `make clean`. To find out which module version will be built (and why)
use `make version`. If you are unsure about the options or variables try
`make help`. Several commands can be chained like
`make uninstall clean build install`.

### GNUmakefile or Makefile?

GNU `make`, which EPICS uses, reads commands from `GNUmakefile`, `makefile`,
or `Makefile`, in that precedence (unless told otherwise with `-f`). All
three names work with driver.makefile but `GNUmakefile` is preferred, because
the name `Makefile` is already used by many third party modules for the
standard EPICS build method. Thus when having a module that should be
compatible with the standard EPICS build method and with driver.makefile,
it makes sense to keep `Makefile` for the standard method.

`GNUmakefile` takes precedence over `Makefile` if both exist
(see also: [`man make`](https://man7.org/linux/man-pages/man1/make.1.html)).
Thus to compile with driver.makefile, simply write `make` but to compile the
standard EPICS way write `make -f Makefile`. The name `makefile`, (which
would also take precedence over  `Makefile`) should not be used as an
alternative to `Makefile` because Windows ignores the different
capitalization.

When giving the module sources to other institutes which do not use
driver.makefile, simply remove the `GNUmakefile` but keep (or create) the
`Makefile` for the standard EPICS build method..

### Versions and Tags

The version is generated from a tag in CVS or GIT. If all used files are
checked in (committed) and tagged and the tag ends in two or three numbers 
separated by `_` or consists of two or three numbers separated by `.` then
the version number is generated from these numbers. A missing third number
is replaced with 0.

**Examples:** `mydriver_7_2`, `1.2.3`

The first number is the major version number. It must be incremented if any
change in the module is not backward compatible. Such changes include:
   * Removing or renaming features, for example interface functions or IOC
     shell functions, templates, macros, interface headers. (Renaming or
     changing only internally used functions or header files do not change
     the interface and thus do not require a new major version number.
     Thus do not install internal header files unnecessarily.)
   * Changing the behavior of such features, for example the meaning or
     order of function arguments.
   * Adding macros without default value to a template
   * Changing structure layouts in interface header files
   * Basically any modification that makes it impossible to use the newer
     version instead of the older one.

The second number is the minor version number. It resets to 0 whenever the
major number is incremented. Increment the minor version number whenever a
new feature is added but where backward compatibility is kept.
Such changes include:
   * Adding new features like functions, templates, header files.
   * Adding new macros with default values to a template so that the
     instantiated records do not change
   * Adding new debugging features or changing debug messages

The third number is the patch level. It resets whenever the minor version
number is changed. Increment the patch level with every bug fix. Do not add
new features without using a new minor version number.

When the current source is not tagged, not committed (or, for GIT, not
pushed) or not even tracked by CVS or GIT, then a test version is built.
By default the version string is the content of the `$USER` variable. You
can overwrite this by calling `make LIBVERSION=versionstring`. Be careful
when doing this!

:exclamation: Never "recycle" version numbers by overwriting already
installed (non-test) versions when anything has changed! Use new version
numbers!

### Global configuration

When starting, driver.makefile reads a configuration file `config` from the
directory where driver.makefile is installed, by default `/ioc/config`.
In this file, you can overwrite default configuration variables, for example
`DEFAULT_EPICS_VERSIONS`, `BUILDCLASSES`, `EPICS_MODULES`, `EPICS_LOCATION`,
`EXCLUDE_ARCHS`, or whatever variable you may want to define.

This overwrites the default settings in driver.makefile but is overwritten
by settings in the GNUmakefile of the module, which in turn is overwritten
by any variables set on the command line when `make` is called.

### `MODULE` Variable

This variable overrides the default module name, which is the name of the
directory in which `make` has been invoked, i.e. where the `GNUmakefile` is
located, typically the top level directory of the module source. Only if the
directory has the name `src` or `snl`, the name of the parent directory is
taken. It is required to change the module name if the directory contains
characters that cannot be part of a C variable name, e.g. a space, a dot, or
a minus.

:bulb: Older versions of driver.makefile used the `PROJECT` variable
instead. It is still supported for backward compatibility.

### `BUILDCLASSES` Variable

In order to stay compatible with older EPICS 3.13 drivers that are not
operating system independent, the default is to compile for vxWorks only.
This is controlled by the `BUILDCLASSES` variable. Add `Linux` to it to
compile for both, vxWorks and Linux. Replace it with `Linux` to compile for
Linux only. For Windows builds, use `WIN32` (for 64 bit builds too).

_PSI: The `BUILDCLASSES` variable is overwritten in the
[global configuration](#global-configuration) to contain all three,
`Linux`, `vxWorks` and `WIN32`. Overwrite it if your module does not compile
for all OS classes._

:exclamation: Not all cross builds are necessarily available for all
faclilities. It depends on what is included in the EPICS installations.

**Example:**
```
BUILDCLASSES = Linux
```

:exclamation: Older code that uses vxWorks specific functions or headers will
fail to compile on any other operating system.

### `EPICS_VERSIONS` and `DEFAULT_EPICS_VERSIONS` Variables
The variable `DEFAULT_EPICS_VERSIONS` contains a list or EPICS base versions
to use for building modules. This variable is a candidate to be modified in
the [global configuration](#global-configuration) as it may change over the
years.
**Example:**
```
DEFAULT_EPICS_VERSIONS = 3.14.12 3.15.5 7.0.4.1
```

:exclamation: Do *not* set this variable in the `GNUmakefile` of a module as
this would make the module incapable of being reduilt with future EPICS base
installations.

It is not necessary, that all listed EPICS base versions are actually
available on the compile host (some may only be available on certain host OS
versions or architectures due to compiler requirements).

The variable `EPICS_VERSIONS` contains all elements of
`DEFAULT_EPICS_VERSIONS` which are actually available (for which the
configured compiler is found).

I strongly suggest not to overwrite this variable in the module either.
Instead, use the `EXCLUDE_VERSIONS` variable described below.


### `EXCLUDE_VERSIONS` Variable

The default is to compile for all available EPICS versions. But sometimes
code cannot be compiled with all versions. Therefore specific versions like
3.13.10 or a group of versions like 3.14 or 7 can be excluded from
compilation.

**Example:**
```
# do not build this module for any EPICS base 3.13 or 7 versions
EXCLUDE_VERSIONS = 3.13 7
```

### `EXCLUDE_ARCHS` Variable

If code cannot compile for some IOC architectures, you can skip them.
All architectures starting or ending with one of the words listed here are
excluded from building.

**Example:**
```
# do not build for mvl40-xscale_be or any eldk* or any *ppc604 architecture
EXCLUDE_ARCHS = mvl40-xscale_be eldk ppc604
```

### `ARCH_FILTER` Variable

Similar to `EXCLUDE_ARCHS` but more flexible and defining a positive list
instead of a negative list. Build only for architectures that match one of
the patterns in the variable. Use `%` as a wildcard (only once per pattern).

**Example:**
```
# build only for Scientific Linux and for PPC 604 architectures
ARCH_FILTER = SL% %ppc604
```

:exclamation: Which architectures are available depends on the EPICS base
installations. Some facilities may add architectures to the standard set.

_PSI: Instead of the standard `linux-x86` and `linux-x86_64`, we use more
specific names which allows us to support code for different Linux versions
on the same NFS server. Currently supported Linux host architectures are
`SL6-x86_64` and `RHEL-x86_64`._

### Source Code and `SOURCES` Variable

A module can consist of C/C++ code implementing EPICS drivers, device
support, additional record types, _sub_/_genSub_/_aSub_ record functions,
SNL (State Notation Language) code, or any other code that should run on
an IOC. All the module code is compiled and linked into one dynamically
loadable library for each target architecture and EPICS version.

By default, driver.makefile finds source code files automatically: All C/C++
(`*.c`, `*.cc`, `*.cpp`) and SNL (`*.st`, `*.stt`) code in the top level
directory of the module. Hidden files (staring with `.`) as well as files
starting with `~` (backup files of some editors) are ignored.

To change this, list the source code files in the variable `SOURCES`. If 
that variable is defined, no automatic detection of source code is done.
For code only to be compiled with certain EPICS versions, OS classes, or
architectures, use variables like `SOURCES_*`, for example `SOURCES_3.13`,
`SOURCES_3.14.12`, `SOURCES_7`, `SOURCES_Linux`, `SOURCES_vxWorks`,
`SOURCES_7.0.6_SL6`.

For backward compatibility, `SOURCES_3.14`, `SOURCES_3.14_Linux` and such
are built for any EPICS release from 3.14 on like 3.15 and 7.

**Example:**
```
include /ioc/tools/driver.makefile
SOURCES += mycode.c
SOURCES += subdir/othercode.cc
SOURCES += statemachine.st
SOURCES_3.13 += codeOnlyFor3.13.c
SOURCES_3.14 += codeFor3.14orHigherIncluding7.c
SOURCES_3.14.12 += codeOnlyFor3.14.12.c
SOURCES_3 += codeOnlyFor3.c
SOURCES_7 += codeOnlyFor7.c
```

If all files are OS class specific or EPICS base version specific so that
`SOURCES` would be empty, automatic source code detection can be suppressed
by setting `SOURCES=-none-`.

### DBD Files and `DBDS` Variable

A module often also contains one or more DBD files (for record types,
device support, IOC functions,...).

Some DBD files are also generated automatically if necessary:
For EPICS 3.14, a DBD file is generated for each SNL source file (`*.st`,
`*.stt`) containing the names of the state programs.

By default all DBD files in the top level directory are combined into one
module DBD file for each EPICS version (the files may differ in details
between different EPICS versions).

To change this, list the files in the variable `DBDS`. EPICS base version
dependent files can be listed in variables like `DBD_3.13` or `DBD_3.14.12`.

:exclamation: There is no support for architecture dependent DBD files.

### Header Files and Dependencies, `HEADERS`, `*_VERSION`, `REQUIRED` and `IGNORE_MODULES` Variables

If a module provides features (in particular functions) to be used by other
modules, it contains one or more C/C++  header files which can be included
by the code of other modules. All such header files must be specified in the
variable `HEADERS`.

:exclamation: Only install the **interface headers**, i.e. those header
files that are required by *other* (dependent) code. Do **not** simply
install all header files found in the module!

Some header files are generated automatically, in particular record type and
menu headers, which are created from DBD files. These header files are
automatically installed and need not be added to the `HEADERS` variable.

The header search path of the compiler (`-I`) is set up automatically to
find the header files of the highest version of each other installed module.
If necessary, it is possible to use a different version of a module by
defining a variable `<module>_VERSION`.

**Example:**
```
asyn_VERSION = 4.8.1
```

:bulb: As *all* other modules are searched for header files (in unspecified
order), it makes sense to avoid too generic header file names, such as
`version.h`. At least to not install them.

:bulb: OS class dependent header files which are located in an appropriate
subdirectory like `os/Linux/` keep their location in such a subdirectory
and will not found when compiling other OS classes.

Including an installed header file of an other module creates a dependency
that is detected automatically by driver.makefile and resolved by `require`.

Dependencies that cannot be detected automatically (because no header file
is involved, e.g. when a db template file is required) can be added manually
using one the variables `REQUIRED`, `REQUIRED_<EPICS_version>`,
`REQUIRED_<OS_CLASS>`, `REQUIRED_<ARCHITECTURE>` or similar.

**Example:**
```
REQUIRED_3.13 = timestamp
```

When a module is loaded with `require`, all its dependencies are resolved by
first loading highest compatible minor version of the depended on module.

:exclamation: Some third party software modules follow other version numbering
conventions and may no adhere to the rule not to break backward compatibility
without incrementing the major version number. In such cases, define either
`USE_EXACT_MINOR_VERSION` or even `USE_EXACT_VERSION` in the
`GNUmakefile` of that module. This will prevent `reqire` to assume that 
higher minor versions (or even patch levels) are backward compatible when
a dependency on that module is found.

Problematic header files installed by another module may be ignored, e.g.
in case of file name clashes. Set the variable `IGNORE_MODULES` to a list
of modules whose header files should not be found automatically.

**Example:**
```
IGNORE_MODULES = motorBase asynMotor
```

:exclamation: Do not install different versions a module with headers under
different names (for the same EPICS version).
This will inevitably create file name clashes which cause problems to all
other modules that use this module.


### Template Files and `TEMPLATES` Variable

Modules can also provide db EPICS template files. This is useful if a driver
is typically used with one or more matching template files. The template files
are installed together with the other module files for each version. Thus they
may differ between versions (e.g. in order to match the changing features of
the driver). It is even possible to have module that contain only template
files and no code. Also substitution files may be provided here.

By default, all templates (`*.template`, `*.db`) and substitution files
(`*.subs`) in the top level directory are installed.

To change this, list the files in the variable `TEMPLATES` or
`TEMPLATES_<EPICS_version>`.

### Startup Scripts Snippets and `SCRIPTS` Variable

A module can contain startup script snippets to be executed automatically
after a module is loaded or as requested by the user.
(See [above](#startup-script-snippets).)

The script snippets may contain macros using the `$(MACRO)` or
`$(MACRO=default)` syntax which are replaced by actual values passed to the
`require` command as `"MACRO=VALUE, ..."`, by environment variables (in
particular `$(IOC)` is the IOC name when using the `iocsh` wrapper script),
or using a default value (in that precedence).
See [above](#environment-variables) for environment variables that are set by
`require` automatically.

By default, all `*.cmd` files in the top level directory are installed.
To change this, list the files in the variable `SCRIPTS` or
`SCRIPTS_<EPICS_version>`.

:exclamation: Please be aware that environment variables set in the startup
script snippet with `epicsEnvSet` keep their values even after the script has
finished and may affect other modules loaded later.

To set local variables, use the syntax `variable=value` instead. These
do not keep their values when the script snippet has finished.
For more details see [above](#local-script-variables).

### UI Screens and `QT` Variable

A module may have related user interface (UI) screens. They can be installed
with `make installui` and uninstalled with `make uninstallui`.

Currently caQtDM UIs are supported. By default all `qt/*` files are installed,
but that can be overwritten with the `QT` variable.

The installation location is *not* the module install directory, but the
global location `${CONFIGBASE}/qt/` if `CONFIGBASE` is defined. If not
the location is `${EPICS_MODULES}/qt/` which in turn defaults to
`/ioc/modules/qt/`.

:bulb: Other UI types may be supported by defining a `INSTALL_UI_RULE`,
passing the name of the variable, the install location and the default search
pattern. This is how `QT` files are configured:
```
$(eval $(call INSTALL_UI_RULE,QT,${CONFIGBASE}/qt,qt/*))
```

It is not possible to have different versions installed like for the modules
themselves, because there is no way for a client to know which module
versions are currently loaded on the IOCs (which may even vary among IOCs).
It is assumed that the latest UI version is more or less compatible with older
module versions.

For this reason, you have to type `make installui` explicitly and not
only `make install`. This will first uninstall all UI files beonging to older
versions of the module, just like `make uninstallui`.
(For this purpose, driver.makefile tracks the installed UIs in a hidden file
in the install location).


## Building EPICS 3.13 modules for 3.14 or higher

The way EPICS software is built has changed a lot from R3.13 to R3.14 due to
the requirement to run on other oprating systems than just vxWorks. This
makes it hard to write a module that compiles with both EPICS release
families. However, driver.makefile contains some features to hide this from
the developper.

For most 3.13 code, it is possible to compile it straight forward or with
minor modifications for 3.14. The additional code that is required by
EPICS 3.14 is generated automatically by driver.makefile. Of course, code
using vxWorks functions and headers cannot be compiled for other operating
systems, even when using EPICS 3.14.

For a wide range of 3.14 code, it is possible to compile it for 3.13 without
modifications. New 3.14 specific functions come from a special compatibility
library which is loaded automatically when loading this module. EPICS 3.14
specific DBD file entries are automatically deleted from module DBD files
for 3.13.

### Undefined Functions

Some EPICS 3.14 header files do not include all other headers they did in
3.13. So it may happen that you have to put additional `#include` directives
into 3.13 code to compile it for 3.14. A typical candidate is recGbl.h which
is no longer included by regSup.h. This modification is fully backward
compatible.

### C++ Problems

EPICS 3.13 was not C++ aware. Thus in 3.13, EPICS headers **have to** be
wrapped in `extern "C" { }`. But EPICS 3.14 does use C++, so in 3.14, EPICS
headers **must not** be wrapped. This can be solved by including
epicsVersion.h and some `#ifdef BASE_VERSION` statements.

This works because the macro `BASE_VERSION` is not defined in 3.14. any more.
Only wrap EPICS headers. Do **not** wrap operating system headers like
stdio.h.

**Example:**
```
// #include system headers here

#include <epicsVersion.h>
#ifdef BASE_VERSION
// This is for EPICS 3.13
extern "C" {
#endif

// #include EPICS headers here

#ifdef BASE_VERSION
}
#endif
```

### Static Device Support Structures

In EPICS 3.14, it is possible to define device support and driver structures
`static`. This is not compatible with EPICS 3.13. To compile the code for
EPICS 3.13., remove the `static` keyword (sometimes hidden in a macro called
`LOCAL`).

## Third Party Packages

Usually third party packages come with a structure and Makefiles that follow
the standard makeBaseApp project layout. They typically have a `Makefile`
that works with either EPICS 3.13 or 3.14 but not both. And often source
files are located in sub-directories. The easiest way to deal with these
projects is to create a `GNUmakefile` in the top level directory and list
all require source files in the `SOURCES` variable.

## Find Out Which Module Is Required

_PSI: If you do not know which modules are needed for a given set of records,
use `externalLinks`:_

```
externalLinks --require *.subs
```

_In addition to printing the list of link targets not resolved internally
within the records defined by the given substitution files and thus
finding potentioal typos (the original purpose of the tool), with the
`--require` option it also prints a list of modules required. It does this
by searching all installed modules for missing record types device supports._
