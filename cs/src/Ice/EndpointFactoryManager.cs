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

namespace IceInternal
{

using System.Collections;
using System.Diagnostics;
using System.Text.RegularExpressions;

public sealed class EndpointFactoryManager
{
    internal
    EndpointFactoryManager(Instance instance)
    {
	_instance = instance;
	_factories = new ArrayList();
    }
    
    public void
    add(EndpointFactory factory)
    {
	lock(this)
	{
	    for(int i = 0; i < _factories.Count; i++)
	    {
		EndpointFactory f = (EndpointFactory)_factories[i];
		if(f.type() == factory.type())
		{
		    Debug.Assert(false);
		}
	    }
	    _factories.Add(factory);
	}
    }
    
    public EndpointFactory
    get(short type)
    {
	lock(this)
	{
	    for(int i = 0; i < _factories.Count; i++)
	    {
		EndpointFactory f = (EndpointFactory)_factories[i];
		if(f.type() == type)
		{
		    return f;
		}
	    }
	    return null;
	}
    }
    
    public Endpoint
    create(string str)
    {
	lock(this)
	{
	    string s = str.Trim();
	    if(s.Length == 0)
	    {
		Ice.EndpointParseException e = new Ice.EndpointParseException();
		e.str = str;
		throw e;
	    }
	    
	    Regex p = new Regex("([ \t\n\r]+)|$");
	    Match m = p.Match(s);
	    Debug.Assert(m.Success);
	    
	    string protocol = s.Substring(0, m.Index);
	    
	    if(protocol.Equals("default"))
	    {
		protocol = _instance.defaultsAndOverrides().defaultProtocol;
	    }
	    
	    for(int i = 0; i < _factories.Count; i++)
	    {
		EndpointFactory f = (EndpointFactory)_factories[i];
		if(f.protocol().Equals(protocol))
		{
		    return f.create(s.Substring(m.Index + m.Length));
		}
	    }
	    
	    Ice.EndpointParseException e2 = new Ice.EndpointParseException();
	    e2.str = str;
	    throw e2;
	}
    }
    
    public Endpoint
    read(BasicStream s)
    {
	lock(this)
	{
	    short type = s.readShort();
	    
	    for(int i = 0; i < _factories.Count; i++)
	    {
		EndpointFactory f = (EndpointFactory)_factories[i];
		if(f.type() == type)
		{
		    return f.read(s);
		}
	    }
	    
	    return new UnknownEndpoint(type, s);
	}
    }
    
    internal void
    destroy()
    {
	for(int i = 0; i < _factories.Count; i++)
	{
	    EndpointFactory f = (EndpointFactory)_factories[i];
	    f.destroy();
	}
	_factories.Clear();
    }
    
    private readonly Instance _instance;
    private readonly ArrayList _factories;
}

}
