// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package Ice;

public interface ObjectPrx
{
    int ice_hash();

    boolean ice_isA(String __id);
    boolean ice_isA(String __id, java.util.Map __context);

    void ice_ping();
    void ice_ping(java.util.Map __context);

    String[] ice_ids();
    String[] ice_ids(java.util.Map __context);

    String ice_id();
    String ice_id(java.util.Map __context);

    // Returns true if ok, false if user exception.
    boolean ice_invoke(String operation, OperationMode mode, byte[] inParams, ByteSeqHolder outParams);
    boolean ice_invoke(String operation, OperationMode mode, byte[] inParams, ByteSeqHolder outParams,
                       java.util.Map __context);

    void ice_invoke_async(AMI_Object_ice_invoke cb, String operation, OperationMode mode, byte[] inParams);
    void ice_invoke_async(AMI_Object_ice_invoke cb, String operation, OperationMode mode, byte[] inParams,
                          java.util.Map context);

    Identity ice_getIdentity();
    ObjectPrx ice_newIdentity(Identity newIdentity);

    java.util.Map ice_getContext();
    ObjectPrx ice_newContext(java.util.Map newContext);

    String ice_getFacet();
    ObjectPrx ice_newFacet(String newFacet);

    ObjectPrx ice_twoway();
    boolean ice_isTwoway();
    ObjectPrx ice_oneway();
    boolean ice_isOneway();
    ObjectPrx ice_batchOneway();
    boolean ice_isBatchOneway();
    ObjectPrx ice_datagram();
    boolean ice_isDatagram();
    ObjectPrx ice_batchDatagram();
    boolean ice_isBatchDatagram();

    ObjectPrx ice_secure(boolean b);
    ObjectPrx ice_compress(boolean co);
    ObjectPrx ice_timeout(int t);
    ObjectPrx ice_router(Ice.RouterPrx router);
    ObjectPrx ice_locator(Ice.LocatorPrx locator);
    ObjectPrx ice_collocationOptimization(boolean b);
    ObjectPrx ice_default();

    Connection ice_connection();

    boolean equals(java.lang.Object r);
}
