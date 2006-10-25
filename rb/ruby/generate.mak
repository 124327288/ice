# **********************************************************************
#
# Copyright (c) 2003-2006 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

.SUFFIXES:	.rb

!IF EXIST (../slice)
slicedir	= ../slice
!ELSE IFDEF ICE_HOME
slicedir	= $(ICE_HOME)/slice
!ELSE
!ERROR "Slice directory not found - set ICE_HOME!"
!ENDIF

SLICE2RB	= ..\bin\slice2rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check Ice.rb!
#
ICE_SRCS	= Ice/LocalException.rb \
		  Ice/Communicator.rb \
		  Ice/CommunicatorF.rb \
		  Ice/Logger.rb \
		  Ice/LoggerF.rb \
		  Ice/BuiltinSequences.rb \
		  Ice/ObjectAdapter.rb \
		  Ice/ObjectAdapterF.rb \
		  Ice/ServantLocator.rb \
		  Ice/ServantLocatorF.rb \
		  Ice/Properties.rb \
		  Ice/PropertiesF.rb \
		  Ice/ObjectFactory.rb \
		  Ice/ObjectFactoryF.rb \
		  Ice/Identity.rb \
		  Ice/Current.rb \
		  Ice/Router.rb \
		  Ice/RouterF.rb \
		  Ice/Plugin.rb \
		  Ice/PluginF.rb \
		  Ice/Locator.rb \
		  Ice/LocatorF.rb \
		  Ice/StatsF.rb \
		  Ice/Stats.rb \
		  Ice/Process.rb \
		  Ice/ProcessF.rb \
		  Ice/FacetMap.rb \
		  Ice/Connection.rb \
		  Ice/ConnectionF.rb \
		  Ice/SliceChecksumDict.rb \
		  Ice/Endpoint.rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check Glacier2.rb!
#
GLACIER2_SRCS	= Glacier2/RouterF.rb \
		  Glacier2/Router.rb \
		  Glacier2/SessionF.rb \
		  Glacier2/Session.rb \
		  Glacier2/PermissionsVerifierF.rb \
		  Glacier2/PermissionsVerifier.rb \
		  Glacier2/SSLInfo.rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check IceBox.rb!
#
ICEBOX_SRCS	= IceBox/IceBox.rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check IceGrid.rb!
#
ICEGRID_SRCS	= IceGrid/Admin.rb \
		  IceGrid/Descriptor.rb \
		  IceGrid/Exception.rb \
		  IceGrid/FileParser.rb \
		  IceGrid/Observer.rb \
		  IceGrid/Query.rb \
		  IceGrid/Registry.rb \
		  IceGrid/Session.rb \
		  IceGrid/UserAccountMapper.rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check IcePatch2.rb!
#
ICEPATCH2_SRCS	= IcePatch2/FileInfo.rb \
		  IcePatch2/FileServer.rb

#
# IMPORTANT: If you add or remove Slice files, you also need to check IceStorm.rb!
#
ICESTORM_SRCS	= IceStorm/IceStorm.rb

ALL_SRCS	= $(ICE_SRCS) \
		  $(GLACIER2_SRCS) \
		  $(ICEBOX_SRCS) \
		  $(ICEGRID_SRCS) \
		  $(ICEPATCH2_SRCS) \
		  $(ICESTORM_SRCS)

MODULES		= Glacier2 Ice IceBox IceGrid IcePatch2 IceStorm

SLICE2RBFLAGS	= -I$(slicedir) --ice

all:: $(MODULES) $(ALL_SRCS)

$(MODULES):
	-mkdir $@

$(ALL_SRCS): {$(slicedir)}$*.ice
	-$(SLICE2RB) $(SLICE2RBFLAGS) --output-dir $(*D) $(slicedir)/$*.ice

clean::
	-rmdir /S /Q $(MODULES)
