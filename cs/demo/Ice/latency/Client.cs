// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************


using System;

public class Client
{
    private static int run(string[] args, Ice.Communicator communicator)
    {
        Ice.Properties properties = communicator.getProperties();
        string refProperty = "Latency.Ping";
        string @ref = properties.getProperty(refProperty);
        if(@ref.Length == 0)
        {
            Console.Error.WriteLine("property `" + refProperty + "' not set");
            return 1;
        }
        
        Ice.ObjectPrx @base = communicator.stringToProxy(@ref);
        PingPrx ping = PingPrxHelper.checkedCast(@base);
        if(ping == null)
        {
            Console.Error.WriteLine("invalid proxy");
            return 1;
        }
        
        // Initial ping to setup the connection.
        ping.ice_ping();
        
        long tv1 = (System.DateTime.Now.Ticks - 621355968000000000) / 10000;
        int repetitions = 100000;
        Console.Out.WriteLine("pinging server " + repetitions + " times (this may take a while)");
        for (int i = 0; i < repetitions; i++)
        {
            ping.ice_ping();
        }
        
        long tv2 = (System.DateTime.Now.Ticks - 621355968000000000) / 10000;
        double total = (double)(tv2 - tv1);
        double perPing = total / repetitions;
        
        Console.Out.WriteLine("time for " + repetitions + " pings: " + total + "ms");
        Console.Out.WriteLine("time per ping: " + perPing + "ms");
        
        return 0;
    }
    
    public static void Main(string[] args)
    {
        int status = 0;
        Ice.Communicator communicator = null;
        
        try
        {
            Ice.Properties properties = Ice.Util.createProperties();
            properties.load("config");
            communicator = Ice.Util.initializeWithProperties(ref args, properties);
            status = run(args, communicator);
        }
        catch(System.Exception ex)
        {
	    Console.Error.WriteLine(ex);
            status = 1;
        }
        
	if(communicator != null)
	{
	    try
	    {
		communicator.destroy();
	    }
	    catch(System.Exception ex)
	    {
		Console.Error.WriteLine(ex);
		status = 1;
	    }
	}
        
        System.Environment.Exit(status);
    }
}
