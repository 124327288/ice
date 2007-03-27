// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef ICEE_CONNECTION_F_H
#define ICEE_CONNECTION_F_H

#include <IceE/Handle.h>
#include <IceE/Shared.h>

namespace Ice
{

class Connection;
ICE_API bool operator==(const Connection&, const Connection&);
ICE_API bool operator<(const Connection&, const Connection&);

}

namespace IceInternal
{

ICE_API IceUtil::Shared* upCast(::Ice::Connection*);

}

namespace Ice
{

typedef ::IceInternal::Handle< ::Ice::Connection> ConnectionPtr;

}

#endif
