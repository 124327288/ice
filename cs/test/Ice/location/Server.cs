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

public class Server
{
    private static int run(string[] args, Ice.Communicator communicator)
    {
        //
        // Register the server manager. The server manager creates a new
        // 'server' (a server isn't a different process, it's just a new
        // communicator and object adapter).
        //
        Ice.Properties properties = communicator.getProperties();
        properties.setProperty("ServerManagerAdapter.Endpoints", "default -p 12345");
        
        Ice.ObjectAdapter adapter = communicator.createObjectAdapter("ServerManagerAdapter");
        
        Ice.Object object_Renamed = new ServerManagerI(adapter);
        adapter.add(object_Renamed, Ice.Util.stringToIdentity("ServerManager"));
        
        //
        // We also register a sample server locator which implements the
        // locator interface, this locator is used by the clients and the
        // 'servers' created with the server manager interface.
        //
        ServerLocatorRegistry registry = new ServerLocatorRegistry();
        registry.addObject(adapter.createProxy(Ice.Util.stringToIdentity("ServerManager")));
        registry.addObject(communicator.stringToProxy("test@TestAdapter"));
        Ice.LocatorRegistryPrx registryPrx = Ice.LocatorRegistryPrxHelper.uncheckedCast(
						adapter.add(registry, Ice.Util.stringToIdentity("registry")));
        
        ServerLocator locator = new ServerLocator(registry, registryPrx);
        adapter.add(locator, Ice.Util.stringToIdentity("locator"));
        
        adapter.activate();
        communicator.waitForShutdown();
        
        return 0;
    }
    
    public static void Main(string[] args)
    {
        int status = 0;
        Ice.Communicator communicator = null;
        
        try
        {
            communicator = Ice.Util.initialize(ref args);
            status = run(args, communicator);
        }
        catch(Ice.LocalException ex)
        {
            System.Console.Error.WriteLine(ex);
            status = 1;
        }
        
        if(communicator != null)
        {
            try
            {
                communicator.destroy();
            }
            catch(Ice.LocalException ex)
            {
                System.Console.Error.WriteLine(ex);
                status = 1;
            }
        }
        
        System.Environment.Exit(status);
    }
}
