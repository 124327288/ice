// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

public class FreezeService extends Ice.LocalObjectImpl implements IceBox.FreezeService
{
    public void
    start(String name, Ice.Communicator communicator, String[] args, String envName)
        throws IceBox.FailureException
    {
	Ice.Properties properties = communicator.getProperties();

        Ice.ObjectAdapter adapter = communicator.createObjectAdapter(name);
        Ice.Object object = new TestI(adapter, communicator.getProperties());
        adapter.add(object, Ice.Util.stringToIdentity(properties.getProperty(name + ".Identity")));
        adapter.activate();
    }

    public void
    stop()
    {
    }
}
