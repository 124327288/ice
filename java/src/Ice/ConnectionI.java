// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package Ice;

public final class ConnectionI extends IceInternal.EventHandler implements Connection
{
    public java.lang.Object
    clone()
        throws java.lang.CloneNotSupportedException
    {
        return super.clone();
    }

    public int
    ice_hash()
    {
        return hashCode();
    }

    public synchronized void
    validate()
    {
	assert(_state == StateNotValidated);

	if(!_endpoint.datagram()) // Datagram connections are always implicitly validated.
	{
	    try
	    {
		if(_adapter != null)
		{
		    synchronized(_sendMutex)
		    {
			//
			// Incoming connections play the active role
			// with respect to connection validation.
			//
			IceInternal.BasicStream os = new IceInternal.BasicStream(_instance);
			os.writeByte(IceInternal.Protocol.magic[0]);
			os.writeByte(IceInternal.Protocol.magic[1]);
			os.writeByte(IceInternal.Protocol.magic[2]);
			os.writeByte(IceInternal.Protocol.magic[3]);
			os.writeByte(IceInternal.Protocol.protocolMajor);
			os.writeByte(IceInternal.Protocol.protocolMinor);
			os.writeByte(IceInternal.Protocol.encodingMajor);
			os.writeByte(IceInternal.Protocol.encodingMinor);
			os.writeByte(IceInternal.Protocol.validateConnectionMsg);
			os.writeByte((byte)0); // Compression status (always zero for validate connection).
			os.writeInt(IceInternal.Protocol.headerSize); // Message size.
			IceInternal.TraceUtil.traceHeader("sending validate connection", 
							  os, _logger, _traceLevels);
			_transceiver.write(os, _endpoint.timeout());
		    }
		}
		else
		{
		    //
		    // Outgoing connections play the passive role with
		    // respect to connection validation.
		    //
		    IceInternal.BasicStream is = new IceInternal.BasicStream(_instance);
		    is.resize(IceInternal.Protocol.headerSize, true);
		    is.pos(0);
		    _transceiver.read(is, _endpoint.timeout());
		    int pos = is.pos();
		    assert(pos >= IceInternal.Protocol.headerSize);
		    is.pos(0);
		    byte[] m = new byte[4];
		    m[0] = is.readByte();
		    m[1] = is.readByte();
		    m[2] = is.readByte();
		    m[3] = is.readByte();
		    if(m[0] != IceInternal.Protocol.magic[0] || m[1] != IceInternal.Protocol.magic[1]
		       || m[2] != IceInternal.Protocol.magic[2] || m[3] != IceInternal.Protocol.magic[3])
		    {
		        BadMagicException ex = new BadMagicException();
			ex.badMagic = m;
			throw ex;
		    }
		    byte pMajor = is.readByte();
		    byte pMinor = is.readByte();

		    //
		    // We only check the major version number
		    // here. The minor version number is irrelevant --
		    // no matter what minor version number is offered
		    // by the server, we can be certain that the
		    // server supports at least minor version 0.  As
		    // the client, we are obliged to never produce a
		    // message with a minor version number that is
		    // larger than what the server can understand, but
		    // we don't care if the server understands more
		    // than we do.
		    //
		    // Note: Once we add minor versions, we need to
		    // modify the client side to never produce a
		    // message with a minor number that is greater
		    // than what the server can handle. Similarly, the
		    // server side will have to be modified so it
		    // never replies with a minor version that is
		    // greater than what the client can handle.
		    //
		    if(pMajor != IceInternal.Protocol.protocolMajor)
		    {
			UnsupportedProtocolException e = new UnsupportedProtocolException();
			e.badMajor = pMajor < 0 ? pMajor + 255 : pMajor;
			e.badMinor = pMinor < 0 ? pMinor + 255 : pMinor;
			e.major = IceInternal.Protocol.protocolMajor;
			e.minor = IceInternal.Protocol.protocolMinor;
			throw e;
		    }

		    byte eMajor = is.readByte();
		    byte eMinor = is.readByte();

		    //
		    // The same applies here as above -- only the
		    // major version number of the encoding is
		    // relevant.
		    //
		    if(eMajor != IceInternal.Protocol.encodingMajor)
		    {
			UnsupportedEncodingException e = new UnsupportedEncodingException();
			e.badMajor = eMajor < 0 ? eMajor + 255 : eMajor;
			e.badMinor = eMinor < 0 ? eMinor + 255 : eMinor;
			e.major = IceInternal.Protocol.encodingMajor;
			e.minor = IceInternal.Protocol.encodingMinor;
			throw e;
		    }

		    byte messageType = is.readByte();
		    if(messageType != IceInternal.Protocol.validateConnectionMsg)
		    {
			throw new ConnectionNotValidatedException();
		    }

                    byte compress = is.readByte(); // Ignore compression status for validate connection.

		    int size = is.readInt();
		    if(size != IceInternal.Protocol.headerSize)
		    {
			throw new IllegalMessageSizeException();
		    }
		    IceInternal.TraceUtil.traceHeader("received validate connection", is, 
						      _logger, _traceLevels);
		}
	    }
	    catch(LocalException ex)
	    {
		setState(StateClosed, ex);
		assert(_exception != null);
		throw _exception;
	    }
	}
	
	if(_acmTimeout > 0)
	{
	    _acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
	}

	//
	// We start out in holding state.
	//
	setState(StateHolding);
    }

    public synchronized void
    activate()
    {
	setState(StateActive);
    }

    public synchronized void
    hold()
    {
	setState(StateHolding);
    }

    // DestructionReason.
    public final static int ObjectAdapterDeactivated = 0;
    public final static int CommunicatorDestroyed = 1;

    public synchronized void
    destroy(int reason)
    {
	switch(reason)
	{
	    case ObjectAdapterDeactivated:
	    {
		setState(StateClosing, new ObjectAdapterDeactivatedException());
		break;
	    }
	    
	    case CommunicatorDestroyed:
	    {
		setState(StateClosing, new CommunicatorDestroyedException());
		break;
	    }
	}
    }

    public synchronized boolean
    isValidated()
    {
        return _state > StateNotValidated;
    }

    public synchronized boolean
    isDestroyed()
    {
	return _state >= StateClosing;
    }

    public synchronized boolean
    isFinished()
    {
	if(_transceiver == null && _dispatchCount == 0)
	{
	    //
	    // We must destroy the incoming cache. It is now not
	    // needed anymore.
	    //
	    synchronized(_incomingCacheMutex)
	    {
		while(_incomingCache != null)
		{
		    _incomingCache.__destroy();
		    _incomingCache = _incomingCache.next;
		}
	    }

	    return true;
	}
	else
	{
	    return false;
	}
    }

    public synchronized void
    waitUntilHolding()
    {
	while(_state < StateHolding || _dispatchCount > 0)
	{
	    try
	    {
		wait();
	    }
	    catch(InterruptedException ex)
	    {
	    }
        }
    }

    public synchronized void
    waitUntilFinished()
    {
	//
	// We wait indefinitely until connection closing has been
	// initiated. We also wait indefinitely until all outstanding
	// requests are completed. Otherwise we couldn't guarantee
	// that there are no outstanding calls when deactivate() is
	// called on the servant locators.
	//
	while(_state < StateClosing || _dispatchCount > 0)
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
	// Now we must wait until close() has been called on the
	// transceiver.
	//
	while(_transceiver != null)
	{
	    try
	    {
		if(_state != StateClosed && _endpoint.timeout() >= 0)
		{
		    long absoluteWaitTime = _stateTime + _endpoint.timeout();
		    long waitTime = absoluteWaitTime - System.currentTimeMillis();
		    
		    if(waitTime > 0)
		    {
			//
			// We must wait a bit longer until we close
			// this connection.
			//
			wait(waitTime);
			if(System.currentTimeMillis() >= absoluteWaitTime)
			{
			    setState(StateClosed, new CloseTimeoutException());
			}
		    }
		    else
		    {
			//
			// We already waited long enough, so let's
			// close this connection!
			//
			setState(StateClosed, new CloseTimeoutException());
		    }

		    //
		    // No return here, we must still wait until
		    // close() is called on the _transceiver.
		    //
		}
		else
		{
		    wait();
		}
	    }
	    catch(InterruptedException ex)
	    {
	    }
	}

	assert(_state == StateClosed);

	//
	// We must destroy the incoming cache. It is now not
	// needed anymore.
	//
	synchronized(_incomingCacheMutex)
	{
	    while(_incomingCache != null)
	    {
		_incomingCache.__destroy();
		_incomingCache = _incomingCache.next;
	    }
	}
    }

    public synchronized void
    monitor()
    {
	if(_state != StateActive)
	{
	    return;
	}
	
	//
	// Check for timed out async requests.
	//
	java.util.Iterator i = _asyncRequests.entryIterator();
	while(i.hasNext())
	{
	    IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
	    IceInternal.OutgoingAsync out = (IceInternal.OutgoingAsync)e.getValue();
	    if(out.__timedOut())
	    {
		setState(StateClosed, new TimeoutException());
		return;
	    }
	}

	//
	// Active connection management for idle connections.
	//
	if(_acmTimeout > 0 &&
	   _requests.isEmpty() && _asyncRequests.isEmpty() &&
	   !_batchStreamInUse && _batchStream.isEmpty() &&
	   _dispatchCount == 0)
	{
	    if(System.currentTimeMillis() >= _acmAbsoluteTimeoutMillis)
	    {
		setState(StateClosing, new ConnectionTimeoutException());
		return;
	    }
	}	    
    }

    private final static byte[] _requestHdr =
    {
	IceInternal.Protocol.magic[0],
	IceInternal.Protocol.magic[1],
	IceInternal.Protocol.magic[2],
	IceInternal.Protocol.magic[3],
        IceInternal.Protocol.protocolMajor,
        IceInternal.Protocol.protocolMinor,
        IceInternal.Protocol.encodingMajor,
        IceInternal.Protocol.encodingMinor,
        IceInternal.Protocol.requestMsg,
        (byte)0, // Compression status.
        (byte)0, (byte)0, (byte)0, (byte)0, // Message size (placeholder).
        (byte)0, (byte)0, (byte)0, (byte)0  // Request ID (placeholder).
    };

    //
    // TODO: Should not be a member function of ConnectionI.
    //
    public void
    prepareRequest(IceInternal.BasicStream os)
    {
        os.writeBlob(_requestHdr);
    }

    public void
    sendRequest(IceInternal.BasicStream os, IceInternal.Outgoing out)
    {
	int requestId = 0;

	synchronized(this)
	{
	    assert(!(out != null && _endpoint.datagram())); // Twoway requests cannot be datagrams.

	    if(_exception != null)
	    {
		throw _exception;
	    }

	    assert(_state > StateNotValidated);
	    assert(_state < StateClosing);
	
	    //
	    // Fill in the message size.
	    //
	    os.pos(10);
	    os.writeInt(os.size());

	    //
	    // Only add to the request map if this is a twoway call.
	    //
	    if(out != null)
	    {
		//
		// Create a new unique request ID.
		//
		requestId = _nextRequestId++;
		if(requestId <= 0)
		{
		    _nextRequestId = 1;
		    requestId = _nextRequestId++;
		}

		//
		// Fill in the request ID.
		//
		os.pos(IceInternal.Protocol.headerSize);
		os.writeInt(requestId);

		//
		// Add to the requests map.
		//
		_requests.put(requestId, out);
	    }

	    if(_acmTimeout > 0)
	    {
		_acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
	    }
	}

	try
	{
	    synchronized(_sendMutex)
	    {
		if(_transceiver == null) // Has the transceiver already been closed?
		{
		    assert(_exception != null);
		    throw _exception; // The exception is immutable at this point.
		}
		
		//
		// Send the request.
		//
		IceInternal.TraceUtil.traceRequest("sending request", os, _logger, _traceLevels);
		_transceiver.write(os, _endpoint.timeout());
	    }
	}
	catch(LocalException ex)
	{
	    synchronized(this)
	    {
		setState(StateClosed, ex);
		assert(_exception != null);
		
		if(out != null)
		{
		    //
		    // If the request has already been removed from
		    // the request map, we are out of luck. It would
		    // mean that finished() has been called already,
		    // and therefore the exception has been set using
		    // the Outgoing::finished() callback. In this
		    // case, we cannot throw the exception here,
		    // because we must not both raise an exception and
		    // have Outgoing::finished() called with an
		    // exception. This means that in some rare cases,
		    // a request will not be retried even though it
		    // could. But I honestly don't know how I could
		    // avoid this, without a very elaborate and
		    // complex design, which would be bad for
		    // performance.
		    //
		    IceInternal.Outgoing o = (IceInternal.Outgoing)_requests.remove(requestId);
		    if(o != null)
		    {
			assert(o == out);
			throw _exception;
		    }
		}
		else
		{
		    throw _exception;
		}
	    }
	}
    }

    public void
    sendAsyncRequest(IceInternal.BasicStream os, IceInternal.OutgoingAsync out)
    {
	int requestId = 0;

	synchronized(this)
	{
	    assert(!_endpoint.datagram()); // Twoway requests cannot be datagrams, and async implies twoway.

	    if(_exception != null)
	    {
		throw _exception;
	    }

	    assert(_state > StateNotValidated);
	    assert(_state < StateClosing);
	
	    //
	    // Fill in the message size.
	    //
	    os.pos(10);
	    os.writeInt(os.size());

	    //
	    // Create a new unique request ID.
	    //
	    requestId = _nextRequestId++;
	    if(requestId <= 0)
	    {
		_nextRequestId = 1;
		requestId = _nextRequestId++;
	    }
	    
	    //
	    // Fill in the request ID.
	    //
	    os.pos(IceInternal.Protocol.headerSize);
	    os.writeInt(requestId);
	    
	    //
	    // Add to the requests map.
	    //
	    _asyncRequests.put(requestId, out);

	    if(_acmTimeout > 0)
	    {
		_acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
	    }
	}

	try
	{
	    synchronized(_sendMutex)
	    {
		if(_transceiver == null) // Has the transceiver already been closed?
		{
		    assert(_exception != null);
		    throw _exception; // The exception is immutable at this point.
		}
		
		//
		// Send the request.
		//
		IceInternal.TraceUtil.traceRequest("sending asynchronous request", os, 
						   _logger, _traceLevels);
		_transceiver.write(os, _endpoint.timeout());
	    }
	}
	catch(LocalException ex)
	{
	    synchronized(this)
	    {
		setState(StateClosed, ex);
		assert(_exception != null);
		
		//
		// If the request has already been removed from the
		// async request map, we are out of luck. It would
		// mean that finished() has been called already, and
		// therefore the exception has been set using the
		// OutgoingAsync::__finished() callback. In this case,
		// we cannot throw the exception here, because we must
		// not both raise an exception and have
		// OutgoingAsync::__finished() called with an
		// exception. This means that in some rare cases, a
		// request will not be retried even though it
		// could. But I honestly don't know how I could avoid
		// this, without a very elaborate and complex design,
		// which would be bad for performance.
		//
		IceInternal.OutgoingAsync o = (IceInternal.OutgoingAsync)_asyncRequests.remove(requestId);
		if(o != null)
		{
		    assert(o == out);
		    throw _exception;
		}
	    }
	}
    }

    private final static byte[] _requestBatchHdr =
    {
	IceInternal.Protocol.magic[0],
	IceInternal.Protocol.magic[1],
	IceInternal.Protocol.magic[2],
	IceInternal.Protocol.magic[3],
        IceInternal.Protocol.protocolMajor,
        IceInternal.Protocol.protocolMinor,
        IceInternal.Protocol.encodingMajor,
        IceInternal.Protocol.encodingMinor,
        IceInternal.Protocol.requestBatchMsg,
        0, // Compression status.
        (byte)0, (byte)0, (byte)0, (byte)0, // Message size (placeholder).
        (byte)0, (byte)0, (byte)0, (byte)0  // Number of requests in batch (placeholder).
    };

    public synchronized void
    prepareBatchRequest(IceInternal.BasicStream os)
    {
	while(_batchStreamInUse && _exception == null)
	{
	    try
	    {
		wait();
	    }
	    catch(InterruptedException ex)
	    {
	    }
	}

        if(_exception != null)
        {
            throw _exception;
        }

        assert(_state > StateNotValidated);
	assert(_state < StateClosing);

        if(_batchStream.isEmpty())
        {
	    try
	    {
		_batchStream.writeBlob(_requestBatchHdr);
	    }
	    catch(LocalException ex)
	    {
		setState(StateClosed, ex);
		throw ex;
	    }
        }

        _batchStreamInUse = true;
	_batchStream.swap(os);

	//
	// _batchStream now belongs to the caller, until
	// finishBatchRequest() is called.
	//
    }

    public synchronized void
    finishBatchRequest(IceInternal.BasicStream os)
    {
        if(_exception != null)
        {
            throw _exception;
        }

        assert(_state > StateNotValidated);
	assert(_state < StateClosing);

        _batchStream.swap(os); // Get the batch stream back.
	++_batchRequestNum; // Increment the number of requests in the batch.

	//
	// Give the ConnectionI back.
	//
	assert(_batchStreamInUse);
        _batchStreamInUse = false;
	notifyAll();
    }

    public void
    flushBatchRequests()
    {
	synchronized(this)
	{
	    while(_batchStreamInUse && _exception == null)
	    {
		try
		{
		    wait();
		}
		catch(InterruptedException ex)
		{
		}
	    }
	    
	    if(_exception != null)
	    {
		throw _exception;
	    }

	    if(_batchStream.isEmpty())
	    {
		return; // Nothing to do.
	    }

	    assert(_state > StateNotValidated);
	    assert(_state < StateClosing);
	    
	    //
	    // Fill in the message size.
	    //
	    _batchStream.pos(10);
	    _batchStream.writeInt(_batchStream.size());
	    
	    //
	    // Fill in the number of requests in the batch.
	    //
	    _batchStream.writeInt(_batchRequestNum);

	    if(_acmTimeout > 0)
	    {
		_acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
	    }

	    //
	    // Prevent that new batch requests are added while we are
	    // flushing.
	    //
	    _batchStreamInUse = true;
	}

	try
	{
	    synchronized(_sendMutex)
	    {
		if(_transceiver == null) // Has the transceiver already been closed?
		{
		    assert(_exception != null);
		    throw _exception; // The exception is immutable at this point.
		}
		
		//
		// Send the batch request.
		//
		IceInternal.TraceUtil.traceBatchRequest("sending batch request", _batchStream, 
							_logger, _traceLevels);
		_transceiver.write(_batchStream, _endpoint.timeout());
	    }
	}
	catch(LocalException ex)
	{
	    synchronized(this)
	    {
		setState(StateClosed, ex);
		assert(_exception != null);
		
		//
		// Since batch requests are all oneways (or datagrams), we
		// must report the exception to the caller.
		//
		throw _exception;
	    }
	}

	synchronized(this)
	{
	    //
	    // Reset the batch stream, and notify that flushing is over.
	    //
	    _batchStream.destroy();
	    _batchStream = new IceInternal.BasicStream(_instance);
	    _batchRequestNum = 0;
	    _batchStreamInUse = false;
	    notifyAll();
	}
    }

    public void
    sendResponse(IceInternal.BasicStream os, byte compress)
    {
	try
	{
	    synchronized(_sendMutex)
	    {
		if(_transceiver == null) // Has the transceiver already been closed?
		{
		    assert(_exception != null);
		    throw _exception; // The exception is immutable at this point.
		}

		//
		// Fill in the message size.
		//
		os.pos(10);
		os.writeInt(os.size());
		
		//
		// Send the reply.
		//
		IceInternal.TraceUtil.traceReply("sending reply", os, _logger, _traceLevels);
		_transceiver.write(os, _endpoint.timeout());
	    }
	}
	catch(LocalException ex)
	{
	    synchronized(this)
	    {
		setState(StateClosed, ex);
	    }
	}

	synchronized(this)
	{
	    assert(_state > StateNotValidated);

	    try
	    {
		if(--_dispatchCount == 0)
		{
		    notifyAll();
		}
		
		if(_state == StateClosing && _dispatchCount == 0)
		{
		    initiateShutdown();
		}

		if(_acmTimeout > 0)
		{
		    _acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
		}
	    }
	    catch(LocalException ex)
	    {
		setState(StateClosed, ex);
	    }
	}
    }

    public synchronized void
    sendNoResponse()
    {
	assert(_state > StateNotValidated);
	
	try
	{
	    if(--_dispatchCount == 0)
	    {
		notifyAll();
	    }
	    
	    if(_state == StateClosing && _dispatchCount == 0)
	    {
		initiateShutdown();
	    }
	}
	catch(LocalException ex)
	{
	    setState(StateClosed, ex);
	}
    }

    public IceInternal.Endpoint
    endpoint()
    {
        // No mutex protection necessary, _endpoint is immutable.
        return _endpoint;
    }

    public synchronized void
    setAdapter(ObjectAdapter adapter)
    {
	//
	// Before we set an adapter (or reset it) we wait until the
	// dispatch count with any old adapter is zero.
        //
        // A deadlock can occur if we wait while an operation is
        // outstanding on this adapter that holds a lock while
        // calling this function (e.g., __getDelegate).
        //
        // In order to avoid such a deadlock, we only wait if the new
        // adapter is different than the current one.
        //
        // TODO: Verify that this fix solves all cases.
	//
        if(_adapter != adapter)
        {
            while(_dispatchCount > 0)
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
            // We never change the thread pool with which we were
            // initially registered, even if we add or remove an object
            // adapter.
            //
            
            _adapter = adapter;
            if(_adapter != null)
            {
                _servantManager = ((ObjectAdapterI)_adapter).getServantManager();
            }
            else
            {
                _servantManager = null;
            }
        }
    }

    public synchronized ObjectAdapter
    getAdapter()
    {
	return _adapter;
    }

    public synchronized ObjectPrx
    createProxy(Identity ident)
    {
        //
        // Create a reference and return a reverse proxy for this
        // reference.
        //
        IceInternal.Endpoint[] endpoints = new IceInternal.Endpoint[0];
        ConnectionI[] connections = new ConnectionI[1];
        connections[0] = this;
        IceInternal.Reference ref = _instance.referenceFactory().create(ident, new java.util.HashMap(), "",
                                                                        IceInternal.Reference.ModeTwoway, false, "",
                                                                        endpoints, null, null, connections, true);
        return _instance.proxyFactory().referenceToProxy(ref);
    }

    //
    // Operations from EventHandler
    //

    public boolean
    datagram()
    {
       return _endpoint.datagram();
    }

    public boolean
    readable()
    {
        return true;
    }

    public void
    read(IceInternal.BasicStream stream)
    {
	_transceiver.read(stream, 0);

	//
	// Updating _acmAbsoluteTimeoutMillis is to expensive here,
	// because we would have to acquire a lock just for this
	// purpose. Instead, we update _acmAbsoluteTimeoutMillis in
	// message().
	//
    }

    private final static byte[] _replyHdr =
    {
	IceInternal.Protocol.magic[0],
	IceInternal.Protocol.magic[1],
	IceInternal.Protocol.magic[2],
	IceInternal.Protocol.magic[3],
        IceInternal.Protocol.protocolMajor,
        IceInternal.Protocol.protocolMinor,
        IceInternal.Protocol.encodingMajor,
        IceInternal.Protocol.encodingMinor,
        IceInternal.Protocol.replyMsg,
        (byte)0, // Compression status.
        (byte)0, (byte)0, (byte)0, (byte)0 // Message size (placeholder).
    };

    public void
    message(IceInternal.BasicStream stream, IceInternal.ThreadPool threadPool)
    {
	byte compress = 0;
	int requestId = 0;
	int invokeNum = 0;
	IceInternal.ServantManager servantManager = null;
	ObjectAdapter adapter = null;
	IceInternal.OutgoingAsync outAsync = null;

        synchronized(this)
        {
	    //
	    // We must promote within the synchronization, otherwise
	    // there could be various race conditions with close
	    // connection messages and other messages.
	    //
	    threadPool.promoteFollower();

	    assert(_state > StateNotValidated);

            if(_state == StateClosed)
            {
		return;
            }

	    if(_acmTimeout > 0)
	    {
		_acmAbsoluteTimeoutMillis = System.currentTimeMillis() + _acmTimeout * 1000;
	    }

	    try
	    {
		//
		// We don't need to check magic and version here. This
		// has already been done by the ThreadPool, which
		// provides us the stream.
		//
		assert(stream.pos() == stream.size());
		stream.pos(8);
		byte messageType = stream.readByte();
		compress = stream.readByte();
		if(compress == (byte)2)
		{
		    throw new CompressionNotSupportedException();
		}
		stream.pos(IceInternal.Protocol.headerSize);

                switch(messageType)
                {
		    case IceInternal.Protocol.closeConnectionMsg:
		    {
			IceInternal.TraceUtil.traceHeader("received close connection", stream, 
							  _logger, _traceLevels);
			if(_endpoint.datagram() && _warn)
			{
			    _logger.warning("ignoring close connection message for datagram connection:\n" + _desc);
			}
			else
			{
			    setState(StateClosed, new CloseConnectionException());
			}
			break;
		    }

                    case IceInternal.Protocol.requestMsg:
                    {
                        if(_state == StateClosing)
                        {
                            IceInternal.TraceUtil.traceRequest("received request during closing\n" +
                                                   "(ignored by server, client will retry)",
                                                   stream, _logger, _traceLevels);
                        }
                        else
                        {
                            IceInternal.TraceUtil.traceRequest("received request", stream, 
							       _logger, _traceLevels);
			    requestId = stream.readInt();
			    invokeNum = 1;
			    servantManager = _servantManager;
			    adapter = _adapter;
			    ++_dispatchCount;
                        }
                        break;
                    }

                    case IceInternal.Protocol.requestBatchMsg:
                    {
                        if(_state == StateClosing)
                        {
                            IceInternal.TraceUtil.traceBatchRequest
				("received batch request during closing\n" +
				 "(ignored by server, client will retry)",
				 stream, _logger, _traceLevels);
                        }
                        else
                        {
                            IceInternal.TraceUtil.traceBatchRequest("received batch request",
								    stream, _logger, _traceLevels);
			    invokeNum = stream.readInt();
			    if(invokeNum < 0)
			    {
				throw new NegativeSizeException();
			    }
			    servantManager = _servantManager;
			    adapter = _adapter;
			    _dispatchCount += invokeNum;
                        }
                        break;
                    }

                    case IceInternal.Protocol.replyMsg:
                    {
                        IceInternal.TraceUtil.traceReply("received reply", stream, _logger, _traceLevels);
                        requestId = stream.readInt();
                        IceInternal.Outgoing out = (IceInternal.Outgoing)_requests.remove(requestId);
                        if(out != null)
                        {
			    out.finished(stream);
			}
			else
			{
			    outAsync = (IceInternal.OutgoingAsync)_asyncRequests.remove(requestId);
			    if(outAsync == null)
			    {
				throw new UnknownRequestIdException();
			    }
                        }
                        break;
                    }

                    case IceInternal.Protocol.validateConnectionMsg:
                    {
                        IceInternal.TraceUtil.traceHeader("received validate connection", stream, 
							  _logger, _traceLevels);
			if(_warn)
			{
			    _logger.warning("ignoring unexpected validate connection message:\n" + _desc);
			}
                        break;
                    }

                    default:
                    {
                        IceInternal.TraceUtil.traceHeader("received unknown message\n" +
                                              "(invalid, closing connection)",
                                              stream, _logger, _traceLevels);
                        throw new UnknownMessageException();
                    }
                }
            }
            catch(LocalException ex)
            {
                setState(StateClosed, ex);
                return;
            }
        }

	//
	// Asynchronous replies must be handled outside the thread
	// synchronization, so that nested calls are possible.
	//
	if(outAsync != null)
	{
	    outAsync.__finished(stream);
	}

	//
	// Method invocation (or multiple invocations for batch messages)
	// must be done outside the thread synchronization, so that nested
	// calls are possible.
	//
	IceInternal.Incoming in = null;
	try
	{
	    while(invokeNum > 0)
	    {
		
		//
		// Prepare the invocation.
		//
		boolean response = !_endpoint.datagram() && requestId != 0;
		in = getIncoming(adapter, response, compress);
		IceInternal.BasicStream is = in.is();
		stream.swap(is);
		IceInternal.BasicStream os = in.os();
		
		//
		// Prepare the response if necessary.
		//
		if(response)
		{
		    assert(invokeNum == 1); // No further invocations if a response is expected.
		    os.writeBlob(_replyHdr);
		    
		    //
		    // Fill in the request ID.
		    //
		    os.writeInt(requestId);
		}
		
		in.invoke(servantManager);
		
		//
		// If there are more invocations, we need the stream back.
		//
		if(--invokeNum > 0)
		{
		    stream.swap(is);
                }

		reclaimIncoming(in);
		in = null;
	    }
	}
	catch(LocalException ex)
	{
	    assert(invokeNum > 0);

	    synchronized(this)
	    {
		setState(StateClosed, ex);
	    }
	}
	catch(AssertionError ex)
	{
	    assert(invokeNum > 0);

	    //
	    // Java only: Upon an assertion, we don't kill the whole
	    // process, but just print the stack trace and close the
	    // connection.
	    //
	    synchronized(this)
	    {
		java.io.StringWriter sw = new java.io.StringWriter();
		java.io.PrintWriter pw = new java.io.PrintWriter(sw);
		ex.printStackTrace(pw);
		pw.flush();
		UnknownException exc = new UnknownException();
		exc.unknown = sw.toString();
		_logger.error(exc.unknown);
		setState(StateClosed, exc);
	    }
	}
	finally
	{
	    if(in != null)
	    {
		reclaimIncoming(in);
	    }

	    //
	    // If invoke() above raised an exception, and therefore
	    // neither sendResponse() nor sendNoResponse() has been
	    // called, then we must decrement _dispatchCount here.
	    //
	    if(invokeNum > 0)
	    {
		synchronized(this)
		{
		    assert(_dispatchCount > 0);
		    _dispatchCount -= invokeNum;
		    assert(_dispatchCount >= 0);
		    if(_dispatchCount == 0)
		    {
			notifyAll();
		    }
		}
	    }
	}
    }

    public void
    finished(IceInternal.ThreadPool threadPool)
    {
	threadPool.promoteFollower();

	LocalException exception = null;

	IceInternal.IntMap requests = null;
	IceInternal.IntMap asyncRequests = null;
        IceInternal.Incoming in = null;

	synchronized(this)
	{
	    if(_state == StateActive || _state == StateClosing)
	    {
		registerWithPool();
	    }
	    else if(_state == StateClosed)
	    {
		//
		// We must make sure that nobody is sending when we
		// close the transceiver.
		//
		synchronized(_sendMutex)
		{
		    try
		    {
			_transceiver.close();
		    }
		    catch(LocalException ex)
		    {
			exception = ex;
		    }
		    
		    _transceiver = null;
		    notifyAll();
		}
	    }

	    if(_state == StateClosed || _state == StateClosing)
	    {
		requests = _requests;
		_requests = new IceInternal.IntMap();

		asyncRequests = _asyncRequests;
		_asyncRequests = new IceInternal.IntMap();
	    }
	}

        while(in != null)
	{
            in.__destroy();
            in = in.next;
        }

	if(requests != null)
	{
	    java.util.Iterator i = requests.entryIterator();
	    while(i.hasNext())
	    {
		IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
		IceInternal.Outgoing out = (IceInternal.Outgoing)e.getValue();
		out.finished(_exception); // The exception is immutable at this point.
	    }
	}
	
	if(asyncRequests != null)
	{
	    java.util.Iterator i = asyncRequests.entryIterator();
	    while(i.hasNext())
	    {
		IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
		IceInternal.OutgoingAsync out = (IceInternal.OutgoingAsync)e.getValue();
		out.__finished(_exception); // The exception is immutable at this point.
	    }
	}

	if(exception != null)
	{
	    throw exception;
	}
    }

    public synchronized void
    exception(LocalException ex)
    {
	setState(StateClosed, ex);
    }

    public String
    type()
    {
	return _type; // No mutex lock, _type is immutable.
    }

    public int
    timeout()
    {
	return _endpoint.timeout(); // No mutex protection necessary, _endpoint is immutable.
    }

    public String
    toString()
    {
	return _toString();
    }

    public String
    _toString()
    {
	return _desc; // No mutex lock, _desc is immutable.
    }

    public ConnectionI(IceInternal.Instance instance, IceInternal.Transceiver transceiver, 
		       IceInternal.Endpoint endpoint, ObjectAdapter adapter)
    {
        super(instance);
        _transceiver = transceiver;
	_desc = transceiver.toString();
        _type = transceiver.type();
        _endpoint = endpoint;
        _adapter = adapter;
        _logger = instance.logger(); // Cached for better performance.
        _traceLevels = instance.traceLevels(); // Cached for better performance.
	_registeredWithPool = false;
	_warn = _instance.properties().getPropertyAsInt("Ice.Warn.Connections") > 0 ? true : false;
	_acmTimeout = _endpoint.datagram() ? 0 : _instance.connectionIdleTime();
	_acmAbsoluteTimeoutMillis = 0;
        _nextRequestId = 1;
        _batchStream = new IceInternal.BasicStream(instance);
	_batchStreamInUse = false;
	_batchRequestNum = 0;
        _dispatchCount = 0;
        _state = StateNotValidated;
	_stateTime = System.currentTimeMillis();

	if(_adapter != null)
	{
	    _threadPool = ((ObjectAdapterI)_adapter).getThreadPool();
	    _servantManager = ((ObjectAdapterI)_adapter).getServantManager();
	}
	else
	{
	    _threadPool = _instance.clientThreadPool();
	    _servantManager = null;
	}
    }

    protected void
    finalize()
        throws Throwable
    {
        assert(_state == StateClosed);
	assert(_transceiver == null);
	assert(_dispatchCount == 0);
        assert(_incomingCache == null);

        _batchStream.destroy();

        super.finalize();
    }

    private static final int StateNotValidated = 0;
    private static final int StateActive = 1;
    private static final int StateHolding = 2;
    private static final int StateClosing = 3;
    private static final int StateClosed = 4;

    private void
    setState(int state, LocalException ex)
    {
	//
	// If setState() is called with an exception, then only closed
	// and closing states are permissible.
	//
	assert(state == StateClosing || state == StateClosed);

        if(_state == state) // Don't switch twice.
        {
            return;
        }

        if(_exception == null)
        {
	    _exception = ex;

            if(_warn)
            {
		//
		// We don't warn if we are not validated.
		//
		if(_state > StateNotValidated)
		{
		    //
		    // Don't warn about certain expected exceptions.
		    //
		    if(!(_exception instanceof CloseConnectionException ||
			 _exception instanceof ConnectionTimeoutException ||
			 _exception instanceof CommunicatorDestroyedException ||
			 _exception instanceof ObjectAdapterDeactivatedException ||
			 (_exception instanceof ConnectionLostException && _state == StateClosing)))
		    {
			warning("connection exception", _exception);
		    }
		}
            }
        }

	//
	// We must set the new state before we notify requests of any
	// exceptions. Otherwise new requests may retry on a
	// connection that is not yet marked as closed or closing.
	//
        setState(state);
    }

    private void
    setState(int state)
    {
        //
        // We don't want to send close connection messages if the endpoint
        // only supports oneway transmission from client to server.
        //
        if(_endpoint.datagram() && state == StateClosing)
        {
            state = StateClosed;
        }

        if(_state == state) // Don't switch twice.
        {
            return;
        }

        switch(state)
        {
	    case StateNotValidated:
	    {
		assert(false);
		break;
	    }

            case StateActive:
            {
		//
		// Can only switch from holding or not validated to
		// active.
		//
                if(_state != StateHolding && _state != StateNotValidated)
                {
                    return;
                }
                registerWithPool();
                break;
            }

            case StateHolding:
            {
		//
		// Can only switch from active or not validated to
		// holding.
		//
		if(_state != StateActive && _state != StateNotValidated)
		{
                    return;
                }
                unregisterWithPool();
                break;
            }

            case StateClosing:
            {
		//
		// Can't change back from closed.
		//
                if(_state == StateClosed)
                {
                    return;
                }
		registerWithPool(); // We need to continue to read in closing state.
                break;
            }
	    
            case StateClosed:
            {
		//
		// If we change from not validated, we can close right
		// away. Otherwise we first must make sure that we are
		// registered, then we unregister, and let finished()
		// do the close.
		//
		if(_state == StateNotValidated)
		{
		    assert(!_registeredWithPool);

		    //
		    // We must make sure that nobody is sending when
		    // we close the transceiver.
		    //
		    synchronized(_sendMutex)
		    {
			try
			{
			    _transceiver.close();
			}
			catch(LocalException ex)
			{
			    // Here we ignore any exceptions in close().
			}

			_transceiver = null;
			//notifyAll(); // We notify already below.
		    }
		}
		else
		{
		    registerWithPool();
		    unregisterWithPool();
		}
		break;
            }
        }

        _state = state;
	_stateTime = System.currentTimeMillis();
	notifyAll();

        if(_state == StateClosing && _dispatchCount == 0)
        {
            try
            {
                initiateShutdown();
            }
            catch(LocalException ex)
            {
                setState(StateClosed, ex);
            }
        }
    }

    private void
    initiateShutdown()
    {
	assert(_state == StateClosing);
	assert(_dispatchCount == 0);

	if(!_endpoint.datagram())
	{
	    synchronized(_sendMutex)
	    {
		//
		// Before we shut down, we send a close connection
		// message.
		//
		IceInternal.BasicStream os = new IceInternal.BasicStream(_instance);
		os.writeByte(IceInternal.Protocol.magic[0]);
		os.writeByte(IceInternal.Protocol.magic[1]);
		os.writeByte(IceInternal.Protocol.magic[2]);
		os.writeByte(IceInternal.Protocol.magic[3]);
		os.writeByte(IceInternal.Protocol.protocolMajor);
		os.writeByte(IceInternal.Protocol.protocolMinor);
		os.writeByte(IceInternal.Protocol.encodingMajor);
		os.writeByte(IceInternal.Protocol.encodingMinor);
		os.writeByte(IceInternal.Protocol.closeConnectionMsg);
		os.writeByte((byte)0); // Compression status.
		os.writeInt(IceInternal.Protocol.headerSize); // Message size.
		
		//
		// Send the message.
		//
		IceInternal.TraceUtil.traceHeader("sending close connection", os, _logger, _traceLevels);
		_transceiver.write(os, _endpoint.timeout());
		_transceiver.shutdown();
	    }
	}
    }

    private void
    registerWithPool()
    {
	if(!_registeredWithPool)
	{
	    _threadPool._register(_transceiver.fd(), this);
	    _registeredWithPool = true;

	    IceInternal.ConnectionMonitor connectionMonitor = _instance.connectionMonitor();
	    if(connectionMonitor != null)
	    {
		connectionMonitor.add(this);
	    }
	}
    }

    private void
    unregisterWithPool()
    {
	if(_registeredWithPool)
	{
	    _threadPool.unregister(_transceiver.fd());
	    _registeredWithPool = false;	

	    IceInternal.ConnectionMonitor connectionMonitor = _instance.connectionMonitor();
	    if(connectionMonitor != null)
	    {
		connectionMonitor.remove(this);
	    }
	}
    }

    private void
    warning(String msg, Exception ex)
    {
        java.io.StringWriter sw = new java.io.StringWriter();
        java.io.PrintWriter pw = new java.io.PrintWriter(sw);
        Throwable t = ex;
        do
        {
            t.printStackTrace(pw);
            t = t.getCause();
            if(t != null)
            {
                pw.println("Caused by:\n");
            }
        }
        while(t != null);
        pw.flush();
        String s = msg + ":\n" + sw.toString() + _desc;
        _logger.warning(s);
    }

    private IceInternal.Incoming
    getIncoming(ObjectAdapter adapter, boolean response, byte compress)
    {
        IceInternal.Incoming in = null;

        synchronized(_incomingCacheMutex)
        {
            if(_incomingCache == null)
            {
                in = new IceInternal.Incoming(_instance, this, adapter, response, compress);
            }
            else
            {
                in = _incomingCache;
                _incomingCache = _incomingCache.next;
                in.next = null;
                in.reset(_instance, this, adapter, response, compress);
            }
        }

        return in;
    }

    private void
    reclaimIncoming(IceInternal.Incoming in)
    {
        synchronized(_incomingCacheMutex)
        {
            in.next = _incomingCache;
            _incomingCache = in;
        }
    }

    private IceInternal.Transceiver _transceiver;
    private final String _desc;
    private final String _type;
    private final IceInternal.Endpoint _endpoint;

    private ObjectAdapter _adapter;
    private IceInternal.ServantManager _servantManager;

    private final Logger _logger;
    private final IceInternal.TraceLevels _traceLevels;

    private boolean _registeredWithPool;
    private final IceInternal.ThreadPool _threadPool;

    private final boolean _warn;

    private final int _acmTimeout;
    private long _acmAbsoluteTimeoutMillis;

    private int _nextRequestId;
    private IceInternal.IntMap _requests = new IceInternal.IntMap();
    private IceInternal.IntMap _asyncRequests = new IceInternal.IntMap();

    private LocalException _exception;

    private IceInternal.BasicStream _batchStream;
    private boolean _batchStreamInUse;
    private int _batchRequestNum;

    private int _dispatchCount;

    private int _state; // The current state.
    private long _stateTime; // The last time when the state was changed.

    //
    // We have a separate mutex for sending, so that we don't block
    // the whole connection when we do a blocking send.
    //
    private java.lang.Object _sendMutex = new java.lang.Object();

    private IceInternal.Incoming _incomingCache;
    private java.lang.Object _incomingCacheMutex = new java.lang.Object();
}
