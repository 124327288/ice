// **********************************************************************
//
// Copyright (c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package Ice;

/**
 * Callback interface for <Blobject> AMD servants.
 *
 @see BlobjectAsync
 **/
public interface AMD_Object_ice_invoke
{
    /**
     * Indicates to the Ice run time that an operation
     * completed.
     *
     * @param ok <code>true</code> indicates that the operation
     * completed successfully; <code>false</code> indicates that the
     * operation raised user exception.
     * @param outParams The encoded out-parameters for the operation or,
     * if <code>ok</code> is <code>false</code>, the encoded user exception.
     **/
    void ice_response(boolean ok, byte[] outParams);

    /**
     * Indicates to the Ice run time that an operation completed
     * with a run-time exception.
     *
     * @param ex The encoded Ice run-time exception. Note that, if <code>ex</code>
     * is a user exception, the caller receives <code>UnknownUserException</code>.
     * Use <code>ice_response</code> to raise user exceptions.
     **/

    void ice_exception(java.lang.Exception ex);
}
