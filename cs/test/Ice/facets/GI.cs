// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************


public sealed class GI : _GDisp
{
    public GI(Ice.Communicator communicator)
    {
        _communicator = communicator;
    }
    
    public override string callG(Ice.Current current)
    {
        return "G";
    }
    
    public override void shutdown(Ice.Current current)
    {
        _communicator.shutdown();
    }
    
    private Ice.Communicator _communicator;
}
