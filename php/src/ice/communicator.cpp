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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ice_communicator.h"
#include "ice_proxy.h"
#include "ice_marshal.h"
#include "ice_util.h"

using namespace std;
using namespace IcePHP;

ZEND_EXTERN_MODULE_GLOBALS(ice)

//
// Class entries represent the PHP class implementations we have registered.
//
static zend_class_entry* _communicatorClassEntry;

//
// Ice::Communicator support.
//
static zend_object_handlers _handlers;

extern "C"
{
static zend_object_value handleAlloc(zend_class_entry* TSRMLS_DC);
static void handleDestroy(void*, zend_object_handle TSRMLS_DC);
static zend_object_value handleClone(zval* TSRMLS_DC);
static union _zend_function* handleGetMethod(zval*, char*, int TSRMLS_DC);
}

static void initCommunicator(ice_object* TSRMLS_DC);

//
// Function entries for Ice::Communicator methods.
//
static function_entry _methods[] =
{
    {"__construct",         PHP_FN(Ice_Communicator___construct),         NULL},
    {"stringToProxy",       PHP_FN(Ice_Communicator_stringToProxy),       NULL},
    {"proxyToString",       PHP_FN(Ice_Communicator_proxyToString),       NULL},
    {"addObjectFactory",    PHP_FN(Ice_Communicator_addObjectFactory),    NULL},
    {"removeObjectFactory", PHP_FN(Ice_Communicator_removeObjectFactory), NULL},
    {"findObjectFactory",   PHP_FN(Ice_Communicator_findObjectFactory),   NULL},
    {"flushBatchRequests",  PHP_FN(Ice_Communicator_flushBatchRequests),  NULL},
    {NULL, NULL, NULL}
};

bool
IcePHP::communicatorInit(TSRMLS_D)
{
    //
    // Register the Ice_Communicator class.
    //
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Ice_Communicator", _methods);
    ce.create_object = handleAlloc;
    _communicatorClassEntry = zend_register_internal_class(&ce TSRMLS_CC);
    memcpy(&_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    _handlers.clone_obj = handleClone;
    _handlers.get_method = handleGetMethod;

    return true;
}

bool
IcePHP::createCommunicator(TSRMLS_D)
{
    zval* global;
    MAKE_STD_ZVAL(global);

    //
    // Create the global variable for the communicator, but delay creation of the communicator
    // itself until it is first used (see handleGetMethod).
    //
    if(object_init_ex(global, _communicatorClassEntry) != SUCCESS)
    {
        zend_error(E_ERROR, "unable to create object for communicator");
        return false;
    }

    //
    // Register the global variable "ICE" to hold the communicator.
    //
    ICE_G(communicator) = global;
    ZEND_SET_GLOBAL_VAR("ICE", global);

    return true;
}

Ice::CommunicatorPtr
IcePHP::getCommunicator(TSRMLS_D)
{
    Ice::CommunicatorPtr result;

    zval **zv;
    if(zend_hash_find(&EG(symbol_table), "ICE", sizeof("ICE"), (void **) &zv) == SUCCESS)
    {
        ice_object* obj = getObject(*zv TSRMLS_CC);
        assert(obj);

        //
        // Initialize the communicator if necessary.
        //
        if(!obj->ptr)
        {
            try
            {
                initCommunicator(obj TSRMLS_CC);
            }
            catch(const IceUtil::Exception& ex)
            {
                ostringstream ostr;
                ex.ice_print(ostr);
                zend_error(E_ERROR, "unable to initialize communicator:\n%s", ostr.str().c_str());
                return 0;
            }
        }

        Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);
        result = *_this;
    }

    return result;
}

zval*
IcePHP::getCommunicatorZval(TSRMLS_D)
{
    zval **zv = NULL;
    zend_hash_find(&EG(symbol_table), "ICE", sizeof("ICE"), (void **) &zv);
    assert(zv);
    return *zv;
}

ZEND_FUNCTION(Ice_Communicator___construct)
{
    zend_error(E_ERROR, "Ice_Communicator cannot be instantiated, use the global variable $ICE");
}

ZEND_FUNCTION(Ice_Communicator_stringToProxy)
{
    if(ZEND_NUM_ARGS() != 1)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        return;
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    char *str;
    int len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len) == FAILURE)
    {
        RETURN_NULL();
    }

    Ice::ObjectPrx proxy;
    try
    {
        proxy = (*_this)->stringToProxy(str);
    }
    catch(const IceUtil::Exception& ex)
    {
        throwException(ex TSRMLS_CC);
        RETURN_NULL();
    }

    if(!createProxy(return_value, proxy TSRMLS_CC))
    {
        RETURN_NULL();
    }
}

ZEND_FUNCTION(Ice_Communicator_proxyToString)
{
    if(ZEND_NUM_ARGS() != 1)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        return;
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    zval* zprx;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O!", &zprx, proxyClassEntry) == FAILURE)
    {
        RETURN_EMPTY_STRING();
    }

    Ice::ObjectPrx proxy;
    Slice::ClassDefPtr def;
    if(!fetchProxy(zprx, proxy, def TSRMLS_CC))
    {
        RETURN_EMPTY_STRING();
    }

    try
    {
        string result = (*_this)->proxyToString(proxy);
        RETURN_STRING(const_cast<char*>(result.c_str()), 1);
    }
    catch(const IceUtil::Exception& ex)
    {
        throwException(ex TSRMLS_CC);
        RETURN_EMPTY_STRING();
    }
}

ZEND_FUNCTION(Ice_Communicator_addObjectFactory)
{
    if(ZEND_NUM_ARGS() != 2)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        return;
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    zval* zfactory;
    char* id;
    int len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "os", &zfactory, &id, &len) == FAILURE)
    {
        return;
    }

    //
    // Verify that the object implements Ice_ObjectFactory.
    //
    // TODO: When zend_check_class is changed to also check interfaces, we can remove this code and
    // pass the class entry for Ice_ObjectFactory to zend_parse_parameters instead.
    //
    zend_class_entry* ce = Z_OBJCE_P(zfactory);
    zend_class_entry* base = findClass("Ice_ObjectFactory" TSRMLS_CC);
    assert(base);
    if(!checkClass(ce, base))
    {
        zend_error(E_ERROR, "%s(): object does not implement Ice_ObjectFactory", get_active_function_name(TSRMLS_C));
        return;
    }

    //
    // Retrieve the PHPObjectFactory.
    //
    Ice::ObjectFactoryPtr factory = (*_this)->findObjectFactory("");
    PHPObjectFactoryPtr phpFactory = PHPObjectFactoryPtr::dynamicCast(factory);
    assert(phpFactory);

    phpFactory->addObjectFactory(zfactory, id TSRMLS_CC);
}

ZEND_FUNCTION(Ice_Communicator_removeObjectFactory)
{
    if(ZEND_NUM_ARGS() != 1)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        RETURN_NULL();
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    char* id;
    int len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &id, &len) == FAILURE)
    {
        RETURN_NULL();
    }

    //
    // Retrieve the PHPObjectFactory.
    //
    Ice::ObjectFactoryPtr factory = (*_this)->findObjectFactory("");
    PHPObjectFactoryPtr phpFactory = PHPObjectFactoryPtr::dynamicCast(factory);
    assert(phpFactory);

    phpFactory->removeObjectFactory(id TSRMLS_CC);
}

ZEND_FUNCTION(Ice_Communicator_findObjectFactory)
{
    if(ZEND_NUM_ARGS() != 1)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        RETURN_NULL();
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    char* id;
    int len;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &id, &len) == FAILURE)
    {
        RETURN_NULL();
    }

    //
    // Retrieve the PHPObjectFactory.
    //
    Ice::ObjectFactoryPtr factory = (*_this)->findObjectFactory("");
    PHPObjectFactoryPtr phpFactory = PHPObjectFactoryPtr::dynamicCast(factory);
    assert(phpFactory);

    phpFactory->findObjectFactory(id, return_value TSRMLS_CC);
}

ZEND_FUNCTION(Ice_Communicator_flushBatchRequests)
{
    if(ZEND_NUM_ARGS() != 0)
    {
        WRONG_PARAM_COUNT;
    }

    ice_object* obj = getObject(getThis() TSRMLS_CC);
    if(!obj)
    {
        RETURN_NULL();
    }
    assert(obj->ptr);
    Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);

    try
    {
        (*_this)->flushBatchRequests();;
    }
    catch(const IceUtil::Exception& ex)
    {
        throwException(ex TSRMLS_CC);
    }
}

#ifdef WIN32
extern "C"
#endif
static zend_object_value
handleAlloc(zend_class_entry* ce TSRMLS_DC)
{
    zend_object_value result;

    ice_object* obj = newObject(ce TSRMLS_CC);
    assert(obj);

    result.handle = zend_objects_store_put(obj, handleDestroy, NULL TSRMLS_CC);
    result.handlers = &_handlers;

    return result;
}

#ifdef WIN32
extern "C"
#endif
static void
handleDestroy(void* p, zend_object_handle handle TSRMLS_DC)
{
    ice_object* obj = static_cast<ice_object*>(p);
    if(obj->ptr)
    {
        Ice::CommunicatorPtr* _this = static_cast<Ice::CommunicatorPtr*>(obj->ptr);
        try
        {
            (*_this)->destroy();
        }
        catch(const IceUtil::Exception& ex)
        {
            ostringstream ostr;
            ex.ice_print(ostr);
            zend_error(E_ERROR, "unable to destroy communicator:\n%s", ostr.str().c_str());
        }
        delete _this;
    }

    zend_objects_destroy_object(static_cast<zend_object*>(p), handle TSRMLS_CC);
}

#ifdef WIN32
extern "C"
#endif
static zend_object_value
handleClone(zval* zv TSRMLS_DC)
{
    zend_object_value result;
    memset(&result, 0, sizeof(zend_object_value));
    zend_error(E_ERROR, "__clone is not supported for Ice_Communicator");
    return result;
}

#ifdef WIN32
extern "C"
#endif
static union _zend_function*
handleGetMethod(zval* zv, char* method, int len TSRMLS_DC)
{
    //
    // Delegate to the standard implementation of get_method. We're simply using this hook
    // as a convenient way of implementing lazy initialization of the communicator.
    //
    zend_function* result = zend_get_std_object_handlers()->get_method(zv, method, len TSRMLS_CC);
    if(result)
    {
        ice_object* obj = static_cast<ice_object*>(zend_object_store_get_object(zv TSRMLS_CC));
        if(!obj->ptr)
        {
            try
            {
                initCommunicator(obj TSRMLS_CC);
            }
            catch(const IceUtil::Exception& ex)
            {
                ostringstream ostr;
                ex.ice_print(ostr);
                zend_error(E_ERROR, "unable to initialize communicator:\n%s", ostr.str().c_str());
                return 0;
            }
        }
    }

    return result;
}

//
// Initialize a communicator instance and store it in the given object. Can raise exceptions.
//
static void
initCommunicator(ice_object* obj TSRMLS_DC)
{
    assert(!obj->ptr);

    Ice::PropertiesPtr* properties = static_cast<Ice::PropertiesPtr*>(ICE_G(properties));

    int argc = 0;
    char** argv = 0;
    Ice::CommunicatorPtr communicator = Ice::initializeWithProperties(argc, argv, *properties);
    obj->ptr = new Ice::CommunicatorPtr(communicator);

    //
    // Register our default object factory with the communicator.
    //
    Ice::ObjectFactoryPtr factory = new PHPObjectFactory(TSRMLS_C);
    communicator->addObjectFactory(factory, "");
}
