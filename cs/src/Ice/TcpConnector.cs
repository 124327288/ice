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


namespace IceInternal
{

    using System.Net;
    using System.Net.Sockets;

    sealed class TcpConnector : Connector
    {
	public Transceiver connect(int timeout)
	{
	    if(_traceLevels.network >= 2)
	    {
		string s = "trying to establish tcp connection to " + ToString();
		_logger.trace(_traceLevels.networkCat, s);
	    }
	    
	    Socket fd = Network.createSocket(false);
	    Network.setBlock(fd, false);
	    Network.doConnect(fd, _addr, timeout);
	    
	    if(_traceLevels.network >= 1)
	    {
		string s = "tcp connection established\n" + Network.fdToString(fd);
		_logger.trace(_traceLevels.networkCat, s);
	    }
	    
	    return new TcpTransceiver(_instance, fd);
	}
	
	public override string ToString()
	{
	    return Network.addrToString(_addr);
	}
	
	//
	// Only for use by TcpEndpoint
	//
	internal TcpConnector(Instance instance, string host, int port)
	{
	    _instance = instance;
	    _traceLevels = instance.traceLevels();
	    _logger = instance.logger();
	    
	    _addr = Network.getAddress(host, port);
	}
	
	private Instance _instance;
	private TraceLevels _traceLevels;
	private Ice.Logger _logger;
	private IPEndPoint _addr;
    }

}
