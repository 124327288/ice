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

public final class EndpointFactoryManager
{
    EndpointFactoryManager(Instance instance)
    {
        _instance = instance;
    }

    public synchronized void
    add(EndpointFactory factory)
    {
        for(int i = 0; i < _factories.size(); i++)
        {
            EndpointFactory f = (EndpointFactory)_factories.get(i);
            if(f.type() == factory.type())
            {
                assert(false);
            }
        }
        _factories.add(factory);
    }

    public synchronized EndpointFactory
    get(short type)
    {
        for(int i = 0; i < _factories.size(); i++)
        {
            EndpointFactory f = (EndpointFactory)_factories.get(i);
            if(f.type() == type)
            {
                return f;
            }
        }
        return null;
    }

    public synchronized Endpoint
    create(String str)
    {
        String s = str.trim();
        if(s.length() == 0)
        {
	    Ice.EndpointParseException e = new Ice.EndpointParseException();
	    e.str = str;
	    throw e;
        }

        java.util.regex.Pattern p = java.util.regex.Pattern.compile("([ \t\n\r]+)|$");
        java.util.regex.Matcher m = p.matcher(s);
        boolean b = m.find();
        assert(b);

        String protocol = s.substring(0, m.start());

        if(protocol.equals("default"))
        {
            protocol = _instance.defaultsAndOverrides().defaultProtocol;
        }

        for(int i = 0; i < _factories.size(); i++)
        {
            EndpointFactory f = (EndpointFactory)_factories.get(i);
            if(f.protocol().equals(protocol))
            {
                return f.create(s.substring(m.end()));
            }
        }

	Ice.EndpointParseException e = new Ice.EndpointParseException();
	e.str = str;
	throw e;
    }

    public synchronized Endpoint
    read(BasicStream s)
    {
        Endpoint v;
        short type = s.readShort();

        for(int i = 0; i < _factories.size(); i++)
        {
            EndpointFactory f = (EndpointFactory)_factories.get(i);
            if(f.type() == type)
            {
                return f.read(s);
            }
        }

        return new UnknownEndpoint(type, s);
    }

    void
    destroy()
    {
        for(int i = 0; i < _factories.size(); i++)
        {
            EndpointFactory f = (EndpointFactory)_factories.get(i);
            f.destroy();
        }
        _factories.clear();
    }

    private Instance _instance;
    private java.util.ArrayList _factories = new java.util.ArrayList();
}
