// **********************************************************************
//
// Copyright (c) 2001
// MutableRealms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

package IceInternal;

final class TcpEndpoint implements Endpoint
{
    final static short TYPE = 1;

    public
    TcpEndpoint(Instance instance, String ho, int po, int ti)
    {
        _instance = instance;
        _host = ho;
        _port = po;
        _timeout = ti;
        calcHashValue();
    }

    public
    TcpEndpoint(Instance instance, String str)
    {
        _instance = instance;
        _host = null;
        _port = 0;
        _timeout = -1;

        String[] arr = str.split("[ \t\n\r]+");

        int i = 0;
        while (i < arr.length)
        {
            if (arr[i].length() == 0)
            {
                i++;
                continue;
            }

            String option = arr[i++];
            if (option.length() != 2 || option.charAt(0) != '-')
            {
                throw new Ice.EndpointParseException();
            }

            String argument = null;
            if (i < arr.length && arr[i].charAt(0) != '-')
            {
                argument = arr[i++];
            }

            switch (option.charAt(1))
            {
                case 'h':
                {
                    if (argument == null)
                    {
                        throw new Ice.EndpointParseException();
                    }

                    _host = argument;
                    break;
                }

                case 'p':
                {
                    if (argument == null)
                    {
                        throw new Ice.EndpointParseException();
                    }

                    try
                    {
                        _port = Integer.parseInt(argument);
                    }
                    catch (NumberFormatException ex)
                    {
                        throw new Ice.EndpointParseException();
                    }

                    break;
                }

                case 't':
                {
                    if (argument == null)
                    {
                        throw new Ice.EndpointParseException();
                    }

                    try
                    {
                        _timeout = Integer.parseInt(argument);
                    }
                    catch (NumberFormatException ex)
                    {
                        throw new Ice.EndpointParseException();
                    }

                    break;
                }

                default:
                {
                    throw new Ice.EndpointParseException();
                }
            }
        }

        if (_host == null)
        {
            _host = _instance.defaultsAndOverwrites().defaultHost;
        }

        calcHashValue();
    }

    public
    TcpEndpoint(BasicStream s)
    {
        _instance = s.instance();
        s.startReadEncaps();
        _host = s.readString();
        _port = s.readInt();
        _timeout = s.readInt();
        s.endReadEncaps();
        calcHashValue();
    }

    //
    // Marshal the endpoint
    //
    public void
    streamWrite(BasicStream s)
    {
        s.writeShort(TYPE);
        s.startWriteEncaps();
        s.writeString(_host);
        s.writeInt(_port);
        s.writeInt(_timeout);
        s.endWriteEncaps();
    }

    //
    // Convert the endpoint to its string form
    //
    public String
    toString()
    {
        String s = "tcp -h " + _host + " -p " + _port;
        if (_timeout != -1)
        {
            s += " -t " + _timeout;
        }
        return s;
    }

    //
    // Return the endpoint type
    //
    public short
    type()
    {
        return TYPE;
    }

    //
    // Return the timeout for the endpoint in milliseconds. 0 means
    // non-blocking, -1 means no timeout.
    //
    public int
    timeout()
    {
        return _timeout;
    }

    //
    // Return a new endpoint with a different timeout value, provided
    // that timeouts are supported by the endpoint. Otherwise the same
    // endpoint is returned.
    //
    public Endpoint
    timeout(int timeout)
    {
        if (timeout == _timeout)
        {
            return this;
        }
        else
        {
            return new TcpEndpoint(_instance, _host, _port, timeout);
        }
    }

    //
    // Return true if the endpoint is datagram-based.
    //
    public boolean
    datagram()
    {
        return false;
    }

    //
    // Return true if the endpoint is secure.
    //
    public boolean
    secure()
    {
        return false;
    }

    //
    // Return true if the endpoint type is unknown.
    //
    public boolean
    unknown()
    {
        return false;
    }

    //
    // Return a client side transceiver for this endpoint, or null if a
    // transceiver can only be created by a connector.
    //
    public Transceiver
    clientTransceiver()
    {
        return null;
    }

    //
    // Return a server side transceiver for this endpoint, or null if a
    // transceiver can only be created by an acceptor. In case a
    // transceiver is created, this operation also returns a new
    // "effective" endpoint, which might differ from this endpoint,
    // for example, if a dynamic port number is assigned.
    //
    public Transceiver
    serverTransceiver(EndpointHolder endpoint)
    {
        endpoint.value = this;
        return null;
    }

    //
    // Return a connector for this endpoint, or null if no connector
    // is available.
    //
    public Connector
    connector()
    {
        return new TcpConnector(_instance, _host, _port);
    }

    //
    // Return an acceptor for this endpoint, or null if no acceptors
    // is available. In case an acceptor is created, this operation
    // also returns a new "effective" endpoint, which might differ
    // from this endpoint, for example, if a dynamic port number is
    // assigned.
    //
    public Acceptor
    acceptor(EndpointHolder endpoint)
    {
        TcpAcceptor p = new TcpAcceptor(_instance, _host, _port);
        endpoint.value = new TcpEndpoint(_instance, _host, p.effectivePort(), _timeout);
        return p;
    }

    //
    // Check whether the endpoint is equivalent to a specific
    // Transceiver or Acceptor
    //
    public boolean
    equivalent(Transceiver transceiver)
    {
        return false;
    }

    public boolean
    equivalent(Acceptor acceptor)
    {
        TcpAcceptor tcpAcceptor = null;
        try
        {
            tcpAcceptor = (TcpAcceptor)acceptor;
        }
        catch (ClassCastException ex)
        {
            return false;
        }
        return tcpAcceptor.equivalent(_host, _port);
    }

    public int
    hashCode()
    {
        return _hashCode;
    }

    //
    // Compare endpoints for sorting purposes
    //
    public boolean
    equals(java.lang.Object obj)
    {
        return compareTo(obj) == 0;
    }

    public int
    compareTo(java.lang.Object obj) // From java.lang.Comparable
    {
        TcpEndpoint p = null;

        try
        {
            p = (TcpEndpoint)obj;
        }
        catch (ClassCastException ex)
        {
            return 1;
        }

        if (this == p)
        {
            return 0;
        }

        if (_port < p._port)
        {
            return -1;
        }
        else if (p._port < _port)
        {
            return 1;
        }

        if (_timeout < p._timeout)
        {
            return -1;
        }
        else if (p._timeout < _timeout)
        {
            return 1;
        }

        if (!_host.equals(p._host))
        {
            //
            // We do the most time-consuming part of the comparison last.
            //
            java.net.InetSocketAddress laddr;
            java.net.InetSocketAddress raddr;
            laddr = Network.getAddress(_host, _port);
            raddr = Network.getAddress(p._host, p._port);
            byte[] larr = laddr.getAddress().getAddress();
            byte[] rarr = raddr.getAddress().getAddress();
            assert(larr.length == rarr.length);
            for (int i = 0; i < larr.length; i++)
            {
                if (larr[i] < rarr[i])
                {
                    return -1;
                }
                else if (rarr[i] < larr[i])
                {
                    return 1;
                }
            }
        }

        return 0;
    }

    private void
    calcHashValue()
    {
        _hashCode = _host.hashCode();
        _hashCode = 5 * _hashCode + _port;
        _hashCode = 5 * _hashCode + _timeout;
    }

    private Instance _instance;
    private String _host;
    private int _port;
    private int _timeout;
    private int _hashCode;
}
