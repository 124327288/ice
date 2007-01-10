// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

using Test;

public class Client
{
    internal class TestClient : Ice.Application
    {
        public override int run(string[] args)
        {
	    TestIntfPrx obj = AllTests.allTests(communicator(), false);
	    obj.shutdown();
            return 0;
        }
    }
    
    public static void Main(string[] args)
    {
        TestClient app = new TestClient();
        int result = app.main(args);
	if(result != 0)
	{
	    System.Environment.Exit(result);
	}
    }
}
