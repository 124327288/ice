// **********************************************************************
//
// Copyright (c) 2003 - 2004
// ZeroC, Inc.
// North Palm Beach, FL, USA
//
// All Rights Reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************


public class AllTests
{
    private static void
    test(boolean b)
    {
        if (!b)
        {
            throw new RuntimeException();
        }
    }

    public static TestPrx
    allTests(Ice.Communicator communicator)
    {
	System.out.print("testing stringToProxy... ");
	System.out.flush();
	String ref = "test @ TestAdapter";
	Ice.ObjectPrx base = communicator.stringToProxy(ref);
	test(base != null);
	System.out.println("ok");

	System.out.print("testing checked cast... ");
	System.out.flush();
	TestPrx obj = TestPrxHelper.checkedCast(base);
	test(obj != null);
	test(obj.equals(base));
	System.out.println("ok");
	
	System.out.print("pinging server... ");
	System.out.flush();
	obj.ice_ping();
	System.out.println("ok");
	
	return obj;
    }

    public static TestPrx
    allTestsWithDeploy(Ice.Communicator communicator)
    {
	System.out.print("testing stringToProxy... ");
	System.out.flush();
	Ice.ObjectPrx base = communicator.stringToProxy("test @ TestAdapter");
	test(base != null);
	Ice.ObjectPrx base2 = communicator.stringToProxy("test");
	test(base2 != null);
	System.out.println("ok");

	System.out.print("testing checked cast... ");
	System.out.flush();
	TestPrx obj = TestPrxHelper.checkedCast(base);
	test(obj != null);
	test(obj.equals(base));
	TestPrx obj2 = TestPrxHelper.checkedCast(base2);
	test(obj2 != null);
	test(obj2.equals(base2));
	System.out.println("ok");
	
	System.out.print("pinging server... ");
	System.out.flush();
	obj.ice_ping();
	obj2.ice_ping();
	System.out.println("ok");

	System.out.print("testing reference with unknown identity... ");
	System.out.flush();
	try
	{
	    communicator.stringToProxy("unknown/unknown").ice_ping();
	    test(false);
	}
	catch(Ice.NotRegisteredException ex)
	{
	    test(ex.kindOfObject.equals("object"));
	    test(ex.id.equals("unknown/unknown"));
	}
	System.out.println("ok");	

	System.out.print("testing reference with unknown adapter... ");
	System.out.flush();
	try
	{
	    communicator.stringToProxy("test @ TestAdapterUnknown").ice_ping();
	    test(false);
	}
	catch(Ice.NotRegisteredException ex)
	{
	    test(ex.kindOfObject.equals("object adapter"));
	    test(ex.id.equals("TestAdapterUnknown"));
	}
	System.out.println("ok");	

	IcePack.AdminPrx admin = IcePack.AdminPrxHelper.checkedCast(communicator.stringToProxy("IcePack/Admin"));
	test(admin != null);

	try
	{
	    admin.setServerActivation("server", IcePack.ServerActivation.Manual);
	    admin.stopServer("server");
	}
	catch(IcePack.ServerNotExistException ex)
	{
	    test(false);
	}
	catch(IcePack.NodeUnreachableException ex)
	{
	    test(false);
	}

	System.out.print("testing whether server is still reachable... ");
	System.out.flush();
	try
	{
	    obj = TestPrxHelper.checkedCast(base);
	    test(false);
	}
	catch(Ice.NoEndpointException ex)
	{
	}
	try
	{
	    obj2 = TestPrxHelper.checkedCast(base2);
	    test(false);
	}
	catch(Ice.NoEndpointException ex)
	{
	}
	
	try
	{
	    admin.setServerActivation("server", IcePack.ServerActivation.OnDemand);
	}
	catch(IcePack.ServerNotExistException ex)
	{
	    test(false);
	}
	catch(IcePack.NodeUnreachableException ex)
	{
	    test(false);
	}

	try
	{
	    obj = TestPrxHelper.checkedCast(base);
	}
	catch(Ice.NoEndpointException ex)
	{
	    test(false);
	}
	try
	{
	    obj2 = TestPrxHelper.checkedCast(base2);
	}
	catch(Ice.NoEndpointException ex)
	{
	    test(false);
	}
	System.out.println("ok");

	return obj;
    }
}
