// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

package Ice;

//
// The default logger for MIDP application simply stubs out the calls to the logger interface. MIDP apps generally
// don't have a stderr or stdout to write to.
//

public final class LoggerI extends LocalObjectImpl implements Logger
{
    public 
    LoggerI(String prefix)
    {
	if(prefix.length() > 0)
	{
	    _prefix = prefix + ": ";
	}
	/*
	String logURL = "socket://142.163.163.194:33456";
	try
	{
	    javax.microedition.io.SocketConnection s = 
		(javax.microedition.io.SocketConnection)javax.microedition.io.Connector.open(logURL);
	    
	    s.setSocketOption(javax.microedition.io.SocketConnection.DELAY, 0);
	    s.setSocketOption(javax.microedition.io.SocketConnection.LINGER, 0);
	    s.setSocketOption(javax.microedition.io.SocketConnection.KEEPALIVE, 0);
	    
	    _out = new java.io.PrintStream(s.openOutputStream());
	}
	catch(java.io.IOException ex)
	{
	    // Ignore.
	}
	*/

        _date = java.text.DateFormat.getDateInstance(java.text.DateFormat.SHORT);
        _time = new java.text.SimpleDateFormat(" HH:mm:ss:SSS");
    }

    public void
    print(String message)
    {
	synchronized(_globalMutex)
	{
	    _out.println(message);
	    _out.flush();
	}
    }

    public void
    trace(String category, String message)
    {
        StringBuffer s = new StringBuffer("[ ");
        s.append(_date.format(new java.util.Date()));
        s.append(_time.format(new java.util.Date()));
	s.append(_prefix);
	s.append(category);
	s.append(": ");
        s.append(message);
        s.append(" ]");
        int idx = 0;
        while((idx = s.indexOf("\n", idx)) != -1)
        {
            s.insert(idx + 1, "  ");
            ++idx;
        }

	synchronized(_globalMutex)
        {
            _out.println(s.toString());
	    _out.flush();
	}
    }

    public void
    warning(String message)
    {
	StringBuffer s = new StringBuffer();
        s.append(_date.format(new java.util.Date()));
        s.append(_time.format(new java.util.Date()));
	s.append(_prefix);
	s.append("warning: ");
	s.append(message);

	synchronized(_globalMutex)
	{
	    _out.println(s.toString());
	    _out.flush();
	}
    }

    public void
    error(String message)
    {
	StringBuffer s = new StringBuffer();
        s.append(_date.format(new java.util.Date()));
        s.append(_time.format(new java.util.Date()));
	s.append(_prefix);
	s.append("error: ");
	s.append(message);

	synchronized(_globalMutex)
	{
	    _out.println(s.toString());
	    _out.flush();
	}
    }

    protected
    LoggerI(LoggerI source)
    {
	super(source);
	_prefix = source._prefix;
	_date = source._date;
	_time = source._time;
	_out = source._out;
    }
    
    public java.lang.Object
    ice_clone()
    {
	return new LoggerI(this);
    }

    String _prefix = "";
    static java.lang.Object _globalMutex = new java.lang.Object();
    java.text.DateFormat _date;
    java.text.SimpleDateFormat _time;
    java.io.PrintStream _out = System.err;
}
