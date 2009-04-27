// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

public final class ObjectAdapterFactory
{
    public void
    shutdown()
    {
        java.util.List<Ice.ObjectAdapterI> adapters;
        synchronized(this)
        {
            //
            // Ignore shutdown requests if the object adapter factory has
            // already been shut down.
            //
            if(_instance == null)
            {
                return;
            }

            _instance = null;
            _communicator = null;

            adapters = _adapters;
        
            notifyAll();
        }

        //
        // Deactivate outside the thread synchronization, to avoid
        // deadlocks.
        //
        if(adapters != null)
        {
            for(Ice.ObjectAdapterI adapter : adapters)
            {
                adapter.deactivate();
            }
        }
    }

    public void
    waitForShutdown()
    {
        java.util.List<Ice.ObjectAdapterI> adapters;
        synchronized(this)
        {
            //
            // First we wait for the shutdown of the factory itself.
            //
            while(_instance != null)
            {
                try
                {
                    wait();
                }
                catch(InterruptedException ex)
                {
                }
            }
            
            //
            // If some other thread is currently shutting down, we wait
            // until this thread is finished.
            //
            while(_waitForShutdown)
            {
                try
                {
                    wait();
                }
                catch(InterruptedException ex)
                {
                }
            }
            _waitForShutdown = true;
            adapters = _adapters;
        }
        
        //
        // Now we wait for deactivation of each object adapter.
        //
        if(adapters != null)
        {
            for(Ice.ObjectAdapterI adapter : adapters)
            {
                adapter.waitForDeactivate();
            }
        }

        synchronized(this)
        {
            //
            // Signal that waiting is complete.
            //
            _waitForShutdown = false;
            notifyAll();
        }
    }

    public synchronized boolean
    isShutdown()
    {
        return _instance == null;
    }

    public void
    destroy()
    {
        //
        // First wait for shutdown to finish.
        //
        waitForShutdown();

        java.util.List<Ice.ObjectAdapterI> adapters;
        synchronized(this)
        {
            adapters = _adapters;

            //
            // For consistency with C#, we set _adapters to null
            // so that our finalizer does not try to invoke any
            // methods on member objects.
            //
            _adapters = null;
        }

        //
        // Now we destroy each object adapter.
        //
        if(adapters != null)
        {
            for(Ice.ObjectAdapterI adapter : adapters)
            {
                adapter.destroy();
            }
        }
    }
    
    public synchronized Ice.ObjectAdapter
    createObjectAdapter(String name, Ice.RouterPrx router)
    {
        if(_instance == null)
        {
            throw new Ice.ObjectAdapterDeactivatedException();
        }

        Ice.ObjectAdapterI adapter = null;
        if(name.length() == 0)
        {
            String uuid = java.util.UUID.randomUUID().toString();
            adapter = new Ice.ObjectAdapterI(_instance, _communicator, this, uuid, null, true);
        }
        else
        {
            if(_adapterNamesInUse.contains(name))
            {
                throw new Ice.AlreadyRegisteredException("object adapter", name);
            }
            _adapterNamesInUse.add(name);
            adapter = new Ice.ObjectAdapterI(_instance, _communicator, this, name, router, false);
        }
        _adapters.add(adapter);
        return adapter;
    }

    public Ice.ObjectAdapter
    findObjectAdapter(Ice.ObjectPrx proxy)
    {
        java.util.List<Ice.ObjectAdapterI> adapters;
        synchronized(this)
        {
            if(_instance == null)
            {
                return null;
            }

            adapters = _adapters;
        }

        for(Ice.ObjectAdapterI adapter : adapters)
        {
            try
            {
                if(adapter.isLocal(proxy))
                {
                    return adapter;
                }
            }
            catch(Ice.ObjectAdapterDeactivatedException ex)
            {
                // Ignore.
            }
        }

        return null;
    }

    public synchronized void
    removeObjectAdapter(Ice.ObjectAdapter adapter)
    {
        if(_instance == null)
        {
            return;
        }

        _adapters.remove(adapter);
        _adapterNamesInUse.remove(adapter.getName());
    }

    public void
    flushBatchRequests()
    {
        java.util.List<Ice.ObjectAdapterI> adapters;
        synchronized(this)
        {
            if(_adapters == null)
            {
                return;
            }

            adapters = _adapters;
        }

        for(Ice.ObjectAdapterI adapter : adapters)
        {
            adapter.flushBatchRequests();
        }
    }

    //
    // Only for use by Instance.
    //
    ObjectAdapterFactory(Instance instance, Ice.Communicator communicator)
    {
        _instance = instance;
        _communicator = communicator;
        _waitForShutdown = false;
    }

    protected synchronized void
    finalize()
        throws Throwable
    {
        IceUtilInternal.Assert.FinalizerAssert(_instance == null);
        IceUtilInternal.Assert.FinalizerAssert(_communicator == null);
        IceUtilInternal.Assert.FinalizerAssert(_adapters == null);
        IceUtilInternal.Assert.FinalizerAssert(!_waitForShutdown);

        super.finalize();
    }

    private Instance _instance;
    private Ice.Communicator _communicator;
    private java.util.Set<String> _adapterNamesInUse = new java.util.HashSet<String>();
    private java.util.List<Ice.ObjectAdapterI> _adapters = new java.util.LinkedList<Ice.ObjectAdapterI>();
    private boolean _waitForShutdown;
}
