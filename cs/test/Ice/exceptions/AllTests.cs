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


using System;
using System.Diagnostics;
using System.Threading;

public class AllTests
{
    private static void test(bool b)
    {
	if(!b)
	{
	    throw new System.Exception();
	}
    }
    
    private class Callback
    {
	internal Callback()
	{
	    _called = false;
	}
	
	public virtual bool check()
	{
	    lock(this)
	    {
		while(!_called)
		{
		    try
		    {
			Monitor.Wait(this, TimeSpan.FromMilliseconds(5000));
		    }
		    catch(ThreadInterruptedException)
		    {
			continue;
		    }
		    
		    if(!_called)
		    {
			return false; // Must be timeout.
		    }
		}
		
		return true;
	    }
	}
	
	public virtual void called()
	{
	    lock(this)
	    {
		Debug.Assert(!_called);
		_called = true;
		Monitor.Pulse(this);
	    }
	}
	
	private bool _called;
    }
    
/*
    private class AMI_Thrower_throwAasAI:AMI_Thrower_throwAasA
    {
	public AMI_Thrower_throwAasAI()
	{
	    InitBlock();
	}
	private void InitBlock()
	{
	    callback = new Callback();
	}
	public override void ice_response()
	{
	    AllTests.test(false);
	}
	
	public virtual void ice_exception(Ice.LocalException exc)
	{
	    AllTests.test(false);
	}
	
	public virtual void ice_exception(Ice.UserException exc)
	{
	    try
	    {
		throw exc;
	    }
	    catch(A ex)
	    {
		AllTests.test(ex.aMem == 1);
	    }
	    catch(Exception ex)
	    {
		AllTests.test(false);
	    }
	    callback.called();
	}
	
	public virtual bool check()
	{
	    return callback.check();
	}
	
	private Callback callback;
    }
    
    private class AMI_Thrower_throwAasAObjectNotExistI:AMI_Thrower_throwAasA
    {
	public AMI_Thrower_throwAasAObjectNotExistI()
	{
	    InitBlock();
	}
	private void InitBlock()
	{
	    callback = new Callback();
	}
	public override void ice_response()
	{
	    AllTests.test(false);
	}
	
	public virtual void ice_exception(Ice.LocalException exc)
	{
	    try
	    {
		throw exc;
	    }
	    catch(Ice.ObjectNotExistException ex)
	    {
		Ice.Identity id = Ice.Util.stringToIdentity("does not exist");
		AllTests.test(ex.id.Equals(id));
	    }
	    catch(Exception ex)
	    {
		AllTests.test(false);
	    }
	    callback.called();
	}
	
	public virtual void ice_exception(Ice.UserException exc)
	{
	    AllTests.test(false);
	}
	
	public virtual bool check()
	{
	    return callback.check();
	}
	
	private Callback callback;
    }
    
    private class AMI_Thrower_throwAasAFacetNotExistI:AMI_Thrower_throwAasA
    {
	public AMI_Thrower_throwAasAFacetNotExistI()
	{
	    InitBlock();
	}
	private void InitBlock()
	{
	    callback = new Callback();
	}
	public override void ice_response()
	{
	    AllTests.test(false);
	}
	
	public virtual void ice_exception(Ice.LocalException exc)
	{
	    try
	    {
		throw exc;
	    }
	    catch(Ice.FacetNotExistException ex)
	    {
		AllTests.test(ex.facet.length == 1);
		AllTests.test(ex.facet[0].Equals("no such facet"));
	    }
	    catch(Exception ex)
	    {
		AllTests.test(false);
	    }
	    callback.called();
	}
	
	public virtual void ice_exception(Ice.UserException exc)
	{
	    AllTests.test(false);
	}
	
	public virtual bool check()
	{
	    return callback.check();
	}
	
	private Callback callback;
    }
    
    private class AMI_Thrower_throwAasAFacetNotExist2I:AMI_Thrower_throwAasA
    {
	public AMI_Thrower_throwAasAFacetNotExist2I()
	{
	    InitBlock();
	}
	private void InitBlock()
	{
	    callback = new Callback();
	}
	public override void ice_response()
	{
	    AllTests.test(false);
	}
	
	public virtual void ice_exception(Ice.LocalException exc)
	{
	    try
	    {
		throw exc;
	    }
	    catch(Ice.FacetNotExistException ex)
	    {
		AllTests.test(ex.facet.length == 2);
		AllTests.test(ex.facet[0].Equals("no such facet"));
		AllTests.test(ex.facet[1].Equals("no such facet either"));
	    }
	    catch(Exception ex)
	    {
		AllTests.test(false);
	    }
	    callback.called();
	}
	
	public virtual void ice_exception(Ice.UserException exc)
	{
	    AllTests.test(false);
	}
	
	public virtual bool check()
	{
	    return callback.check();
	}
	
	private Callback callback;
    }
    
    private class AMI_Thrower_throwAorDasAorDI:AMI_Thrower_throwAorDasAorD
    {
        public AMI_Thrower_throwAorDasAorDI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(A ex)
            {
                AllTests.test(ex.aMem == 1);
            }
            catch(D ex)
            {
                AllTests.test(ex.dMem == - 1);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwBasAI:AMI_Thrower_throwBasA
    {
        public AMI_Thrower_throwBasAI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(B ex)
            {
                AllTests.test(ex.aMem == 1);
                AllTests.test(ex.bMem == 2);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwCasAI:AMI_Thrower_throwCasA
    {
        public AMI_Thrower_throwCasAI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(C ex)
            {
                AllTests.test(ex.aMem == 1);
                AllTests.test(ex.bMem == 2);
                AllTests.test(ex.cMem == 3);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwBasBI:AMI_Thrower_throwBasB
    {
        public AMI_Thrower_throwBasBI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(B ex)
            {
                AllTests.test(ex.aMem == 1);
                AllTests.test(ex.bMem == 2);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwCasBI:AMI_Thrower_throwCasB
    {
        public AMI_Thrower_throwCasBI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(C ex)
            {
                AllTests.test(ex.aMem == 1);
                AllTests.test(ex.bMem == 2);
                AllTests.test(ex.cMem == 3);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwCasCI:AMI_Thrower_throwCasC
    {
        public AMI_Thrower_throwCasCI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.UserException exc)
        {
            try
            {
                throw exc;
            }
            catch(C ex)
            {
                AllTests.test(ex.aMem == 1);
                AllTests.test(ex.bMem == 2);
                AllTests.test(ex.cMem == 3);
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwUndeclaredAI:AMI_Thrower_throwUndeclaredA
    {
        public AMI_Thrower_throwUndeclaredAI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.UnknownUserException ex)
            {
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwUndeclaredBI:AMI_Thrower_throwUndeclaredB
    {
        public AMI_Thrower_throwUndeclaredBI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.UnknownUserException ex)
            {
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwUndeclaredCI:AMI_Thrower_throwUndeclaredC
    {
        public AMI_Thrower_throwUndeclaredCI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.UnknownUserException ex)
            {
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwLocalExceptionI:AMI_Thrower_throwLocalException
    {
        public AMI_Thrower_throwLocalExceptionI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.UnknownLocalException ex)
            {
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_Thrower_throwNonIceExceptionI:AMI_Thrower_throwNonIceException
    {
        public AMI_Thrower_throwNonIceExceptionI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.UnknownException ex)
            {
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
    
    private class AMI_WrongOperation_noSuchOperationI:AMI_WrongOperation_noSuchOperation
    {
        public AMI_WrongOperation_noSuchOperationI()
        {
	    InitBlock();
        }
        private void InitBlock()
        {
            callback = new Callback();
        }
        public override void ice_response()
        {
            AllTests.test(false);
        }
        
        public virtual void ice_exception(Ice.LocalException exc)
        {
            try
            {
                throw exc;
            }
            catch(Ice.OperationNotExistException ex)
            {
                AllTests.test(ex.operation.Equals("noSuchOperation"));
            }
            catch(Exception ex)
            {
                AllTests.test(false);
            }
            callback.called();
        }
        
        public virtual bool check()
        {
            return callback.check();
        }
        
        private Callback callback;
    }
*/
    
    public static ThrowerPrx allTests(Ice.Communicator communicator, bool collocated)
    {
        {
            Console.Write("testing servant registration exceptions... ");
            Ice.ObjectAdapter adapter = communicator.createObjectAdapter("TestAdapter1");
            Ice.Object obj = new EmptyI();
            adapter.add(obj, Ice.Util.stringToIdentity("x"));
            bool gotException = false;
            try
            {
                adapter.add(obj, Ice.Util.stringToIdentity("x"));
            }
            catch(Ice.AlreadyRegisteredException)
            {
                gotException = true;
            }
            test(gotException);
            
            gotException = false;
            adapter.remove(Ice.Util.stringToIdentity("x"));
            try
            {
                adapter.remove(Ice.Util.stringToIdentity("x"));
            }
            catch(Ice.NotRegisteredException)
            {
                gotException = true;
            }
            test(gotException);
            adapter.deactivate();
            Console.WriteLine("ok");
        }
        
        {
            Console.Write("testing servant locator registration exceptions... ");
            Ice.ObjectAdapter adapter = communicator.createObjectAdapter("TestAdapter2");
            Ice.ServantLocator loc = new ServantLocatorI();
            adapter.addServantLocator(loc, "x");
            bool gotException = false;
            try
            {
                adapter.addServantLocator(loc, "x");
            }
            catch(Ice.AlreadyRegisteredException)
            {
                gotException = true;
            }
            test(gotException);
            
            adapter.deactivate();
            Console.WriteLine("ok");
        }
        
        {
            Console.Write("testing object factory registration exceptions... ");
            Ice.ObjectFactory of = new ObjectFactoryI();
            communicator.addObjectFactory(of, "::x");
            bool gotException = false;
            try
            {
                communicator.addObjectFactory(of, "::x");
            }
            catch(Ice.AlreadyRegisteredException)
            {
                gotException = true;
            }
            test(gotException);
            
            gotException = false;
            communicator.removeObjectFactory("::x");
            try
            {
                communicator.removeObjectFactory("::x");
            }
            catch(Ice.NotRegisteredException)
            {
                gotException = true;
            }
            test(gotException);
            Console.WriteLine("ok");
        }
        
        Console.Write("testing stringToProxy... ");
        Console.Out.Flush();
        String ref_Renamed = "thrower:default -p 12345 -t 2000";
        Ice.ObjectPrx base_Renamed = communicator.stringToProxy(ref_Renamed);
        test(base_Renamed != null);
        Console.WriteLine("ok");
        
        Console.Write("testing checked cast... ");
        Console.Out.Flush();
        ThrowerPrx thrower = ThrowerPrxHelper.checkedCast(base_Renamed);
        test(thrower != null);
        test(thrower.Equals(base_Renamed));
        Console.WriteLine("ok");
        
        Console.Write("catching exact types... ");
        Console.Out.Flush();
        
        try
        {
            thrower.throwAasA(1);
            test(false);
        }
        catch(A ex)
        {
            test(ex.aMem == 1);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwAorDasAorD(1);
            test(false);
        }
        catch(A ex)
        {
            test(ex.aMem == 1);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwAorDasAorD(- 1);
            test(false);
        }
        catch(D ex)
        {
            test(ex.dMem == - 1);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwBasB(1, 2);
            test(false);
        }
        catch(B ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwCasC(1, 2, 3);
            test(false);
        }
        catch(C ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
            test(ex.cMem == 3);
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");
        
        Console.Write("catching base types... ");
        Console.Out.Flush();
        
        try
        {
            thrower.throwBasB(1, 2);
            test(false);
        }
        catch(A ex)
        {
            test(ex.aMem == 1);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwCasC(1, 2, 3);
            test(false);
        }
        catch(B ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");
        
        Console.Write("catching derived types... ");
        Console.Out.Flush();
        
        try
        {
            thrower.throwBasA(1, 2);
            test(false);
        }
        catch(B ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwCasA(1, 2, 3);
            test(false);
        }
        catch(C ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
            test(ex.cMem == 3);
        }
        catch(Exception)
        {
            test(false);
        }
        
        try
        {
            thrower.throwCasB(1, 2, 3);
            test(false);
        }
        catch(C ex)
        {
            test(ex.aMem == 1);
            test(ex.bMem == 2);
            test(ex.cMem == 3);
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");
        
        if(thrower.supportsUndeclaredExceptions())
        {
            test(!collocated);
            
            Console.Write("catching unknown user exception... ");
            Console.Out.Flush();
            
            try
            {
                thrower.throwUndeclaredA(1);
                test(false);
            }
            catch(Ice.UnknownUserException)
            {
            }
            catch(Exception)
            {
                test(false);
            }
            
            try
            {
                thrower.throwUndeclaredB(1, 2);
                test(false);
            }
            catch(Ice.UnknownUserException)
            {
            }
            catch(Exception)
            {
                test(false);
            }
            
            try
            {
                thrower.throwUndeclaredC(1, 2, 3);
                test(false);
            }
            catch(Ice.UnknownUserException)
            {
            }
            catch(Exception)
            {
                test(false);
            }
            
            Console.WriteLine("ok");
        }
        
        Console.Write("catching object not exist exception... ");
        Console.Out.Flush();
        
        {
            Ice.Identity id = Ice.Util.stringToIdentity("does not exist");
            try
            {
                ThrowerPrx thrower2 = ThrowerPrxHelper.uncheckedCast(thrower.ice_newIdentity(id));
                thrower2.ice_ping();
                test(false);
            }
            catch(Ice.ObjectNotExistException ex)
            {
                test(ex.id.Equals(id));
            }
            catch(Exception)
            {
                test(false);
            }
        }
        
        Console.WriteLine("ok");
        
        Console.Write("catching facet not exist exception... ");
        Console.Out.Flush();
        
        try
        {
            ThrowerPrx thrower2 = ThrowerPrxHelper.uncheckedCast(thrower, "no such facet");
            try
            {
                thrower2.ice_ping();
                test(false);
            }
            catch(Ice.FacetNotExistException ex)
            {
                test(ex.facet.Count == 1);
                test(ex.facet[0].Equals("no such facet"));
            }
            
            ThrowerPrx thrower3 = ThrowerPrxHelper.uncheckedCast(thrower2, "no such facet either");
            try
            {
                thrower3.ice_ping();
                test(false);
            }
            catch(Ice.FacetNotExistException ex)
            {
                test(ex.facet.Count == 2);
                test(ex.facet[0].Equals("no such facet"));
                test(ex.facet[1].Equals("no such facet either"));
            }
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");
        
        Console.Write("catching operation not exist exception... ");
        Console.Out.Flush();
        
        try
        {
            WrongOperationPrx thrower2 = WrongOperationPrxHelper.uncheckedCast(thrower);
            thrower2.noSuchOperation();
            test(false);
        }
        catch(Ice.OperationNotExistException ex)
        {
            test(ex.operation.Equals("noSuchOperation"));
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");

        Console.Write("catching unknown local exception... ");
        Console.Out.Flush();
        
        try
        {
            thrower.throwLocalException();
            test(false);
        }
        catch(Ice.TimeoutException)
        {
            //
            // We get the original exception with collocation
            // optimization.
            //
            test(collocated);
        }
        catch(Ice.UnknownLocalException)
        {
            //
            // We get the an unknown local exception without
            // collocation optimization.
            //
            test(!collocated);
        }
        catch(Exception)
        {
            test(false);
        }
        
        Console.WriteLine("ok");

        Console.Write("catching unknown non-Ice exception... ");
        Console.Out.Flush();
        
        try
        {
            thrower.throwNonIceException();
            test(false);
        }
        catch(Ice.UnknownException)
        {
            //
            // We get the unknown exception without collocation
            // optimization.
            //
            test(!collocated);
        }
        catch(System.Exception)
        {
            //
            // We get the original exception with collocation
            // optimization.
            //
            test(collocated);
        }
        
        Console.WriteLine("ok");
        
/*
        if(!collocated)
        {
            Console.Write("catching exact types with AMI... ");
            Console.Out.Flush();
            
            {
                AMI_Thrower_throwAasAI cb = new AMI_Thrower_throwAasAI();
                thrower.throwAasA_async(cb, 1);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwAorDasAorDI cb = new AMI_Thrower_throwAorDasAorDI();
                thrower.throwAorDasAorD_async(cb, 1);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwAorDasAorDI cb = new AMI_Thrower_throwAorDasAorDI();
                thrower.throwAorDasAorD_async(cb, - 1);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwBasBI cb = new AMI_Thrower_throwBasBI();
                thrower.throwBasB_async(cb, 1, 2);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwCasCI cb = new AMI_Thrower_throwCasCI();
                thrower.throwCasC_async(cb, 1, 2, 3);
                test(cb.check());
            }
            
            Console.WriteLine("ok");
            
            Console.Write("catching derived types... ");
            Console.Out.Flush();
            
            {
                AMI_Thrower_throwBasAI cb = new AMI_Thrower_throwBasAI();
                thrower.throwBasA_async(cb, 1, 2);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwCasAI cb = new AMI_Thrower_throwCasAI();
                thrower.throwCasA_async(cb, 1, 2, 3);
                test(cb.check());
            }
            
            {
                AMI_Thrower_throwCasBI cb = new AMI_Thrower_throwCasBI();
                thrower.throwCasB_async(cb, 1, 2, 3);
                test(cb.check());
            }
            
            Console.WriteLine("ok");
            
            if(thrower.supportsUndeclaredExceptions())
            {
                Console.Write("catching unknown user exception with AMI... ");
                Console.Out.Flush();
                
                {
                    AMI_Thrower_throwUndeclaredAI cb = new AMI_Thrower_throwUndeclaredAI();
                    thrower.throwUndeclaredA_async(cb, 1);
                    test(cb.check());
                }
                
                {
                    AMI_Thrower_throwUndeclaredBI cb = new AMI_Thrower_throwUndeclaredBI();
                    thrower.throwUndeclaredB_async(cb, 1, 2);
                    test(cb.check());
                }
                
                {
                    AMI_Thrower_throwUndeclaredCI cb = new AMI_Thrower_throwUndeclaredCI();
                    thrower.throwUndeclaredC_async(cb, 1, 2, 3);
                    test(cb.check());
                }
                
                Console.WriteLine("ok");
            }
            
            Console.Write("catching object not exist exception with AMI... ");
            Console.Out.Flush();
            
            {
                Ice.Identity id = Ice.Util.stringToIdentity("does not exist");
                ThrowerPrx thrower2 = ThrowerPrxHelper.uncheckedCast(thrower.ice_newIdentity(id));
                AMI_Thrower_throwAasAObjectNotExistI cb = new AMI_Thrower_throwAasAObjectNotExistI();
                thrower2.throwAasA_async(cb, 1);
                test(cb.check());
            }
            
            Console.WriteLine("ok");
            
            Console.Write("catching facet not exist exception with AMI... ");
            Console.Out.Flush();
            
            try
            {
                ThrowerPrx thrower2 = ThrowerPrxHelper.uncheckedCast(thrower, "no such facet");
                {
                    AMI_Thrower_throwAasAFacetNotExistI cb = new AMI_Thrower_throwAasAFacetNotExistI();
                    thrower2.throwAasA_async(cb, 1);
                    test(cb.check());
                }
                
                ThrowerPrx thrower3 = ThrowerPrxHelper.uncheckedCast(thrower2, "no such facet either");
                {
                    AMI_Thrower_throwAasAFacetNotExist2I cb = new AMI_Thrower_throwAasAFacetNotExist2I();
                    thrower3.throwAasA_async(cb, 1);
                    test(cb.check());
                }
            }
            catch(Exception ex)
            {
                test(false);
            }
            
            Console.WriteLine("ok");
            
            Console.Write("catching operation not exist exception with AMI... ");
            Console.Out.Flush();
            
            {
                AMI_WrongOperation_noSuchOperationI cb = new AMI_WrongOperation_noSuchOperationI();
                WrongOperationPrx thrower2 = WrongOperationPrxHelper.uncheckedCast(thrower);
                thrower2.noSuchOperation_async(cb);
                test(cb.check());
            }
            
            Console.WriteLine("ok");
            
            Console.Write("catching unknown local exception with AMI... ");
            Console.Out.Flush();
            
            {
                AMI_Thrower_throwLocalExceptionI cb = new AMI_Thrower_throwLocalExceptionI();
                thrower.throwLocalException_async(cb);
                test(cb.check());
            }
            
            Console.WriteLine("ok");
            
            Console.Write("catching unknown non-Ice exception with AMI... ");
            Console.Out.Flush();
            
            AMI_Thrower_throwNonIceExceptionI cb2 = new AMI_Thrower_throwNonIceExceptionI();
            thrower.throwNonIceException_async(cb2);
            test(cb2.check());
            
            Console.WriteLine("ok");
        }
*/
        
        return thrower;
    }
}
