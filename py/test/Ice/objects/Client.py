# **********************************************************************
#
# Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

import os, sys, traceback

for toplevel in [".", "..", "../..", "../../..", "../../../.."]:
    toplevel = os.path.normpath(toplevel)
    if os.path.exists(os.path.join(toplevel, "python", "Ice.py")):
        break
else:
    raise "can't find toplevel directory!"

sys.path.insert(0, os.path.join(toplevel, "python"))
sys.path.insert(0, os.path.join(toplevel, "lib"))

import Ice, AllTests, TestI

class MyObjectFactory(Ice.ObjectFactory):
    def create(self, type):
        if type == '::Test::B':
            return TestI.BI()
        elif type == '::Test::C':
            return TestI.CI()
        elif type == '::Test::D':
            return TestI.DI()
        assert(False) # Should never be reached

    def destroy(self):
        # Nothing to do
        pass

def run(args, communicator):
    factory = MyObjectFactory()
    communicator.addObjectFactory(factory, '::Test::B')
    communicator.addObjectFactory(factory, '::Test::C')
    communicator.addObjectFactory(factory, '::Test::D')

    initial = AllTests.allTests(communicator)
    initial.shutdown()
    return True

try:
    communicator = Ice.initialize(sys.argv)
    status = run(sys.argv, communicator)
except:
    traceback.print_exc()
    status = False

if communicator:
    try:
        communicator.destroy()
    except:
        traceback.print_exc()
        status = False

sys.exit(not status)
