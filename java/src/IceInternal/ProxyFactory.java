// **********************************************************************
//
// Copyright (c) 2003
// ZeroC, Inc.
// Billerica, MA, USA
//
// All Rights Reserved.
//
// Ice is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2 as published by
// the Free Software Foundation.
//
// **********************************************************************

package IceInternal;

public final class ProxyFactory
{
    public Ice.ObjectPrx
    stringToProxy(String str)
    {
        Reference ref = _instance.referenceFactory().create(str);
        return referenceToProxy(ref);
    }

    public String
    proxyToString(Ice.ObjectPrx proxy)
    {
        if(proxy != null)
        {
            Ice.ObjectPrxHelper h = (Ice.ObjectPrxHelper)proxy;
            return h.__reference().toString();
        }
        else
        {
            return "";
        }
    }

    public Ice.ObjectPrx
    streamToProxy(BasicStream s)
    {
        Ice.Identity ident = new Ice.Identity();
        ident.__read(s);

        Reference ref = _instance.referenceFactory().create(ident, s);
        return referenceToProxy(ref);
    }

    public Ice.ObjectPrx
    referenceToProxy(Reference ref)
    {
        if(ref != null)
        {
            Ice.ObjectPrxHelper proxy = new Ice.ObjectPrxHelper();
            proxy.setup(ref);
            return proxy;
        }
        else
        {
            return null;
        }
    }

    public void
    proxyToStream(Ice.ObjectPrx proxy, BasicStream s)
    {
        if(proxy != null)
        {
            Ice.ObjectPrxHelper h = (Ice.ObjectPrxHelper)proxy;
            Reference ref = h.__reference();
            ref.identity.__write(s);
            ref.streamWrite(s);
        }
        else
        {
            Ice.Identity ident = new Ice.Identity();
            ident.name = "";
            ident.category = "";
            ident.__write(s);
        }
    }

    public int
    checkRetryAfterException(Ice.LocalException ex, int cnt)
    {
	//
	// We don't retry *NotExistException, which are all derived from
	// RequestFailedException.
	//
	if(ex instanceof Ice.RequestFailedException)
	{
	    throw ex;
	}

	++cnt;

        TraceLevels traceLevels = _instance.traceLevels();
        Ice.Logger logger = _instance.logger();

        //
        // Instance components may be null if Communicator has been destroyed.
        //
        if(traceLevels != null && logger != null)
        {
            if(cnt > _retryIntervals.length)
            {
                if(traceLevels.retry >= 1)
                {
                    String s = "cannot retry operation call because retry limit has been exceeded\n" + ex.toString();
                    logger.trace(traceLevels.retryCat, s);
                }
                throw ex;
            }

            if(traceLevels.retry >= 1)
            {
                String s = "re-trying operation call";
                if(cnt > 0 && _retryIntervals[cnt - 1] > 0)
                {
                    s += " in " + _retryIntervals[cnt - 1] + "ms";
                }
                s += " because of exception\n" + ex;
                logger.trace(traceLevels.retryCat, s);
            }

            if(cnt > 0)
            {
                //
                // Sleep before retrying.
                //
                try
                {
                    Thread.currentThread().sleep(_retryIntervals[cnt - 1]);
                }
                catch(InterruptedException ex1)
                {
                }
            }

            return cnt;
        }
        else
        {
            //
            // Impossible to retry after Communicator has been destroyed.
            //
            throw ex;
        }
    }

    //
    // Only for use by Instance.
    //
    ProxyFactory(Instance instance)
    {
        _instance = instance;

	String str = _instance.properties().getPropertyWithDefault("Ice.RetryIntervals", "0");

        String[] arr = str.trim().split("[ \t\n\r]+");

	if(arr.length > 0)
	{
	    _retryIntervals = new int[arr.length];

	    for(int i = 0; i < arr.length; i++)
	    {
		int v;

		try
		{
		    v = Integer.parseInt(arr[i]);
		}
		catch(NumberFormatException ex)
		{
		    v = 0;
		}

		//
		// If -1 is the first value, no retry and wait intervals.
		// 
		if(i == 0 && v == -1)
		{
		    _retryIntervals = new int[0];
		    break;
		}

		_retryIntervals[i] = v > 0 ? v : 0;
	    }
	}
	else
	{
	    _retryIntervals = new int[1];
	    _retryIntervals[0] = 0;
	}
    }

    private Instance _instance;
    private int[] _retryIntervals;
}
