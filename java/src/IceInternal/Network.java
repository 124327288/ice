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


package IceInternal;

public final class Network
{
    public static boolean
    connectionLost(java.io.IOException ex)
    {
        //
        // TODO: The JDK raises a generic IOException for certain
        // cases of connection loss. Unfortunately, our only choice is
        // to search the exception message for distinguishing phrases.
        //

        String msg = ex.getMessage().toLowerCase();

        if(msg != null)
        {
            final String[] msgs =
            {
                "connection reset by peer", // ECONNRESET
                "cannot send after socket shutdown", // ESHUTDOWN (Win32)
                "cannot send after transport endpoint shutdown", // ESHUTDOWN (Linux)
                "software caused connection abort", // ECONNABORTED
                "an existing connection was forcibly closed" // unknown
            };

            for(int i = 0; i < msgs.length; i++)
            {
                if(msg.indexOf(msgs[i]) != -1)
                {
                    return true;
                }
            }
        }

        return false;
    }

    public static boolean
    connectionRefused(java.net.ConnectException ex)
    {
        //
        // The JDK raises a generic ConnectException when the server
        // actively refuses a connection. Unfortunately, our only
        // choice is to search the exception message for
        // distinguishing phrases.
        //

        String msg = ex.getMessage().toLowerCase();

        if(msg != null)
        {
            final String[] msgs =
            {
                "connection refused" // ECONNREFUSED
            };

            for(int i = 0; i < msgs.length; i++)
            {
                if(msg.indexOf(msgs[i]) != -1)
                {
                    return true;
                }
            }
        }

        return false;
    }

    public static java.nio.channels.SocketChannel
    createTcpSocket()
    {
        try
        {
            java.nio.channels.SocketChannel fd = java.nio.channels.SocketChannel.open();
            java.net.Socket socket = fd.socket();
            socket.setTcpNoDelay(true);
            socket.setKeepAlive(true);
            return fd;
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static java.nio.channels.ServerSocketChannel
    createTcpServerSocket()
    {
        try
        {
            java.nio.channels.ServerSocketChannel fd = java.nio.channels.ServerSocketChannel.open();
            //
            // It's not possible to set TCP_NODELAY or KEEP_ALIVE
            // on a server socket in Java
            //
            //java.net.Socket socket = fd.socket();
            //socket.setTcpNoDelay(true);
            //socket.setKeepAlive(true);
            return fd;
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static java.nio.channels.DatagramChannel
    createUdpSocket()
    {
        try
        {
            return java.nio.channels.DatagramChannel.open();
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static void
    setBlock(java.nio.channels.SelectableChannel fd, boolean block)
    {
        try
        {
            fd.configureBlocking(block);
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static java.net.InetSocketAddress
    doBind(java.nio.channels.ServerSocketChannel fd, java.net.InetSocketAddress addr)
    {
        try
        {
            java.net.ServerSocket sock = fd.socket();
            sock.bind(addr);
            return (java.net.InetSocketAddress)sock.getLocalSocketAddress();
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static java.net.InetSocketAddress
    doBind(java.nio.channels.DatagramChannel fd, java.net.InetSocketAddress addr)
    {
        try
        {
            java.net.DatagramSocket sock = fd.socket();
            sock.bind(addr);
            return (java.net.InetSocketAddress)sock.getLocalSocketAddress();
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static void
    doConnect(java.nio.channels.SocketChannel fd, java.net.InetSocketAddress addr, int timeout)
    {
        try
        {
            if(!fd.connect(addr))
            {
                int delay;
                if(timeout > 0 && timeout < 100)
                {
                    delay = timeout;
                }
                else
                {
                    delay = 100; // 100 ms
                }

                int timer = 0;
                while(!fd.finishConnect())
                {
                    if(timeout > 0 && timer >= timeout)
                    {
                        fd.close();
                        throw new Ice.ConnectTimeoutException();
                    }
                    try
                    {
                        Thread.sleep(delay);
                        timer += delay;
                    }
                    catch(InterruptedException ex)
                    {
                    }
                }
            }
        }
        catch(java.net.ConnectException ex)
        {
            try
            {
                fd.close();
            }
            catch(java.io.IOException e)
            {
                // ignore
            }

            Ice.ConnectFailedException se;
	    if(connectionRefused(ex))
	    {
		se = new Ice.ConnectionRefusedException();
	    }
	    else
	    {
		se = new Ice.ConnectFailedException();
	    }
            se.initCause(ex);
            throw se;
        }
        catch(java.io.IOException ex)
        {
            try
            {
                fd.close();
            }
            catch(java.io.IOException e)
            {
                // ignore
            }
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static void
    doConnect(java.nio.channels.DatagramChannel fd, java.net.InetSocketAddress addr, int timeout)
    {
        try
        {
            fd.connect(addr);
        }
        catch(java.net.ConnectException ex)
        {
            try
            {
                fd.close();
            }
            catch(java.io.IOException e)
            {
                // ignore
            }

            Ice.ConnectFailedException se;
	    if(connectionRefused(ex))
	    {
		se = new Ice.ConnectionRefusedException();
	    }
	    else
	    {
		se = new Ice.ConnectFailedException();
	    }
            se.initCause(ex);
            throw se;
        }
        catch(java.io.IOException ex)
        {
            try
            {
                fd.close();
            }
            catch(java.io.IOException e)
            {
                // ignore
            }
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static java.nio.channels.SocketChannel
    doAccept(java.nio.channels.ServerSocketChannel fd, int timeout)
    {
        java.nio.channels.SocketChannel result = null;
        while(result == null)
        {
            try
            {
                result = fd.accept();
                if(result == null)
                {
                    java.nio.channels.Selector selector = java.nio.channels.Selector.open();

                    try
                    {
                        while(true)
                        {
                            try
                            {
                                java.nio.channels.SelectionKey key =
                                    fd.register(selector, java.nio.channels.SelectionKey.OP_ACCEPT);
                                int n;
                                if(timeout > 0)
                                {
                                    n = selector.select(timeout);
                                }
                                else if(timeout == 0)
                                {
                                    n = selector.selectNow();
                                }
                                else
                                {
                                    n = selector.select();
                                }

                                if(n == 0)
                                {
                                    throw new Ice.TimeoutException();
                                }

                                break;
                            }
                            catch(java.io.InterruptedIOException ex)
                            {
                                continue;
                            }
                            catch(java.io.IOException ex)
                            {
                                Ice.SocketException se = new Ice.SocketException();
                                se.initCause(ex);
                                throw se;
                            }
                        }
                    }
                    finally
                    {
                        try
                        {
                            selector.close();
                        }
                        catch(java.io.IOException ex)
                        {
                            // Ignore
                        }
                    }
                }
            }
            catch(java.io.InterruptedIOException ex)
            {
                continue;
            }
            catch(java.io.IOException ex)
            {
                Ice.SocketException se = new Ice.SocketException();
                se.initCause(ex);
                throw se;
            }
        }

        try
        {
            java.net.Socket socket = result.socket();
            socket.setTcpNoDelay(true);
            socket.setKeepAlive(true);
        }
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }

        return result;
    }

    public static void
    setSendBufferSize(java.nio.channels.DatagramChannel fd, int size)
    {
	try
	{
	    java.net.DatagramSocket socket = fd.socket();
	    socket.setSendBufferSize(size);
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static int
    getSendBufferSize(java.nio.channels.DatagramChannel fd)
    {
	int size;
	try
	{
	    java.net.DatagramSocket socket = fd.socket();
	    size = socket.getSendBufferSize();
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
	return size;
    }

    public static void
    setRecvBufferSize(java.nio.channels.ServerSocketChannel fd, int size)
    {
    	try
	{
	    java.net.ServerSocket socket = fd.socket();
	    socket.setReceiveBufferSize(size);
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static void
    setRecvBufferSize(java.nio.channels.DatagramChannel fd, int size)
    {
    	try
	{
	    java.net.DatagramSocket socket = fd.socket();
	    socket.setReceiveBufferSize(size);
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
    }

    public static int
    getRecvBufferSize(java.nio.channels.ServerSocketChannel fd)
    {
        int size;
	try
	{
	    java.net.ServerSocket socket = fd.socket();
	    size = socket.getReceiveBufferSize();
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
	return size;
    }

    public static int
    getRecvBufferSize(java.nio.channels.DatagramChannel fd)
    {
        int size;
	try
	{
	    java.net.DatagramSocket socket = fd.socket();
	    size = socket.getReceiveBufferSize();
	}
        catch(java.io.IOException ex)
        {
            Ice.SocketException se = new Ice.SocketException();
            se.initCause(ex);
            throw se;
        }
	return size;
    }

    public static java.net.InetSocketAddress
    getAddress(String host, int port)
    {
        try
        {
            java.net.InetAddress addr = java.net.InetAddress.getByName(host);
            return new java.net.InetSocketAddress(addr, port);
        }
        catch(java.net.UnknownHostException ex)
        {
	    Ice.DNSException e = new Ice.DNSException();
	    e.host = host;
	    throw e;
        }
    }

    public static String
    getLocalHost(boolean numeric)
    {
        java.net.InetAddress addr = getLocalAddress();

        return numeric ? addr.getHostAddress() : addr.getHostName();
    }

    public static java.net.InetAddress
    getLocalAddress()
    {
        java.net.InetAddress addr = null;

        try
        {
            addr = java.net.InetAddress.getLocalHost();
        }
        catch(java.net.UnknownHostException ex)
        {
            //
            // May be raised on DHCP systems.
            //
        }
        catch(NullPointerException ex)
        {
            //
            // Workaround for bug in JDK.
            //
        }

        if(addr == null)
        {
            //
            // Iterate over the network interfaces and pick an IP
            // address (preferably not the loopback address).
            //
            java.net.InetAddress loopback = null;
            try
            {
                java.util.Enumeration ni = java.net.NetworkInterface.getNetworkInterfaces();
                while(addr == null && ni.hasMoreElements())
                {
                    java.net.NetworkInterface i = (java.net.NetworkInterface)ni.nextElement();
                    java.util.Enumeration addrs = i.getInetAddresses();
                    while(addr == null && addrs.hasMoreElements())
                    {
                        java.net.InetAddress a = (java.net.InetAddress)addrs.nextElement();
                        if(!a.isLoopbackAddress())
                        {
                            addr = a;
                        }
                        else
                        {
                            loopback = a;
                        }
                    }
                }
            }
            catch(java.net.SocketException e)
            {
                Ice.SocketException se = new Ice.SocketException();
                se.initCause(e);
                throw se;
            }

            if(addr == null)
            {
                addr = loopback; // Use the loopback address as the last resort.
            }
        }

        assert(addr != null);
        return addr;
    }

    public static final class SocketPair
    {
        public java.nio.channels.spi.AbstractSelectableChannel source;
        public java.nio.channels.WritableByteChannel sink;
    }

    public static SocketPair
    createPipe()
    {
        SocketPair fds = new SocketPair();

        //
        // TODO: This method should really be very simple. Unfortunately,
        // there's a bug in the Win32 JDK (#4494292) which prevents the
        // Selector from properly detecting input on a Pipe's source
        // channel, so we resort to creating a socket pair. This bug has
        // supposedly been fixed for JDK 1.4.1.
        //
        //java.nio.channels.Pipe pipe = java.nio.channels.Pipe.open();
        //fds.sink = pipe.sink();
        //fds.source = pipe.source();
        //

        java.nio.channels.ServerSocketChannel fd = createTcpServerSocket();

        java.net.InetSocketAddress addr = new java.net.InetSocketAddress("127.0.0.1", 0);

        addr = doBind(fd, addr);

        try
        {
            java.nio.channels.SocketChannel sink = createTcpSocket();
            fds.sink = sink;
            try
            {
                doConnect(sink, addr, -1);
                fds.source = doAccept(fd, -1);
            }
            catch(Ice.LocalException ex)
            {
                try
                {
                    fds.sink.close();
                }
                catch(java.io.IOException e)
                {
                }
                throw ex;
            }
        }
        finally
        {
            try
            {
                fd.close();
            }
            catch(java.io.IOException ex)
            {
            }
        }

        return fds;
    }

    public static String
    fdToString(java.nio.channels.SelectableChannel fd)
    {
        if(fd == null)
        {
            return "<closed>";
        }

        java.net.InetAddress localAddr =  null, remoteAddr = null;
        int localPort = -1, remotePort = -1;

        if(fd instanceof java.nio.channels.SocketChannel)
        {
            java.nio.channels.SocketChannel socketChannel = (java.nio.channels.SocketChannel)fd;
            java.net.Socket socket = socketChannel.socket();
            localAddr = socket.getLocalAddress();
            localPort = socket.getLocalPort();
            remoteAddr = socket.getInetAddress();
            remotePort = socket.getPort();
        }
        else if(fd instanceof java.nio.channels.DatagramChannel)
        {
            java.nio.channels.DatagramChannel datagramChannel = (java.nio.channels.DatagramChannel)fd;
            java.net.DatagramSocket socket = datagramChannel.socket();
            localAddr = socket.getLocalAddress();
            localPort = socket.getLocalPort();
            remoteAddr = socket.getInetAddress();
            remotePort = socket.getPort();
        }
        else
        {
            assert(false);
        }

        StringBuffer s = new StringBuffer();
        s.append("local address = ");
        s.append(localAddr.getHostAddress());
        s.append(':');
        s.append(localPort);
        if(remoteAddr == null)
        {
            s.append("\nremote address = <not connected>");
        }
        else
        {
            s.append("\nremote address = ");
            s.append(remoteAddr.getHostAddress());
            s.append(':');
            s.append(remotePort);
        }

        return s.toString();
    }

    public static String
    addrToString(java.net.InetSocketAddress addr)
    {
        StringBuffer s = new StringBuffer();
        s.append(addr.getAddress().getHostAddress());
        s.append(':');
        s.append(addr.getPort());
        return s.toString();
    }
}
