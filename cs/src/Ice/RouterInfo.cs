// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************


namespace IceInternal
{

    using System.Collections;
    using System.Diagnostics;

    public sealed class RouterInfo
    {

	internal RouterInfo(Ice.RouterPrx router)
	{
	    _router = router;
	    _routingTable = new Ice.RoutingTable();
	    
	    Debug.Assert(_router != null);
	}
	
	public void destroy()
	{
	    lock(this)
	    {
		_clientProxy = null;
		_serverProxy = null;
		_adapter = null;
		_routingTable.clear();
	    }
	}
	
	public override bool Equals(System.Object obj)
	{
	    RouterInfo rhs = obj as RouterInfo;
	    return rhs == null ? false : _router.Equals(rhs._router);
	}

	public override int GetHashCode()
	{
	    return _router.GetHashCode();
	}
	
	public Ice.RouterPrx getRouter()
	{
	    //
	    // No mutex lock necessary, _router is immutable.
	    //
	    return _router;
	}

	public Ice.ObjectPrx getClientProxy()
	{
	    lock(this)
	    {
		if(_clientProxy == null)
		// Lazy initialization.
		{
		    _clientProxy = _router.getClientProxy();
		    if(_clientProxy == null)
		    {
			throw new Ice.NoEndpointException();
		    }
		    _clientProxy = _clientProxy.ice_router(null); // The client proxy cannot be routed.
		}
		
		return _clientProxy;
	    }
	}

	public void setClientProxy(Ice.ObjectPrx clientProxy)
	{
	    lock(this)
	    {
		_clientProxy = clientProxy.ice_router(null); // The client proxy cannot be routed.
	    }
	}

	public Ice.ObjectPrx getServerProxy()
	{
	    if(_serverProxy == null) // Lazy initialization.
	    {
		_serverProxy = _router.getServerProxy();
		if(_serverProxy == null)
		{
		    throw new Ice.NoEndpointException();
		}
		_serverProxy = _serverProxy.ice_router(null); // The server proxy cannot be routed.
	    }
	    
	    return _serverProxy;
	}

	public void setServerProxy(Ice.ObjectPrx serverProxy)
	{
	    _serverProxy = serverProxy.ice_router(null); // The server proxy cannot be routed.
	}

	public void addProxy(Ice.ObjectPrx proxy)
	{
	    //
	    // No mutex lock necessary, _routingTable is immutable, and
	    // RoutingTable is mutex protected.
	    //
	    if(_routingTable.add(proxy))
	    // Only add the proxy to the router if it's not already in the routing table.
	    {
		_router.addProxy(proxy);
	    }
	}

	public void setAdapter(Ice.ObjectAdapter adapter)
	{
	    lock(this)
	    {
		_adapter = adapter;
	    }
	}

	public Ice.ObjectAdapter getAdapter()
	{
	    lock(this)
	    {
		return _adapter;
	    }
	}
	
	private readonly Ice.RouterPrx _router;
	private Ice.ObjectPrx _clientProxy;
	private Ice.ObjectPrx _serverProxy;
	private readonly Ice.RoutingTable _routingTable;
	private Ice.ObjectAdapter _adapter;
    }

    public sealed class RouterManager
    {
        internal RouterManager()
        {
            _table = new Hashtable();
        }
	
        internal void destroy()
        {
            lock(this)
            {
                foreach(RouterInfo i in _table.Values)
                {
                    i.destroy();
                }
                _table.Clear();
            }
        }
	
        //
        // Returns router info for a given router. Automatically creates
        // the router info if it doesn't exist yet.
        //
        public RouterInfo get(Ice.RouterPrx rtr)
        {
            if(rtr == null)
            {
                return null;
            }
	    
            Ice.RouterPrx router = Ice.RouterPrxHelper.uncheckedCast(rtr.ice_router(null)); // The router cannot be routed.
	    
            lock(this)
            {
                RouterInfo info = (RouterInfo)_table[router];
                if(info == null)
                {
                    info = new RouterInfo(router);
                    _table[router] = info;
                }
		
                return info;
            }
        }
	
        private Hashtable _table;
    }

}
