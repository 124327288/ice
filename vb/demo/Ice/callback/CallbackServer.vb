'  **********************************************************************
' 
'  Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
' 
'  This copy of Ice is licensed to you under the terms described in the
'  ICE_LICENSE file included in this distribution.
' 
'  **********************************************************************

Imports Demo

Class CallbackServer
    Inherits Ice.Application

    Public Overloads Overrides Function run(ByVal args() As String) As Integer
	Dim adapter As Ice.ObjectAdapter = communicator().createObjectAdapter("Callback.Server")
	adapter.add(New CallbackI, Ice.Util.stringToIdentity("callback"))
	adapter.activate()
	communicator().waitForShutdown()
	Return 0
    End Function

End Class
