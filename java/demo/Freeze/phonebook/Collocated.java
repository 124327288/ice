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


class PhoneBookCollocated extends Ice.Application
{
    public int
    run(String[] args)
    {
	Ice.Properties properties = communicator().getProperties();
    
	//
	// Create and install a factory for contacts.
	//
	ContactFactory contactFactory = new ContactFactory();
	communicator().addObjectFactory(contactFactory, "::Contact");

	//
	// Create an object adapter
	//
	Ice.ObjectAdapter adapter = communicator().createObjectAdapter("PhoneBook");

	//
	// Create the Name index
	//
	NameIndex index = new NameIndex("name");
	Freeze.Index[] indices = new Freeze.Index[1];
	indices[0] = index;

	//
	// Create an Evictor for contacts.
	//
	Freeze.Evictor evictor = Freeze.Util.createEvictor(adapter, _envName, "contacts", null, indices, true);
	int evictorSize = properties.getPropertyAsInt("PhoneBook.EvictorSize");
	if(evictorSize > 0)
	{
	    evictor.setSize(evictorSize);
	}

	//
	// Set the evictor in the contact factory
	//
	contactFactory.setEvictor(evictor);

	//
	// Register the evictor with the adapter
	//
	adapter.addServantLocator(evictor, "contact");
    
	//
	// Create the phonebook, and add it to the Object Adapter.
	//
	PhoneBookI phoneBook = new PhoneBookI(evictor, contactFactory, index);
	adapter.add(phoneBook, Ice.Util.stringToIdentity("phonebook"));
    
	//
	// Everything ok, let's go.
	//
	int status = RunParser.runParser(appName(), args, communicator());
	adapter.deactivate();
	adapter.waitForDeactivate();

	return status;
    }

    PhoneBookCollocated(String envName)
    {
	_envName = envName;
    }

    private String _envName;
}

public class Collocated
{
    static public void
    main(String[] args)
    {
	PhoneBookCollocated app = new PhoneBookCollocated("db");
	app.main("demo.Freeze.phonebook.Collocated", args, "config");
    }
}
