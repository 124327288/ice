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
        if(!b)
        {
            throw new RuntimeException();
        }
    }

    private static class Callback
    {
	Callback()
	{
	    _called = false;
	}

	public synchronized boolean
	check()
	{
	    while(!_called)
	    {
		try
		{
		    wait(5000);
		}
		catch(InterruptedException ex)
		{
		    continue;
		}

		if(!_called)
		{
		    return false; // Must be timeout.
		}
	    }

	    _called = false;
	    return true;
	}
	
	public synchronized void
	called()
	{
	    assert(!_called);
	    _called = true;
	    notify();
	}

	private boolean _called;
    }

    private static class AMI_Test_baseAsBaseI extends AMI_Test_baseAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(Base b)
	    {
		test(b.b.equals("Base.b"));
		test(b.ice_name().equals("Base"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_unknownDerivedAsBaseI extends AMI_Test_unknownDerivedAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownDerived.b"));
		test(b.ice_name().equals("Base"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownDerivedAsBaseI extends AMI_Test_knownDerivedAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownDerived k)
	    {
		test(k.b.equals("KnownDerived.b"));
		test(k.kd.equals("KnownDerived.kd"));
		test(k.ice_name().equals("KnownDerived"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownDerivedAsKnownDerivedI extends AMI_Test_knownDerivedAsKnownDerived
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownDerived k)
	    {
		test(k.b.equals("KnownDerived.b"));
		test(k.kd.equals("KnownDerived.kd"));
		test(k.ice_name().equals("KnownDerived"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_unknownIntermediateAsBaseI extends AMI_Test_unknownIntermediateAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownIntermediate.b"));
		test(b.ice_name().equals("Base"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownIntermediateAsBaseI extends AMI_Test_knownIntermediateAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("KnownIntermediate.b"));
		test(ki.ki.equals("KnownIntermediate.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownMostDerivedAsBaseI extends AMI_Test_knownMostDerivedAsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownIntermediateAsKnownIntermediateI
	extends AMI_Test_knownIntermediateAsKnownIntermediate
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("KnownIntermediate.b"));
		test(ki.ki.equals("KnownIntermediate.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownMostDerivedAsKnownIntermediateI
	extends AMI_Test_knownMostDerivedAsKnownIntermediate
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_knownMostDerivedAsKnownMostDerivedI
	extends AMI_Test_knownMostDerivedAsKnownMostDerived
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_unknownMostDerived1AsBaseI extends AMI_Test_unknownMostDerived1AsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("UnknownMostDerived1.b"));
		test(ki.ki.equals("UnknownMostDerived1.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_unknownMostDerived1AsKnownIntermediateI
	extends AMI_Test_unknownMostDerived1AsKnownIntermediate
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("UnknownMostDerived1.b"));
		test(ki.ki.equals("UnknownMostDerived1.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    private static class AMI_Test_unknownMostDerived2AsBaseI extends AMI_Test_unknownMostDerived2AsBase
    {
        public void
	ice_response()
	{
	    test(false);
	}

	public void
	ice_exception(Ice.LocalException exc)
	{
	    test(false);
	}

	public void
	ice_exception(Ice.UserException exc)
	{
	    try
	    {
	        throw exc;
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownMostDerived2.b"));
		test(b.ice_name().equals("Base"));
	    }
	    catch(Exception ex)
	    {
	        test(false);
	    }
	    callback.called();
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    }

    public static TestPrx
    allTests(Ice.Communicator communicator, boolean collocated)
    {
        System.out.print("testing stringToProxy... ");
        System.out.flush();
        String ref = "Test:default -p 12345 -t 2000";
        Ice.ObjectPrx base = communicator.stringToProxy(ref);
        test(base != null);
        System.out.println("ok");

        System.out.print("testing checked cast... ");
        System.out.flush();
        TestPrx test = TestPrxHelper.checkedCast(base);
        test(test != null);
        test(test.equals(base));
        System.out.println("ok");

	System.out.print("base... ");
        System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.baseAsBase();
	    }
	    catch(Base b)
	    {
		test(b.b.equals("Base.b"));
		test(b.ice_name().equals("Base"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("base (AMI)... ");
        System.out.flush();
	{
	    AMI_Test_baseAsBaseI cb = new AMI_Test_baseAsBaseI();
	    test.baseAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of unknown derived... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.unknownDerivedAsBase();
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownDerived.b"));
		test(b.ice_name().equals("Base"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of unknown derived (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_unknownDerivedAsBaseI cb = new AMI_Test_unknownDerivedAsBaseI();
	    test.unknownDerivedAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("non-slicing of known derived as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownDerivedAsBase();
	    }
	    catch(KnownDerived k)
	    {
		test(k.b.equals("KnownDerived.b"));
		test(k.kd.equals("KnownDerived.kd"));
		test(k.ice_name().equals("KnownDerived"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("non-slicing of known derived as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownDerivedAsBaseI cb = new AMI_Test_knownDerivedAsBaseI();
	    test.knownDerivedAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("non-slicing of known derived as derived... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownDerivedAsKnownDerived();
	    }
	    catch(KnownDerived k)
	    {
		test(k.b.equals("KnownDerived.b"));
		test(k.kd.equals("KnownDerived.kd"));
		test(k.ice_name().equals("KnownDerived"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("non-slicing of known derived as derived (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownDerivedAsKnownDerivedI cb = new AMI_Test_knownDerivedAsKnownDerivedI();
	    test.knownDerivedAsKnownDerived_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of unknown intermediate as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.unknownIntermediateAsBase();
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownIntermediate.b"));
		test(b.ice_name().equals("Base"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of unknown intermediate as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_unknownIntermediateAsBaseI cb = new AMI_Test_unknownIntermediateAsBaseI();
	    test.unknownIntermediateAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of known intermediate as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownIntermediateAsBase();
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("KnownIntermediate.b"));
		test(ki.ki.equals("KnownIntermediate.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of known intermediate as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownIntermediateAsBaseI cb = new AMI_Test_knownIntermediateAsBaseI();
	    test.knownIntermediateAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of known most derived as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownMostDerivedAsBase();
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of known most derived as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownMostDerivedAsBaseI cb = new AMI_Test_knownMostDerivedAsBaseI();
	    test.knownMostDerivedAsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("non-slicing of known intermediate as intermediate... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownIntermediateAsKnownIntermediate();
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("KnownIntermediate.b"));
		test(ki.ki.equals("KnownIntermediate.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("non-slicing of known intermediate as intermediate (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownIntermediateAsKnownIntermediateI cb = new AMI_Test_knownIntermediateAsKnownIntermediateI();
	    test.knownIntermediateAsKnownIntermediate_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("non-slicing of known most derived as intermediate... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownMostDerivedAsKnownIntermediate();
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("non-slicing of known most derived as intermediate (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownMostDerivedAsKnownIntermediateI cb = new AMI_Test_knownMostDerivedAsKnownIntermediateI();
	    test.knownMostDerivedAsKnownIntermediate_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("non-slicing of known most derived as most derived... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.knownMostDerivedAsKnownMostDerived();
	    }
	    catch(KnownMostDerived kmd)
	    {
		test(kmd.b.equals("KnownMostDerived.b"));
		test(kmd.ki.equals("KnownMostDerived.ki"));
		test(kmd.kmd.equals("KnownMostDerived.kmd"));
		test(kmd.ice_name().equals("KnownMostDerived"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("non-slicing of known most derived as most derived (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_knownMostDerivedAsKnownMostDerivedI cb = new AMI_Test_knownMostDerivedAsKnownMostDerivedI();
	    test.knownMostDerivedAsKnownMostDerived_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, known intermediate as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.unknownMostDerived1AsBase();
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("UnknownMostDerived1.b"));
		test(ki.ki.equals("UnknownMostDerived1.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, known intermediate as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_unknownMostDerived1AsBaseI cb = new AMI_Test_unknownMostDerived1AsBaseI();
	    test.unknownMostDerived1AsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, known intermediate as intermediate... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.unknownMostDerived1AsKnownIntermediate();
	    }
	    catch(KnownIntermediate ki)
	    {
		test(ki.b.equals("UnknownMostDerived1.b"));
		test(ki.ki.equals("UnknownMostDerived1.ki"));
		test(ki.ice_name().equals("KnownIntermediate"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, known intermediate as intermediate (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_unknownMostDerived1AsKnownIntermediateI cb =
		new AMI_Test_unknownMostDerived1AsKnownIntermediateI();
	    test.unknownMostDerived1AsKnownIntermediate_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, unknown intermediate thrown as base... ");
	System.out.flush();
	{
	    boolean gotException = false;
	    try
	    {
		test.unknownMostDerived2AsBase();
	    }
	    catch(Base b)
	    {
		test(b.b.equals("UnknownMostDerived2.b"));
		test(b.ice_name().equals("Base"));
		gotException = true;
	    }
	    catch(Exception ex)
	    {
		test(false);
	    }
	    test(gotException);
	}
	System.out.println("ok");

	System.out.print("slicing of unknown most derived, unknown intermediate thrown as base (AMI)... ");
	System.out.flush();
	{
	    AMI_Test_unknownMostDerived2AsBaseI cb = new AMI_Test_unknownMostDerived2AsBaseI();
	    test.unknownMostDerived2AsBase_async(cb);
	    test(cb.check());
	}
	System.out.println("ok");

        return test;
    }
}
