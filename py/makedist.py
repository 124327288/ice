#!/usr/bin/env python
# **********************************************************************
#
# Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import os, sys, shutil, fnmatch, re

#
# Show usage information.
#
def usage():
    print "Usage: " + sys.argv[0] + " [options] [tag]"
    print
    print "Options:"
    print "-h    Show this message."
    print "-v    Be verbose."
    print
    print "If no tag is specified, HEAD is used."

#
# Find files matching a pattern.
#
def find(path, patt):
    result = [ ]
    files = os.listdir(path)
    for x in files:
        fullpath = os.path.join(path, x);
        if os.path.isdir(fullpath) and not os.path.islink(fullpath):
            result.extend(find(fullpath, patt))
        elif fnmatch.fnmatch(x, patt):
            result.append(fullpath)
    return result

#
# Are we on Windows?
#
win32 = sys.platform.startswith("win") or sys.platform.startswith("cygwin")

#
# Check arguments
#
tag = "-rHEAD"
verbose = 0
for x in sys.argv[1:]:
    if x == "-h":
        usage()
        sys.exit(0)
    elif x == "-v":
        verbose = 1
    elif x.startswith("-"):
        print sys.argv[0] + ": unknown option `" + x + "'"
        print
        usage()
        sys.exit(1)
    else:
        tag = "-r" + x

#
# Remove any existing "dist" directory and create a new one.
#
distdir = "dist"
if os.path.exists(distdir):
    shutil.rmtree(distdir)
os.mkdir(distdir)
os.chdir(distdir)

#
# Export Python sources from CVS.
#
print "Checking out CVS tag " + tag + "..."
if verbose:
    quiet = ""
else:
    quiet = "-Q"
os.system("cvs " + quiet + " -d cvs.mutablerealms.com:/home/cvsroot export " + tag + " icepy")

#
# Export C++ sources.
#
# NOTE: Assumes that the C++ and Python trees will use the same tag.
#
os.system("cvs " + quiet + " -d cvs.mutablerealms.com:/home/cvsroot export " + tag + " ice/slice")
#
# Copy Slice directories.
#
print "Copying Slice directories..."
slicedirs = [\
    "Glacier",\
    "Glacier2",\
    "Ice",\
    "IceBox",\
    "IcePack",\
    "IcePatch",\
    "IceStorm",\
]
os.mkdir(os.path.join("icepy", "slice"))
for x in slicedirs:
    shutil.copytree(os.path.join("ice", "slice", x), os.path.join("icepy", "slice", x), 1)

#
# Remove files.
#
print "Removing unnecessary files..."
filesToRemove = [ \
    os.path.join("icepy", "makedist.py"), \
    ]
for x in filesToRemove:
    os.remove(x)

#
# Translate Slice files.
#
print "Translating..."
cwd = os.getcwd()
os.chdir(os.path.join("icepy", "python"))
if verbose:
    quiet = ""
else:
    quiet = " -s"
os.system("gmake" + quiet)
os.chdir(cwd)

#
# Get Ice version.
#
config = open(os.path.join("icepy", "config", "Make.rules"), "r")
version = re.search("VERSION.*= ([0-9\.]*)", config.read()).group(1)

#
# Create source archives.
#
print "Creating distribution..."
icever = "IcePy-" + version
os.rename("icepy", icever)
if verbose:
    quiet = "v"
else:
    quiet = ""
os.system("tar c" + quiet + "zf " + icever + ".tar.gz " + icever)
if verbose:
    quiet = ""
else:
    quiet = "-q"
os.system("zip -9 -r " + quiet + " " + icever + ".zip " + icever)

#
# Copy files (README, etc.).
#
#shutil.copyfile(os.path.join(icever, "CHANGES"), "IcePy-" + version + "-CHANGES")

#
# Done.
#
print "Cleaning up..."
shutil.rmtree(icever)
shutil.rmtree("ice")
print "Done."
