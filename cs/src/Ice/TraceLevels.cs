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

    public sealed class TraceLevels
    {
	internal TraceLevels(Ice.Properties properties)
	{
	    networkCat = "Network";
	    protocolCat = "Protocol";
	    retryCat = "Retry";
	    securityCat = "Security";
	    locationCat = "Location";
	    slicingCat = "Slicing";
	    
	    string keyBase = "Ice.Trace.";
	    
	    network = properties.getPropertyAsInt(keyBase + networkCat);
	    protocol = properties.getPropertyAsInt(keyBase + protocolCat);
	    retry = properties.getPropertyAsInt(keyBase + retryCat);
	    security = properties.getPropertyAsInt(keyBase + securityCat);
	    location = properties.getPropertyAsInt(keyBase + locationCat);
	    slicing = properties.getPropertyAsInt(keyBase + slicingCat);
	}
	
	public readonly int network;
	public readonly string networkCat;
	public readonly int protocol;
	public readonly string protocolCat;
	public readonly int retry;
	public readonly string retryCat;
	public readonly int security;
	public readonly string securityCat;
	public readonly int location;
	public readonly string locationCat;
	public readonly int slicing;
	public readonly string slicingCat;
    }

}
