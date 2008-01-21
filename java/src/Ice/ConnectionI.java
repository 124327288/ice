// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package Ice;

public final class ConnectionI extends IceInternal.EventHandler 
    implements Connection, IceInternal.SelectorThread.SocketReadyCallback
{
    public interface StartCallback
    {
        void connectionStartCompleted(ConnectionI connection);
        void connectionStartFailed(ConnectionI connection, Ice.LocalException ex);
    }

    public void
    start(StartCallback callback)
    {
        try
        {
            synchronized(this)
            {
                _startCallback = callback;

                //
                // The connection might already be closed if the communicator was destroyed.
                //
                if(_state == StateClosed)
                {
                    assert(_exception != null);
                    throw _exception;
                }

                //
                // In thread per connection mode, we create the thread for the connection. The
                // intialization and validation of the connection is taken care of by the thread
                // per connection. If a callback is given, no need to wait, the thread will notify
                // the callback, otherwise wait until the connection is validated.
                //
                if(_threadPerConnection)
                {
                    try
                    {
                        _thread = new ThreadPerConnection();
                        _thread.start();
                    }
                    catch(java.lang.Exception ex)
                    {
                        java.io.StringWriter sw = new java.io.StringWriter();
                        java.io.PrintWriter pw = new java.io.PrintWriter(sw);
                        ex.printStackTrace(pw);
                        pw.flush();
                        _logger.error("cannot create thread for connection:\n" + sw.toString());

                        //
                        // Clean up.
                        //
                        _thread = null;
                        _state = StateClosed;

                        Ice.SyscallException e = new Ice.SyscallException();
                        e.initCause(ex);
                        throw e;
                    }

                    if(callback == null) // Wait for the connection to be validated.
                    {
                        while(_state <= StateNotValidated)
                        {
                            try
                            {
                                wait();
                            }
                            catch(InterruptedException ex)
                            {
                            }
                        }
                        
                        if(_state >= StateClosing)
                        {
                            assert(_exception != null);
                            throw _exception;
                        }
                    }
                    return; // We're done.
                }
            }

            assert(!_threadPerConnection);

            //
            // Initialize the connection transceiver and then validate the connection.
            //
            IceInternal.SocketStatus status = initialize();
            if(status == IceInternal.SocketStatus.Finished)
            {
                status = validate();
            }

            if(status == IceInternal.SocketStatus.Finished)
            {
                finishStart(null);
                return; // We're done!
            }

            //
            // If the initialization or validation couldn't be completed without potentially 
            // blocking, we register the connection with the selector thread and return.
            //

            synchronized(this)
            {
                if(_state == StateClosed)
                {
                    assert(_exception != null);
                    throw _exception;
                }

                int timeout;
                IceInternal.DefaultsAndOverrides defaultsAndOverrides = _instance.defaultsAndOverrides();
                if(defaultsAndOverrides.overrideConnectTimeout)
                {
                    timeout = defaultsAndOverrides.overrideConnectTimeoutValue;
                }
                else
                {
                    timeout = _endpoint.timeout();
                }
                
                _sendInProgress = true;
                _selectorThread._register(_transceiver.fd(), this, status, timeout);
            }
        }
        catch(Ice.LocalException ex)
        {
            synchronized(this)
            {
                setState(StateClosed, ex);

                //
                // If start is called with a callback, the callback is notified either by the
                // thread per conncetion or the thread pool.
                //
                if(callback != null)
                {
                    if(!_threadPerConnection)
                    {
                        registerWithPool();
                        unregisterWithPool(); // Let finished() do the close.
                    }
                    return;
                }
                
                //
                // Close the transceiver if there's no thread per connection. Otherwise, wait
                // for the thread per connection to take care of it.
                // 
                if(_thread == null && _transceiver != null)
                {
                    try
                    {
                        _transceiver.close();
                    }
                    catch(LocalException e)
                    {
                        // Here we ignore any exceptions in close().
                    }
                    _transceiver = null;
                }
            }

            waitUntilFinished();
            throw ex;
        }
    }

    public synchronized void
    activate()
    {
        if(_state <= StateNotValidated)
        {
            return;
        }

        if(_acmTimeout > 0)
        {
            _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
        }
            
        setState(StateActive);
    }

    public synchronized void
    hold()
    {
        if(_state <= StateNotValidated)
        {
            return;
        }

        setState(StateHolding);
    }

    // DestructionReason.
    public final static int ObjectAdapterDeactivated = 0;
    public final static int CommunicatorDestroyed = 1;

    synchronized public void
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
    
    synchronized public void
    close(boolean force)
    {
        if(force)
        {
            setState(StateClosed, new ForcedCloseConnectionException());
        }
        else
        {
            //
            // If we do a graceful shutdown, then we wait until all
            // outstanding requests have been completed. Otherwise,
            // the CloseConnectionException will cause all outstanding
            // requests to be retried, regardless of whether the
            // server has processed them or not.
            //
            while(!_requests.isEmpty() || !_asyncRequests.isEmpty())
            {
                try
                {
                    wait();
                }
                catch(InterruptedException ex)
                {
                }
            }
            
            setState(StateClosing, new CloseConnectionException());
        }
    }

    public synchronized boolean
    isActiveOrHolding()
    {
        return _state > StateNotValidated && _state < StateClosing;
    }

    public boolean
    isFinished()
    {
        Thread threadPerConnection = null;

        synchronized(this)
        {
            if(_transceiver != null || _dispatchCount != 0 || (_thread != null && _thread.isAlive()))
            {
                return false;
            }

            assert(_state == StateClosed);

            threadPerConnection = _thread;
            _thread = null;
        }

        if(threadPerConnection != null)
        {
            while(true)
            {
                try
                {
                    threadPerConnection.join();
                    break;
                }
                catch(InterruptedException ex)
                {
                }
            }
        }

        return true;
    }

    public synchronized void
    throwException()
    {
        if(_exception != null)
        {
            assert(_state >= StateClosing);
            throw _exception;
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

    public void
    waitUntilFinished()
    {
        Thread threadPerConnection = null;

        synchronized(this)
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
                        long waitTime = absoluteWaitTime - IceInternal.Time.currentMonotonicTimeMillis();
                        
                        if(waitTime > 0)
                        {
                            //
                            // We must wait a bit longer until we close this
                            // connection.
                            //
                            wait(waitTime);
                            if(IceInternal.Time.currentMonotonicTimeMillis() >= absoluteWaitTime)
                            {
                                setState(StateClosed, new CloseTimeoutException());
                            }
                        }
                        else
                        {
                            //
                            // We already waited long enough, so let's close this
                            // connection!
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

            threadPerConnection = _thread;
            _thread = null;

            //
            // Clear the OA. See bug 1673 for the details of why this is necessary.
            //
            _adapter = null;
        }

        if(threadPerConnection != null)
        {
            while(true)
            {
                try
                {
                    threadPerConnection.join();
                    break;
                }
                catch(InterruptedException ex)
                {
                }
            }
        }
    }

    synchronized public void
    monitor()
    {
        if(_state != StateActive)
        {
            return;
        }
        
        //
        // Active connection management for idle connections.
        //
        if(_acmTimeout <= 0 ||
           !_requests.isEmpty() || !_asyncRequests.isEmpty() ||
           _batchStreamInUse || !_batchStream.isEmpty() ||
           _sendInProgress || _dispatchCount > 0)
        {
            return;
        }
        
        if(IceInternal.Time.currentMonotonicTimeMillis() >= _acmAbsoluteTimeoutMillis)
        {
            setState(StateClosing, new ConnectionTimeoutException());
        }
    }

    synchronized public boolean
    sendRequest(IceInternal.Outgoing out, boolean compress, boolean response)
        throws IceInternal.LocalExceptionWrapper
    {
        int requestId = 0;
        final IceInternal.BasicStream os = out.os();

        if(_exception != null)
        {
            //
            // If the connection is closed before we even have a chance
            // to send our request, we always try to send the request
            // again.
            //
            throw new IceInternal.LocalExceptionWrapper(_exception, true);
        }
        
        assert(_state > StateNotValidated);
        assert(_state < StateClosing);
        
        if(response)
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
        }
        
        //
        // Send the message. If it can't be sent without blocking the message is added
        // to _sendStreams and it will be sent by the selector thread or by this thread
        // if flush is true.
        //
        boolean sent = false;
        try
        {
            sent = sendMessage(new OutgoingMessage(out, out.os(), compress, response));
        }
        catch(Ice.LocalException ex)
        {
            setState(StateClosed, ex);
            assert(_exception != null);
            throw _exception;
        }

        if(response)
        {
            //
            // Add to the requests map.
            //
            _requests.put(requestId, out);
        }

        return sent; // The request was sent.
    }

    synchronized public void
    sendAsyncRequest(IceInternal.OutgoingAsync out, boolean compress, boolean response)
        throws IceInternal.LocalExceptionWrapper
    {
        int requestId = 0;
        final IceInternal.BasicStream os = out.__os();
        
        if(_exception != null)
        {
            //
            // If the connection is closed before we even have a chance
            // to send our request, we always try to send the request
            // again.
            //
            throw new IceInternal.LocalExceptionWrapper(_exception, true);
        }
        
        assert(_state > StateNotValidated);
        assert(_state < StateClosing);
        
        if(response)
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
        }
        
        try
        {
            sendMessage(new OutgoingMessage(out, out.__os(), compress, response));
        }
        catch(Ice.LocalException ex)
        {
            setState(StateClosed, ex);
            assert(_exception != null);
            throw _exception;
        }
        
        if(response)
        {
            //
            // Add to the async requests map.
            //
            _asyncRequests.put(requestId, out);
        }
    }

    public synchronized void
    prepareBatchRequest(IceInternal.BasicStream os)
    {
        //
        // Wait if flushing is currently in progress.
        //
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
                _batchStream.writeBlob(IceInternal.Protocol.requestBatchHdr);
            }
            catch(LocalException ex)
            {
                setState(StateClosed, ex);
                throw ex;
            }
        }

        _batchStreamInUse = true;
        _batchMarker = _batchStream.size();
        _batchStream.swap(os);

        //
        // The batch stream now belongs to the caller, until
        // finishBatchRequest() or abortBatchRequest() is called.
        //
    }

    public void
    finishBatchRequest(IceInternal.BasicStream os, boolean compress)
    {
        try
        {
            synchronized(this)
            {
                //
                // Get the batch stream back.
                //
                _batchStream.swap(os);

                if(_exception != null)
                {
                    throw _exception;
                }

                boolean flush = false;
                if(_batchAutoFlush)
                {
                    //
                    // Throw memory limit exception if the first message added causes us to go over
                    // limit. Otherwise put aside the marshalled message that caused limit to be 
                    // exceeded and rollback stream to the marker.
                    try
                    {
                        _transceiver.checkSendSize(_batchStream.getBuffer(), _instance.messageSizeMax());
                    }
                    catch(Ice.LocalException ex)
                    {
                        if(_batchRequestNum > 0)
                        {
                            flush = true;
                        }
                        else
                        {
                            throw ex;
                        }
                    }
                }

                if(flush)
                {
                    //
                    // Temporarily save the last request.
                    //
                    byte[] lastRequest = new byte[_batchStream.size() - _batchMarker];
                    IceInternal.Buffer buffer = _batchStream.getBuffer();
                    buffer.b.position(_batchMarker);
                    buffer.b.get(lastRequest);
                    _batchStream.resize(_batchMarker, false);

                    try
                    {
                        //
                        // Fill in the number of requests in the batch.
                        //
                        _batchStream.pos(IceInternal.Protocol.headerSize);
                        _batchStream.writeInt(_batchRequestNum);
                    
                        OutgoingMessage message = new OutgoingMessage(_batchStream, _batchRequestCompress, true);
                        sendMessage(message);
                    }
                    catch(Ice.LocalException ex)
                    {
                        setState(StateClosed, ex);
                        assert(_exception != null);
                        throw _exception;
                    }

                    //
                    // Reset the batch stream.
                    //
                    _batchStream = new IceInternal.BasicStream(_instance, _batchAutoFlush);
                    _batchRequestNum = 0;
                    _batchRequestCompress = false;
                    _batchMarker = 0;

                    //
                    // Check again if the last request doesn't exceed the maximum message size.
                    //
                    if(IceInternal.Protocol.requestBatchHdr.length + lastRequest.length >  _instance.messageSizeMax())
                    {
                        throw new MemoryLimitException();
                    }

                    //
                    // Start a new batch with the last message that caused us to go over the limit.
                    //
                    _batchStream.writeBlob(IceInternal.Protocol.requestBatchHdr);
                    _batchStream.writeBlob(lastRequest);
                }

                //
                // Increment the number of requests in the batch.
                //
                ++_batchRequestNum;
            
                //
                // We compress the whole batch if there is at least one compressed
                // message.
                //
                if(compress)
                {
                    _batchRequestCompress = true;
                }

                //
                // Notify about the batch stream not being in use anymore.
                //
                assert(_batchStreamInUse);
                _batchStreamInUse = false;
                notifyAll();
            }
        }
        catch(Ice.LocalException ex)
        {
            abortBatchRequest();
            throw ex;
        }
    }

    public synchronized void
    abortBatchRequest()
    {
        _batchStream = new IceInternal.BasicStream(_instance, _batchAutoFlush);
        _batchRequestNum = 0;
        _batchRequestCompress = false;
        _batchMarker = 0;

        assert(_batchStreamInUse);
        _batchStreamInUse = false;
        notifyAll();
    }

    public void
    flushBatchRequests()
    {
        IceInternal.BatchOutgoing out = new IceInternal.BatchOutgoing(this, _instance);
        out.invoke();
    }

    synchronized public boolean
    flushBatchRequests(IceInternal.BatchOutgoing out)
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
        
        if(_batchRequestNum == 0)
        {
            out.sent(false);
            return true;
        }
        
        //
        // Fill in the number of requests in the batch.
        //
        _batchStream.pos(IceInternal.Protocol.headerSize);
        _batchStream.writeInt(_batchRequestNum);
        
        _batchStream.swap(out.os());
        
        boolean sent = false;
        try
        {
            OutgoingMessage message = new OutgoingMessage(out, out.os(), _batchRequestCompress, false);
            sent = sendMessage(message);
        }
        catch(Ice.LocalException ex)
        {
            setState(StateClosed, ex);
            assert(_exception != null);
            throw _exception;
        }
        
        //
        // Reset the batch stream.
        //
        _batchStream = new IceInternal.BasicStream(_instance, _batchAutoFlush);
        _batchRequestNum = 0;
        _batchRequestCompress = false;
        _batchMarker = 0;
        return sent;
    }

    synchronized public void
    flushAsyncBatchRequests(IceInternal.BatchOutgoingAsync outAsync)
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

        if(_batchRequestNum == 0)
        {
            outAsync.__sent(this);
            return;
        }

        //
        // Fill in the number of requests in the batch.
        //
        _batchStream.pos(IceInternal.Protocol.headerSize);
        _batchStream.writeInt(_batchRequestNum);
            
        _batchStream.swap(outAsync.__os());

        try
        {
            OutgoingMessage message = new OutgoingMessage(outAsync, outAsync.__os(), _batchRequestCompress, false);
            sendMessage(message);
        }
        catch(Ice.LocalException ex)
        {
            setState(StateClosed, ex);
            assert(_exception != null);
            throw _exception;
        }
            
        //
        // Reset the batch stream.
        //
        _batchStream = new IceInternal.BasicStream(_instance, _batchAutoFlush);
        _batchRequestNum = 0;
        _batchRequestCompress = false;
        _batchMarker = 0;
    }

    synchronized public void
    sendResponse(IceInternal.BasicStream os, byte compressFlag)
    {
        assert(_state > StateNotValidated);

        try
        {
            if(--_dispatchCount == 0)
            {
                notifyAll();
            }

            if(_state == StateClosed)
            {
                assert(_exception != null);
                throw _exception;
            }

            sendMessage(new OutgoingMessage(os, compressFlag != 0, true));
                
            if(_state == StateClosing && _dispatchCount == 0)
            {
                initiateShutdown();
            }

            if(_acmTimeout > 0)
            {
                _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
            }
        }
        catch(LocalException ex)
        {
            setState(StateClosed, ex);
        }
    }

    synchronized public void
    sendNoResponse()
    {
        assert(_state > StateNotValidated);
        try
        {
            if(--_dispatchCount == 0)
            {
                notifyAll();
            }
            
            if(_state == StateClosed)
            {
                assert(_exception != null);
                throw _exception;
            }

            if(_state == StateClosing && _dispatchCount == 0)
            {
                initiateShutdown();
            }

            if(_acmTimeout > 0)
            {
                _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
            }
        }
        catch(LocalException ex)
        {
            setState(StateClosed, ex);
        }
    }

    public IceInternal.EndpointI
    endpoint()
    {
        return _endpoint; // No mutex protection necessary, _endpoint is immutable.
    }

    public boolean
    threadPerConnection()
    {
        return _threadPerConnection; // No mutex protection necessary, _threadPerConnection is immutable.
    }

    public synchronized void
    setAdapter(ObjectAdapter adapter)
    {
        if(_state == StateClosing || _state == StateClosed)
        {
            assert(_exception != null);
            throw _exception;
        }
        else if(_state <= StateNotValidated)
        {
            return;
        }
        assert(_state < StateClosing);

        _adapter = adapter;

        if(_adapter != null)
        {
            _servantManager = ((ObjectAdapterI)_adapter).getServantManager();
            if(_servantManager == null)
            {
                _adapter = null;
            }
        }
        else
        {
            _servantManager = null;
        }

        //
        // We never change the thread pool with which we were
        // initially registered, even if we add or remove an object
        // adapter.
        //
    }

    public synchronized ObjectAdapter
    getAdapter()
    {
        return _adapter;
    }

    public ObjectPrx
    createProxy(Identity ident)
    {
        //
        // Create a reference and return a reverse proxy for this
        // reference.
        //
        return _instance.proxyFactory().referenceToProxy(_instance.referenceFactory().create(ident, this));
    }

    //
    // Operations from EventHandler
    //

    public boolean
    datagram()
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.
        return _endpoint.datagram(); // No mutex protection necessary, _endpoint is immutable.
    }

    public boolean
    readable()
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.
        return true;
    }

    public boolean
    read(IceInternal.BasicStream stream)
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.

        return _transceiver.read(stream.getBuffer(), 0, _hasMoreData);

        //
        // Updating _acmAbsoluteTimeoutMillis is too expensive here,
        // because we would have to acquire a lock just for this
        // purpose. Instead, we update _acmAbsoluteTimeoutMillis in
        // message().
        //
    }

    public boolean
    hasMoreData()
    {
        return _hasMoreData.value;
    }

    public void
    message(IceInternal.BasicStream stream, IceInternal.ThreadPool threadPool)
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.

        MessageInfo info = new MessageInfo(stream);

        synchronized(this)
        {
            //
            // We must promote within the synchronization, otherwise
            // there could be various race conditions with close
            // connection messages and other messages.
            //
            threadPool.promoteFollower();

            if(_state != StateClosed)
            {
                parseMessage(info);
            }

            //
            // parseMessage() can close the connection, so we must check
            // for closed state again.
            //
            if(_state == StateClosed)
            {
                return;
            }
        }

        //
        // Asynchronous replies must be handled outside the thread
        // synchronization, so that nested calls are possible.
        //
        if(info.outAsync != null)
        {
            info.outAsync.__finished(info.stream);
        }

        //
        // Method invocation (or multiple invocations for batch messages)
        // must be done outside the thread synchronization, so that nested
        // calls are possible.
        //
        invokeAll(info.stream, info.invokeNum, info.requestId, info.compress, info.servantManager, info.adapter);
    }

    public void
    finished(IceInternal.ThreadPool threadPool)
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.

        threadPool.promoteFollower();
        
        Ice.LocalException localEx = null;

        synchronized(this)
        {
            --_finishedCount;
            if(_finishedCount > 0 || _state != StateClosed || _sendInProgress)
            {
                return;
            }
            
            try
            {
                _transceiver.close();
            }
            catch(LocalException ex)
            {
                localEx = ex;
            }
            
            _transceiver = null;
            notifyAll();
        }
            
        finishStart(_exception);

        java.util.Iterator i = _queuedStreams.iterator();
        while(i.hasNext())
        {
            OutgoingMessage message = (OutgoingMessage)i.next();
            message.finished(_exception);
        }
        _queuedStreams.clear();
        
        i = _requests.entryIterator(); // _requests is immutable at this point.
        while(i.hasNext())
        {
            IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
            IceInternal.Outgoing out = (IceInternal.Outgoing)e.getValue();
            out.finished(_exception); // The exception is immutable at this point.
        }
        _requests.clear();

        i = _asyncRequests.entryIterator(); // _asyncRequests is immutable at this point.
        while(i.hasNext())
        {
            IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
            IceInternal.OutgoingAsync out = (IceInternal.OutgoingAsync)e.getValue();
            out.__finished(_exception); // The exception is immutable at this point.
        }
        _asyncRequests.clear();
        
        if(localEx != null)
        {
            throw localEx;
        }
    }

    public synchronized void
    exception(LocalException ex)
    {
        setState(StateClosed, ex);
    }

    public synchronized void
    invokeException(LocalException ex, int invokeNum)
    {
        //
        // Fatal exception while invoking a request. Since sendResponse/sendNoResponse isn't
        // called in case of a fatal exception we decrement _dispatchCount here.
        //

        setState(StateClosed, ex);

        if(invokeNum > 0)
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

    //
    // Operations from SocketReadyCallback
    // 
    public IceInternal.SocketStatus 
    socketReady(boolean finished)
    {
        if(!finished)
        {
            try
            {
                //
                // First, we check if there's something to send. If that's the case, the connection
                // must be active and the only thing to do is send the queued streams.
                //
                if(!_sendStreams.isEmpty())
                {
                    if(!send(0))
                    {
                        return IceInternal.SocketStatus.NeedWrite;
                    }
                    assert(_sendStreams.isEmpty());
                }
                else
                {
                    //
                    // If there's nothing to send, we're still validating the connection.
                    //
                    int state;
                    synchronized(this)
                    {
                        assert(_state == StateClosed || _state <= StateNotValidated);

                        if(_state == StateClosed)
                        {
                            assert(_exception != null);
                            throw _exception;
                        }   
                    
                        state = _state;
                    }
                
                    if(state == StateNotInitialized)
                    {
                        IceInternal.SocketStatus status = initialize();
                        if(status != IceInternal.SocketStatus.Finished)
                        {
                            return status;
                        }
                    }
                
                    if(state <= StateNotValidated)
                    {
                        IceInternal.SocketStatus status = validate();
                        if(status != IceInternal.SocketStatus.Finished)
                        {
                            return status;
                        }
                    }
                
                    finishStart(null);
                }
            }
            catch(Ice.LocalException ex)
            {
                synchronized(this)
                {
                    setState(StateClosed, ex);
                }
            }
        }

        //
        // If there's no more data to send or if connection validation is finished, we checkout 
        // the connection state to figure out whether or not it's time to unregister with the
        // selector thread.
        // 

        synchronized(this)
        {
            assert(_sendInProgress);
            if(_state == StateClosed)
            {
                assert(_startCallback == null || (!_threadPerConnection && !_registeredWithPool));

                _queuedStreams.addAll(0, _sendStreams);
                _sendInProgress = false;

                if(_threadPerConnection)
                {
                    _transceiver.shutdownReadWrite();
                }
                else
                {
                    registerWithPool();
                    unregisterWithPool(); // Let finished() do the close.
                }
                
                notifyAll();
                return IceInternal.SocketStatus.Finished;
            }
            else if(_waitingForSend > 0)
            {
                _sendInProgress = false;
                notifyAll();
                return IceInternal.SocketStatus.Finished;
            }
            else if(_queuedStreams.isEmpty())
            {
                _sendInProgress = false;
                if(_acmTimeout > 0)
                {
                    _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
                }
                return IceInternal.SocketStatus.Finished;
            }
            else
            {
                java.util.LinkedList streams = _queuedStreams;
                _queuedStreams = _sendStreams;
                _sendStreams = streams;
                return IceInternal.SocketStatus.NeedWrite; // We're not finished yet, there's more data to send!
            }
        }
    }

    public void
    socketTimeout()
    {
        synchronized(this)
        {
            if(_state <= StateNotValidated)
            {
                setState(StateClosed, new ConnectTimeoutException());
            }
            else if(_state <= StateClosing)
            {
                setState(StateClosed, new TimeoutException());
            }
        }
    }

    //
    // Only used by the SSL plug-in.
    //
    // The external party has to synchronize the connection, since the
    // connection is the object that protects the transceiver.
    //
    public IceInternal.Transceiver
    getTransceiver()
    {
        return _transceiver;
    }

    public ConnectionI(IceInternal.Instance instance, IceInternal.Transceiver transceiver, 
                       IceInternal.EndpointI endpoint, ObjectAdapter adapter, boolean threadPerConnection)
    {
        super(instance);

        final Ice.InitializationData initData = instance.initializationData();
        _threadPerConnection = threadPerConnection;
        _transceiver = transceiver;
        _desc = transceiver.toString();
        _type = transceiver.type();
        _endpoint = endpoint;
        _adapter = adapter;
        _logger = initData.logger; // Cached for better performance.
        _traceLevels = instance.traceLevels(); // Cached for better performance.
        _registeredWithPool = false;
        _finishedCount = 0;
        _warn = initData.properties.getPropertyAsInt("Ice.Warn.Connections") > 0 ? true : false;
        _cacheBuffers = initData.properties.getPropertyAsIntWithDefault("Ice.CacheMessageBuffers", 1) == 1;
        _acmAbsoluteTimeoutMillis = 0;
        _nextRequestId = 1;
        _batchAutoFlush = initData.properties.getPropertyAsIntWithDefault("Ice.BatchAutoFlush", 1) > 0 ? true : false;
        _batchStream = new IceInternal.BasicStream(instance, _batchAutoFlush);
        _batchStreamInUse = false;
        _batchRequestNum = 0;
        _batchRequestCompress = false;
        _batchMarker = 0;
        _sendInProgress = false;
        _waitingForSend = 0;
        _dispatchCount = 0;
        _state = StateNotInitialized;
        _stateTime = IceInternal.Time.currentMonotonicTimeMillis();

        if(_endpoint.datagram())
        {
            _acmTimeout = 0;
        }
        else
        {
            if(_adapter != null)
            {
                _acmTimeout = _instance.serverACM();
            }
            else
            {
                _acmTimeout = _instance.clientACM();
            }
        }

        int compressionLevel = initData.properties.getPropertyAsIntWithDefault("Ice.Compression.Level", 1);
        if(compressionLevel < 1)
        {
            compressionLevel = 1;
        }
        else if(compressionLevel > 9)
        {
            compressionLevel = 9;
        }
        _compressionLevel = compressionLevel;

        if(_adapter != null)
        {
            _servantManager = ((ObjectAdapterI)_adapter).getServantManager();
        }
        else
        {
            _servantManager = null;
        }
        
        try
        {
            if(!threadPerConnection)
            {
                //
                // Only set _threadPool if we really need it, i.e., if we are
                // not in thread per connection mode. Thread pools have lazy
                // initialization in Instance, and we don't want them to be
                // created if they are not needed.
                //
                if(_adapter != null)
                {
                    _threadPool = ((ObjectAdapterI)_adapter).getThreadPool();
                }
                else
                {
                    _threadPool = _instance.clientThreadPool();
                }
            }
            else
            {
                _threadPool = null; // To satisfy the compiler.
            }

            _selectorThread = _instance.selectorThread();

            _overrideCompress = _instance.defaultsAndOverrides().overrideCompress;
            _overrideCompressValue = _instance.defaultsAndOverrides().overrideCompressValue;
        }
        catch(Ice.LocalException ex)
        {
            throw ex;
        }
        catch(java.lang.Exception ex)
        {
            Ice.SyscallException e = new Ice.SyscallException();
            e.initCause(ex);
            throw e;
        }
    }

    protected synchronized void
    finalize()
        throws Throwable
    {
        IceUtilInternal.Assert.FinalizerAssert(_startCallback == null);
        IceUtilInternal.Assert.FinalizerAssert(_state == StateClosed);
        IceUtilInternal.Assert.FinalizerAssert(_transceiver == null);
        IceUtilInternal.Assert.FinalizerAssert(_dispatchCount == 0);
        IceUtilInternal.Assert.FinalizerAssert(_thread == null);
        IceUtilInternal.Assert.FinalizerAssert(_queuedStreams.isEmpty());
        IceUtilInternal.Assert.FinalizerAssert(_requests.isEmpty());
        IceUtilInternal.Assert.FinalizerAssert(_asyncRequests.isEmpty());

        super.finalize();
    }

    private static final int StateNotInitialized = 0;
    private static final int StateNotValidated = 1;
    private static final int StateActive = 2;
    private static final int StateHolding = 3;
    private static final int StateClosing = 4;
    private static final int StateClosed = 5;

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
                         _exception instanceof ForcedCloseConnectionException ||
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

        //
        // Skip graceful shutdown if we are destroyed before validation.
        //
        if(_state <= StateNotValidated && state == StateClosing)
        {
            state = StateClosed;
        }

        if(_state == state) // Don't switch twice.
        {
            return;
        }

        switch(state)
        {
            case StateNotInitialized:
            {
                assert(false);
                break;
            }

            case StateNotValidated:
            {
                if(_state != StateNotInitialized)
                {
                    assert(_state == StateClosed);
                    return;
                }
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
                if(!_threadPerConnection)
                {
                    registerWithPool();
                }
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
                if(!_threadPerConnection)
                {
                    unregisterWithPool();
                }
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
                if(!_threadPerConnection)
                {
                    registerWithPool(); // We need to continue to read in closing state.
                }
                break;
            }
            
            case StateClosed:
            {
                if(_sendInProgress)
                {
                    //
                    // Unregister with both the pool and the selector thread. We unregister with
                    // the pool to ensure that it stops reading on the socket (otherwise, if the
                    // socket is closed the thread pool would spin always reading 0 from the FD).
                    // The selector thread will register again the FD with the pool once it's 
                    // done.
                    //
                    _selectorThread.unregister(_transceiver.fd());
                    if(!_threadPerConnection)
                    {
                        unregisterWithPool();
                    }

                    _transceiver.shutdownWrite();
                }
                else if(_state <= StateNotValidated || _threadPerConnection)
                {
                    //
                    // If we are in thread per connection mode and the thread is started, we
                    // shutdown both for reading and writing. This will unblock the read call
                    // with an exception. The thread per connection closes the transceiver.
                    //
                    _transceiver.shutdownReadWrite();
                }
                else
                {
                    registerWithPool();
                    unregisterWithPool(); // Let finished() do the close.
                    
                    //
                    // Prevent further writes.
                    //                    
                    _transceiver.shutdownWrite();
                }
                break;
            }
        }

        //  
        // We only register with the connection monitor if our new state
        // is StateActive. Otherwise we unregister with the connection
        // monitor, but only if we were registered before, i.e., if our
        // old state was StateActive.
        //
        IceInternal.ConnectionMonitor connectionMonitor = _instance.connectionMonitor();
        if(connectionMonitor != null)
        {
            if(state == StateActive)
            {
                connectionMonitor.add(this);
            }
            else if(_state == StateActive)
            {
                connectionMonitor.remove(this);
            }
        }

        _state = state;
        _stateTime = IceInternal.Time.currentMonotonicTimeMillis();

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
            //
            // Before we shut down, we send a close connection
            // message.
            //
            IceInternal.BasicStream os = new IceInternal.BasicStream(_instance);
            os.writeBlob(IceInternal.Protocol.magic);
            os.writeByte(IceInternal.Protocol.protocolMajor);
            os.writeByte(IceInternal.Protocol.protocolMinor);
            os.writeByte(IceInternal.Protocol.encodingMajor);
            os.writeByte(IceInternal.Protocol.encodingMinor);
            os.writeByte(IceInternal.Protocol.closeConnectionMsg);
            os.writeByte(_compressionSupported ? (byte)1 : (byte)0);
            os.writeInt(IceInternal.Protocol.headerSize); // Message size.

            sendMessage(new OutgoingMessage(os, false, false));

            //
            // The CloseConnection message should be sufficient. Closing the write
            // end of the socket is probably an artifact of how things were done
            // in IIOP. In fact, shutting down the write end of the socket causes
            // problems on Windows by preventing the peer from using the socket.
            // For example, the peer is no longer able to continue writing a large
            // message after the socket is shutdown.
            //
            //_transceiver.shutdownWrite();
        }
    }

    private IceInternal.SocketStatus 
    initialize()
    {
        int timeout = 0;
        if(_startCallback == null || _threadPerConnection)
        {
            IceInternal.DefaultsAndOverrides defaultsAndOverrides = _instance.defaultsAndOverrides();
            if(defaultsAndOverrides.overrideConnectTimeout)
            {
                timeout = defaultsAndOverrides.overrideConnectTimeoutValue;
            }
            else
            {
                timeout = _endpoint.timeout();
            }
        }
        
        try
        {
            IceInternal.SocketStatus status = _transceiver.initialize(timeout);
            if(status != IceInternal.SocketStatus.Finished)
            {
                if(_startCallback == null || _threadPerConnection)
                {
                    throw new Ice.TimeoutException();
                }
                return status;
            }
        }
        catch(Ice.TimeoutException ex)
        {
            throw new Ice.ConnectTimeoutException();
        }                
        
        synchronized(this)
        {
            if(_state == StateClosed)
            {
                assert(_exception != null);
                throw _exception;
            }

            //
            // Update the connection description once the transceiver is initialized.
            //
            _desc = _transceiver.toString();

            setState(StateNotValidated);
        }

        return IceInternal.SocketStatus.Finished;
    }

    private IceInternal.SocketStatus
    validate()
    {
        if(!_endpoint.datagram()) // Datagram connections are always implicitly validated.
        {
            int timeout = 0;
            if(_startCallback == null || _threadPerConnection)
            {
                IceInternal.DefaultsAndOverrides defaultsAndOverrides = _instance.defaultsAndOverrides();
                if(defaultsAndOverrides.overrideConnectTimeout)
                {
                    timeout = defaultsAndOverrides.overrideConnectTimeoutValue;
                }
                else
                {
                    timeout = _endpoint.timeout();
                }
            }
            
            if(_adapter != null) // The server side has the active role for connection validation.
            {
                IceInternal.BasicStream os = _stream;
                if(os.size() == 0)
                {
                    os.writeBlob(IceInternal.Protocol.magic);
                    os.writeByte(IceInternal.Protocol.protocolMajor);
                    os.writeByte(IceInternal.Protocol.protocolMinor);
                    os.writeByte(IceInternal.Protocol.encodingMajor);
                    os.writeByte(IceInternal.Protocol.encodingMinor);
                    os.writeByte(IceInternal.Protocol.validateConnectionMsg);
                    os.writeByte((byte)0); // Compression status (always zero for validate connection).
                    os.writeInt(IceInternal.Protocol.headerSize); // Message size.
                    IceInternal.TraceUtil.traceSend(os, _logger, _traceLevels);
                    os.prepareWrite();
                }
                else
                {
                    // The stream can only be non-empty if we're doing a non-blocking connection validation.
                    assert(_startCallback != null && !_threadPerConnection); 
                }
                
                try
                {
                    if(!_transceiver.write(os.getBuffer(), timeout))
                    {
                        if(_startCallback == null || _threadPerConnection)
                        {
                            throw new Ice.TimeoutException();
                        }
                        return IceInternal.SocketStatus.NeedWrite;
                    }
                }
                catch(Ice.TimeoutException ex)
                {
                    throw new Ice.ConnectTimeoutException();
                }
            }
            else // The client side has the passive role for connection validation.
            {
                IceInternal.BasicStream is = _stream;
                if(is.size() == 0)
                {
                    is.resize(IceInternal.Protocol.headerSize, true);
                    is.pos(0);
                }
                else
                {
                    // The stream can only be non-empty if we're doing a non-blocking connection validation.
                    assert(_startCallback != null && !_threadPerConnection); 
                }

                try
                {
                    if(!_transceiver.read(is.getBuffer(), timeout, _hasMoreData))
                    {
                        if(_startCallback == null || _threadPerConnection)
                        {
                            throw new Ice.TimeoutException();
                        }
                        return IceInternal.SocketStatus.NeedRead;
                    }
                }
                catch(Ice.TimeoutException ex)
                {
                    throw new Ice.ConnectTimeoutException();
                }

                assert(is.pos() == IceInternal.Protocol.headerSize);
                is.pos(0);
                byte[] m = is.readBlob(4);
                if(m[0] != IceInternal.Protocol.magic[0] || m[1] != IceInternal.Protocol.magic[1] ||
                   m[2] != IceInternal.Protocol.magic[2] || m[3] != IceInternal.Protocol.magic[3])
                {
                    BadMagicException ex = new BadMagicException();
                    ex.badMagic = m;
                    throw ex;
                }
                byte pMajor = is.readByte();
                byte pMinor = is.readByte();
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
                IceInternal.TraceUtil.traceRecv(is, _logger, _traceLevels);
            }
        }
        
        synchronized(this)
        {
            _stream.reset();

            if(_state == StateClosed)
            {
                assert(_exception != null);
                throw _exception;
            }

            //
            // We start out in holding state.
            //
            setState(StateHolding);
        }

        return IceInternal.SocketStatus.Finished;
    }

    private boolean
    send(int timeout)
    {
        assert(_transceiver != null);
        assert(!_sendStreams.isEmpty());

        while(!_sendStreams.isEmpty())
        {
            OutgoingMessage message = (OutgoingMessage)_sendStreams.getFirst();
            if(!message.prepared)
            {
                IceInternal.BasicStream stream = message.stream;

                boolean compress = _overrideCompress ? _overrideCompressValue : message.compress;
                message.stream = doCompress(stream, compress);
                message.stream.prepareWrite();
                message.prepared = true;

                if(message.outAsync != null)
                {
                    IceInternal.TraceUtil.trace("sending asynchronous request", stream, _logger, _traceLevels);
                }
                else
                {
                    IceInternal.TraceUtil.traceSend(stream, _logger, _traceLevels);
                }
                
            }

            if(!_transceiver.write(message.stream.getBuffer(), timeout))
            {
                assert(timeout == 0);
                return false;
            }
                    
            message.sent(this, timeout == 0); // timeout == 0 indicates that this is called by the selector thread.
            _sendStreams.removeFirst();
        }

        return true;
    }

    private boolean
    sendMessage(OutgoingMessage message)
    {
        assert(_state != StateClosed);

        //
        // If another thread is currently sending messages, we queue the
        // message in _queuedStreams.  It will be picked up eventually by
        // the selector thread once the messages from _sendStreams are all
        // sent.
        //
        if(_sendInProgress)
        {    
            message.adopt();
            _queuedStreams.addLast(message);
            return false;
        }

        assert(!_sendInProgress);

        //
        // Attempt to send the message without blocking. If the send blocks, we register 
        // the connection with the selector thread or we request the caller to call 
        // finishSendMessage() outside the synchronization.
        //

        assert(!message.prepared);
        
        IceInternal.BasicStream stream = message.stream;

        boolean compress = _overrideCompress ? _overrideCompressValue : message.compress;
        message.stream = doCompress(stream, compress);
        message.stream.prepareWrite();
        message.prepared = true;

        if(message.outAsync != null)
        {
            IceInternal.TraceUtil.trace("sending asynchronous request", stream, _logger, _traceLevels);
        }
        else
        {
            IceInternal.TraceUtil.traceSend(stream, _logger, _traceLevels);
        }
        
        if(_transceiver.write(message.stream.getBuffer(), 0))
        {
            message.sent(this, false);
            if(_acmTimeout > 0)
            {
                _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
            }
            return true;
        }

        _sendStreams.addLast(message);
        _sendInProgress = true;
        message.adopt();
        _selectorThread._register(_transceiver.fd(), this, IceInternal.SocketStatus.NeedWrite, _endpoint.timeout());
        return false;
    }
    
    private void
    finishStart(Ice.LocalException ex)
    {
        //
        // We set _startCallback to null to break potential cyclic reference count 
        // and because the finalizer checks for  it to ensure that we always invoke
        // on the callback.
        //

        StartCallback callback = null;
        synchronized(this)
        {
            callback = _startCallback;
            _startCallback = null;
        }

        if(callback != null)
        {
            if(ex == null)
            {
                callback.connectionStartCompleted(this);
            }
            else
            {
                callback.connectionStartFailed(this, ex);
            }
        }
    }

    private void
    registerWithPool()
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.

        if(!_registeredWithPool)
        {
            _threadPool._register(_transceiver.fd(), this);
            _registeredWithPool = true;
        }
    }

    private void
    unregisterWithPool()
    {
        assert(!_threadPerConnection); // Only for use with a thread pool.

        if(_registeredWithPool)
        {
            _threadPool.unregister(_transceiver.fd());
            _registeredWithPool = false;        
            ++_finishedCount; // For each unregistration, finished() is called once.
        }
    }

    private IceInternal.BasicStream
    doCompress(IceInternal.BasicStream uncompressed, boolean compress)
    {
        if(_compressionSupported)
        {
            if(compress && uncompressed.size() >= 100)
            {
                //
                // Do compression.
                //
                IceInternal.BasicStream cstream = uncompressed.compress(IceInternal.Protocol.headerSize,
                                                                        _compressionLevel);
                if(cstream != null)
                {
                    //
                    // Set compression status.
                    //
                    cstream.pos(9);
                    cstream.writeByte((byte)2);

                    //
                    // Write the size of the compressed stream into the header.
                    //
                    cstream.pos(10);
                    cstream.writeInt(cstream.size());

                    //
                    // Write the compression status and size of the compressed stream into the header of the
                    // uncompressed stream -- we need this to trace requests correctly.
                    //
                    uncompressed.pos(9);
                    uncompressed.writeByte((byte)2);
                    uncompressed.writeInt(cstream.size());

                    return cstream;
                }
            }
        }

        uncompressed.pos(9);
        uncompressed.writeByte((byte)((_compressionSupported && compress) ? 1 : 0));

        //
        // Not compressed, fill in the message size.
        //
        uncompressed.pos(10);
        uncompressed.writeInt(uncompressed.size());

        return uncompressed;
    }

    private static class MessageInfo
    {
        MessageInfo(IceInternal.BasicStream stream)
        {
            this.stream = stream;
        }

        IceInternal.BasicStream stream;
        int invokeNum;
        int requestId;
        byte compress;
        IceInternal.ServantManager servantManager;
        ObjectAdapter adapter;
        IceInternal.OutgoingAsync outAsync;
    }

    private void
    parseMessage(MessageInfo info)
    {
        assert(_state > StateNotValidated && _state < StateClosed);

        if(_acmTimeout > 0)
        {
            _acmAbsoluteTimeoutMillis = IceInternal.Time.currentMonotonicTimeMillis() + _acmTimeout * 1000;
        }

        try
        {
            //
            // We don't need to check magic and version here. This has
            // already been done by the ThreadPool or ThreadPerConnection,
            // which provides us with the stream.
            //
            assert(info.stream.pos() == info.stream.size());
            info.stream.pos(8);
            byte messageType = info.stream.readByte();
            info.compress = info.stream.readByte();
            if(info.compress == (byte)2)
            {
                if(_compressionSupported)
                {
                    IceInternal.BasicStream ustream = info.stream.uncompress(IceInternal.Protocol.headerSize);
                    if(ustream != info.stream)
                    {
                        info.stream = ustream;
                    }
                }
                else
                {
                    FeatureNotSupportedException ex = new FeatureNotSupportedException();
                    ex.unsupportedFeature = "Cannot uncompress compressed message: "
                        + "org.apache.tools.bzip2.CBZip2OutputStream was not found";
                    throw ex;
                }
            }
            info.stream.pos(IceInternal.Protocol.headerSize);

            switch(messageType)
            {
                case IceInternal.Protocol.closeConnectionMsg:
                {
                    IceInternal.TraceUtil.traceRecv(info.stream, _logger, _traceLevels);
                    if(_endpoint.datagram())
                    {
                        if(_warn)
                        {
                            _logger.warning("ignoring close connection message for datagram connection:\n" + _desc);
                        }
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
                        IceInternal.TraceUtil.trace("received request during closing\n" +
                                                    "(ignored by server, client will retry)",
                                                    info.stream, _logger, _traceLevels);
                    }
                    else
                    {
                        IceInternal.TraceUtil.traceRecv(info.stream, _logger, _traceLevels);
                        info.requestId = info.stream.readInt();
                        info.invokeNum = 1;
                        info.servantManager = _servantManager;
                        info.adapter = _adapter;
                        ++_dispatchCount;
                    }
                    break;
                }

                case IceInternal.Protocol.requestBatchMsg:
                {
                    if(_state == StateClosing)
                    {
                        IceInternal.TraceUtil.trace("received batch request during closing\n" +
                                                    "(ignored by server, client will retry)",
                                                    info.stream, _logger, _traceLevels);
                    }
                    else
                    {
                        IceInternal.TraceUtil.traceRecv(info.stream, _logger, _traceLevels);
                        info.invokeNum = info.stream.readInt();
                        if(info.invokeNum < 0)
                        {
                            info.invokeNum = 0;
                            throw new NegativeSizeException();
                        }
                        info.servantManager = _servantManager;
                        info.adapter = _adapter;
                        _dispatchCount += info.invokeNum;
                    }
                    break;
                }

                case IceInternal.Protocol.replyMsg:
                {
                    IceInternal.TraceUtil.traceRecv(info.stream, _logger, _traceLevels);
                    info.requestId = info.stream.readInt();
                    IceInternal.Outgoing out = (IceInternal.Outgoing)_requests.remove(info.requestId);
                    if(out != null)
                    {
                        out.finished(info.stream);
                    }
                    else
                    {
                        info.outAsync = (IceInternal.OutgoingAsync)_asyncRequests.remove(info.requestId);
                        if(info.outAsync == null)
                        {
                            throw new UnknownRequestIdException();
                        }
                    }
                    break;
                }

                case IceInternal.Protocol.validateConnectionMsg:
                {
                    IceInternal.TraceUtil.traceRecv(info.stream, _logger, _traceLevels);
                    if(_warn)
                    {
                        _logger.warning("ignoring unexpected validate connection message:\n" + _desc);
                    }
                    break;
                }

                default:
                {
                    IceInternal.TraceUtil.trace("received unknown message\n(invalid, closing connection)", 
                                                info.stream, _logger, _traceLevels);
                    throw new UnknownMessageException();
                }
            }
        }
        catch(SocketException ex)
        {
            setState(StateClosed, ex);
        }
        catch(LocalException ex)
        {
            if(_endpoint.datagram())
            {
                if(_warn)
                {
                    _logger.warning("udp connection exception:\n" + ex + _desc);
                }
            }
            else
            {
                setState(StateClosed, ex);
            }
        }
    }

    private void
    invokeAll(IceInternal.BasicStream stream, int invokeNum, int requestId, byte compress,
              IceInternal.ServantManager servantManager, ObjectAdapter adapter)
    {
        //
        // Note: In contrast to other private or protected methods, this
        // operation must be called *without* the mutex locked.
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
                in = getIncoming(adapter, response, compress, requestId);
                IceInternal.BasicStream is = in.is();
                stream.swap(is);
                IceInternal.BasicStream os = in.os();
                
                //
                // Prepare the response if necessary.
                //
                if(response)
                {
                    assert(invokeNum == 1); // No further invocations if a response is expected.
                    os.writeBlob(IceInternal.Protocol.replyHdr);
                    
                    //
                    // Add the request ID.
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
            invokeException(ex, invokeNum);
        }
        catch(java.lang.AssertionError ex) // Upon assertion, we print the stack trace.
        {
            UnknownException uex = new UnknownException();
            java.io.StringWriter sw = new java.io.StringWriter();
            java.io.PrintWriter pw = new java.io.PrintWriter(sw);
            ex.printStackTrace(pw);
            pw.flush();
            uex.unknown = sw.toString();
            _logger.error(uex.unknown);
            invokeException(uex, invokeNum);
        }
        finally
        {
            if(in != null)
            {
                reclaimIncoming(in);
            }
        }
    }

    private void
    run()
    {
        try
        {
            //
            // Initialize the connection transceiver and validate the connection using
            // blocking operations.
            //

            IceInternal.SocketStatus status;

            status = initialize();
            assert(status == IceInternal.SocketStatus.Finished);

            status = validate();
            assert(status == IceInternal.SocketStatus.Finished);
        }
        catch(LocalException ex)
        {
            synchronized(this)
            {
                setState(StateClosed, ex);
                
                if(_transceiver != null)
                {
                    try
                    {
                        _transceiver.close();
                    }
                    catch(LocalException e)
                    {
                        // Here we ignore any exceptions in close().
                    }
                    
                    _transceiver = null;
                }
                notifyAll();
            }

            finishStart(_exception);
            return;
        }

        finishStart(null);

        boolean warnUdp = _instance.initializationData().properties.getPropertyAsInt("Ice.Warn.Datagrams") > 0;

        boolean closed = false;

        IceInternal.BasicStream stream = new IceInternal.BasicStream(_instance);
        while(!closed)
        {
            //
            // We must read new messages outside the thread synchronization because we use blocking reads.
            //

            try
            {
                try
                {
                    stream.resize(IceInternal.Protocol.headerSize, true);
                    stream.pos(0);
                    _transceiver.read(stream.getBuffer(), -1, _hasMoreData);

                    int pos = stream.pos();
                    if(pos < IceInternal.Protocol.headerSize)
                    {
                        //
                        // This situation is possible for small UDP packets.
                        //
                        throw new IllegalMessageSizeException();
                    }
                    stream.pos(0);
                    byte[] m = stream.readBlob(4);
                    if(m[0] != IceInternal.Protocol.magic[0] || m[1] != IceInternal.Protocol.magic[1] ||
                       m[2] != IceInternal.Protocol.magic[2] || m[3] != IceInternal.Protocol.magic[3])
                    {
                        BadMagicException ex = new BadMagicException();
                        ex.badMagic = m;
                        throw ex;
                    }
                    byte pMajor = stream.readByte();
                    byte pMinor = stream.readByte();
                    if(pMajor != IceInternal.Protocol.protocolMajor)
                    {
                        UnsupportedProtocolException e = new UnsupportedProtocolException();
                        e.badMajor = pMajor < 0 ? pMajor + 255 : pMajor;
                        e.badMinor = pMinor < 0 ? pMinor + 255 : pMinor;
                        e.major = IceInternal.Protocol.protocolMajor;
                        e.minor = IceInternal.Protocol.protocolMinor;
                        throw e;
                    }
                    byte eMajor = stream.readByte();
                    byte eMinor = stream.readByte();
                    if(eMajor != IceInternal.Protocol.encodingMajor)
                    {
                        UnsupportedEncodingException e = new UnsupportedEncodingException();
                        e.badMajor = eMajor < 0 ? eMajor + 255 : eMajor;
                        e.badMinor = eMinor < 0 ? eMinor + 255 : eMinor;
                        e.major = IceInternal.Protocol.encodingMajor;
                        e.minor = IceInternal.Protocol.encodingMinor;
                        throw e;
                    }
                    byte messageType = stream.readByte();
                    byte compress = stream.readByte();
                    int size = stream.readInt();
                    if(size < IceInternal.Protocol.headerSize)
                    {
                        throw new IllegalMessageSizeException();
                    }
                    if(size > _instance.messageSizeMax())
                    {
                        throw new MemoryLimitException();
                    }
                    if(size > stream.size())
                    {
                        stream.resize(size, true);
                    }
                    stream.pos(pos);

                    if(pos != stream.size())
                    {
                        if(_endpoint.datagram())
                        {
                            if(warnUdp)
                            {
                                _logger.warning("DatagramLimitException: maximum size of " + pos + " exceeded");
                            }
                            throw new DatagramLimitException();
                        }
                        else
                        {
                            _transceiver.read(stream.getBuffer(), -1, _hasMoreData);
                            assert(stream.pos() == stream.size());
                        }
                    }
                }
                catch(DatagramLimitException ex) // Expected.
                {
                    continue;
                }
                catch(SocketException ex)
                {
                    exception(ex);
                }
                catch(LocalException ex)
                {
                    if(_endpoint.datagram())
                    {
                        if(_warn)
                        {
                            _logger.warning("datagram connection exception:\n" + ex + "\n" + _desc);
                        }
                        continue;
                    }
                    else
                    {
                        exception(ex);
                    }
                }

                MessageInfo info = new MessageInfo(stream);

                LocalException localEx = null;

                synchronized(this)
                {
                    while(_state == StateHolding)
                    {
                        try
                        {
                            wait();
                        }
                        catch(InterruptedException ex)
                        {
                        }
                    }
                
                    if(_state != StateClosed)
                    {
                        parseMessage(info);
                    }

                    //
                    // parseMessage() can close the connection, so we must
                    // check for closed state again.
                    //
                    if(_state == StateClosed)
                    {
                        if(_sendInProgress)
                        {
                            _selectorThread.unregister(_transceiver.fd());
                        }
                                
                        //
                        // Prevent further writes.
                        //
                        _transceiver.shutdownWrite();

                        while(_sendInProgress)
                        {
                            try
                            {
                                wait();
                            }
                            catch(java.lang.Exception ex)
                            {
                            }
                        }

                        try
                        {
                            _transceiver.close();
                        }
                        catch(LocalException ex)
                        {
                            localEx = ex;
                        }
                        _transceiver = null;
                        notifyAll();

                        //
                        // We cannot simply return here. We have to make sure
                        // that all requests (regular and async) are notified
                        // about the closed connection below.
                        //
                        closed = true;
                    }
                }

                //
                // Asynchronous replies must be handled outside the thread
                // synchronization, so that nested calls are possible.
                //
                if(info.outAsync != null)
                {
                    info.outAsync.__finished(info.stream);
                }
                
                //
                // Method invocation (or multiple invocations for batch messages)
                // must be done outside the thread synchronization, so that nested
                // calls are possible.
                //
                invokeAll(info.stream, info.invokeNum, info.requestId, info.compress, info.servantManager, 
                          info.adapter);

                if(closed)
                {
                    java.util.Iterator i = _queuedStreams.iterator();
                    while(i.hasNext())
                    {
                        OutgoingMessage message = (OutgoingMessage)i.next();
                        message.finished(_exception);
                    }
                    _queuedStreams.clear();

                    i = _requests.entryIterator();
                    while(i.hasNext())
                    {
                        IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
                        IceInternal.Outgoing out = (IceInternal.Outgoing)e.getValue();
                        out.finished(_exception); // The exception is immutable at this point.
                    }
                    _requests.clear();

                    i = _asyncRequests.entryIterator();
                    while(i.hasNext())
                    {
                        IceInternal.IntMap.Entry e = (IceInternal.IntMap.Entry)i.next();
                        IceInternal.OutgoingAsync out = (IceInternal.OutgoingAsync)e.getValue();
                        out.__finished(_exception); // The exception is immutable at this point.
                    }
                    _asyncRequests.clear();
                }

                if(localEx != null)
                {
                    assert(closed);
                    throw localEx;
                }    
            }
            finally
            {
                stream.reset();
            }
        }
    }

    private void
    warning(String msg, Exception ex)
    {
        java.io.StringWriter sw = new java.io.StringWriter();
        java.io.PrintWriter pw = new java.io.PrintWriter(sw);
        ex.printStackTrace(pw);
        pw.flush();
        String s = msg + ":\n" + _desc + "\n" + sw.toString();
        _logger.warning(s);
    }

    private void
    error(String msg, Exception ex)
    {
        java.io.StringWriter sw = new java.io.StringWriter();
        java.io.PrintWriter pw = new java.io.PrintWriter(sw);
        ex.printStackTrace(pw);
        pw.flush();
        String s = msg + ":\n" + _desc + "\n" + sw.toString();
        _logger.error(s);
    }

    private IceInternal.Incoming
    getIncoming(ObjectAdapter adapter, boolean response, byte compress, int requestId)
    {
        IceInternal.Incoming in = null;

        if(_cacheBuffers)
        {
            synchronized(_incomingCacheMutex)
            {
                if(_incomingCache == null)
                {
                    in = new IceInternal.Incoming(_instance, this, adapter, response, compress, requestId);
                }
                else
                {
                    in = _incomingCache;
                    _incomingCache = _incomingCache.next;
                    in.reset(_instance, this, adapter, response, compress, requestId);
                    in.next = null;
                }
            }
        }
        else
        {
            in = new IceInternal.Incoming(_instance, this, adapter, response, compress, requestId);
        }

        return in;
    }

    private void
    reclaimIncoming(IceInternal.Incoming in)
    {
        if(_cacheBuffers)
        {
            synchronized(_incomingCacheMutex)
            {
                in.next = _incomingCache;
                _incomingCache = in;
                //
                // Clear references to Ice objects as soon as possible.
                //
                _incomingCache.reclaim();
            }
        }
    }

    public IceInternal.Outgoing
    getOutgoing(IceInternal.RequestHandler handler, String operation, OperationMode mode, java.util.Map context)
        throws IceInternal.LocalExceptionWrapper
    {
        IceInternal.Outgoing out = null;

        if(_cacheBuffers)
        {
            synchronized(_outgoingCacheMutex)
            {
                if(_outgoingCache == null)
                {
                    out = new IceInternal.Outgoing(handler, operation, mode, context);
                }
                else
                {
                    out = _outgoingCache;
                    _outgoingCache = _outgoingCache.next;
                    out.reset(handler, operation, mode, context);
                    out.next = null;
                }
            }
        }
        else
        {
            out = new IceInternal.Outgoing(handler, operation, mode, context);
        }

        return out;
    }

    public void
    reclaimOutgoing(IceInternal.Outgoing out)
    {
        if(_cacheBuffers)
        {
            //
            // Clear references to Ice objects as soon as possible.
            //
            out.reclaim();

            synchronized(_outgoingCacheMutex)
            {
                out.next = _outgoingCache;
                _outgoingCache = out;
            }
        }
    }

    private class ThreadPerConnection extends Thread
    {
        public void
        run()
        {
            if(ConnectionI.this._instance.initializationData().threadHook != null)
            {
                ConnectionI.this._instance.initializationData().threadHook.start();
            }
            
            try
            {
                ConnectionI.this.run();
            }
            catch(Exception ex)
            {
                ConnectionI.this.error("exception in thread per connection", ex);
            }
            finally
            {
                if(ConnectionI.this._instance.initializationData().threadHook != null)
                {
                    ConnectionI.this._instance.initializationData().threadHook.stop();
                }
            }
        }
    }

    private static class OutgoingMessage
    {
        OutgoingMessage(IceInternal.BasicStream stream, boolean compress, boolean adopt)
        {
            this.stream = stream;
            this.compress = compress;
            this.adopt = adopt;
        }
        
        OutgoingMessage(IceInternal.OutgoingMessageCallback out, IceInternal.BasicStream stream, boolean compress,
                        boolean resp)
        {
            this.stream = stream;
            this.compress = compress;
            this.out = out;
            this.response = resp;
        }
        
        OutgoingMessage(IceInternal.OutgoingAsyncMessageCallback out, IceInternal.BasicStream stream, boolean compress,
                        boolean resp)
        {
            this.stream = stream;
            this.compress = compress;
            this.outAsync = out;
            this.response = resp;
        }

        public void
        adopt()
        {
            if(adopt)
            {
                IceInternal.BasicStream stream = new IceInternal.BasicStream(this.stream.instance());
                stream.swap(this.stream);
                this.stream = stream;
                adopt = false;
            }
        }
        
        public void
        sent(ConnectionI connection, boolean notify)
        {
            if(out != null)
            {
                out.sent(notify); // true = notify the waiting thread that the request was sent.
            }
            else if(outAsync != null)
            {
                outAsync.__sent(connection);
            }
        }
        
        public void
        finished(Ice.LocalException ex)
        {
            //
            // Only notify oneway requests. The connection keeps track of twoway 
            // requests in the _requests/_asyncRequests maps and will notify them
            // of the connection exceptions.
            //
            if(!response)
            {
                if(out != null)
                {
                    out.finished(ex);
                }
                else if(outAsync != null)
                {
                    outAsync.__finished(ex);
                }
            }
        }

        public IceInternal.BasicStream stream;
        public IceInternal.OutgoingMessageCallback out;
        public IceInternal.OutgoingAsyncMessageCallback outAsync;
        public boolean compress;
        public boolean response;
        boolean adopt;
        boolean prepared;
    }

    private Thread _thread;
    private final boolean _threadPerConnection;

    private IceInternal.Transceiver _transceiver;
    private Ice.BooleanHolder _hasMoreData = new Ice.BooleanHolder(false);

    private String _desc;
    private final String _type;
    private final IceInternal.EndpointI _endpoint;

    private ObjectAdapter _adapter;
    private IceInternal.ServantManager _servantManager;

    private final Logger _logger;
    private final IceInternal.TraceLevels _traceLevels;

    private boolean _registeredWithPool;
    private int _finishedCount;
    private final IceInternal.ThreadPool _threadPool;
    private final IceInternal.SelectorThread _selectorThread;

    private StartCallback _startCallback = null;

    private final boolean _warn;

    private final int _acmTimeout;
    private long _acmAbsoluteTimeoutMillis;

    private final int _compressionLevel;

    private int _nextRequestId;

    private IceInternal.IntMap _requests = new IceInternal.IntMap();
    private IceInternal.IntMap _asyncRequests = new IceInternal.IntMap();

    private LocalException _exception;

    private boolean _batchAutoFlush;
    private IceInternal.BasicStream _batchStream;
    private boolean _batchStreamInUse;
    private int _batchRequestNum;
    private boolean _batchRequestCompress;
    private int _batchMarker;

    private java.util.LinkedList _queuedStreams = new java.util.LinkedList();
    private java.util.LinkedList _sendStreams = new java.util.LinkedList();
    private boolean _sendInProgress;
    private int _waitingForSend;

    private int _dispatchCount;

    private int _state; // The current state.
    private long _stateTime; // The last time when the state was changed.

    private IceInternal.Incoming _incomingCache;
    private java.lang.Object _incomingCacheMutex = new java.lang.Object();

    private IceInternal.Outgoing _outgoingCache;
    private java.lang.Object _outgoingCacheMutex = new java.lang.Object();

    private static boolean _compressionSupported = IceInternal.BasicStream.compressible();

    private boolean _overrideCompress;
    private boolean _overrideCompressValue;
    private boolean _cacheBuffers;
}
