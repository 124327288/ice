// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifdef _WIN32
#   include <IceUtil/Config.h>
#endif
#include <Operation.h>
#include <Current.h>
#include <Proxy.h>
#include <Types.h>
#include <Util.h>
#include <Ice/Communicator.h>
#include <Ice/IdentityUtil.h>
#include <Ice/IncomingAsync.h>
#include <Ice/Initialize.h>
#include <Ice/LocalException.h>
#include <Ice/ObjectAdapter.h>
#include <Ice/OutgoingAsync.h>
#include <Ice/Proxy.h>
#include <Slice/PythonUtil.h>

using namespace std;
using namespace IcePy;
using namespace Slice::Python;

namespace IcePy
{

class ParamInfo : public UnmarshalCallback
{
public:

    virtual void unmarshaled(PyObject*, PyObject*, void*);

    TypeInfoPtr type;
};
typedef IceUtil::Handle<ParamInfo> ParamInfoPtr;
typedef std::vector<ParamInfoPtr> ParamInfoList;

class OperationI : public Operation
{
public:

    virtual PyObject* invoke(const Ice::ObjectPrx&, const Ice::CommunicatorPtr&, PyObject*, PyObject*);
    virtual PyObject* invokeAsync(const Ice::ObjectPrx&, const Ice::CommunicatorPtr&, PyObject*, PyObject*, PyObject*);

    virtual void dispatch(PyObject*, const Ice::AMD_Object_ice_invokePtr&, const vector<Ice::Byte>&,
                          const Ice::Current&);

    void responseAsync(PyObject*, bool, const vector<Ice::Byte>&, const Ice::CommunicatorPtr&);
    void responseAsyncException(PyObject*, PyObject*);

    void sendResponse(const Ice::AMD_Object_ice_invokePtr&, PyObject*, const Ice::CommunicatorPtr&);
    void sendException(const Ice::AMD_Object_ice_invokePtr&, PyObject*, const Ice::CommunicatorPtr&);

    std::string name;
    Ice::OperationMode mode;
    bool amd;
    ParamInfoList inParams;
    ParamInfoList outParams;
    ParamInfoPtr returnType;
    ExceptionInfoList exceptions;

private:

    bool prepareRequest(const Ice::CommunicatorPtr&, PyObject*, bool, vector<Ice::Byte>&);
    PyObject* unmarshalResults(const vector<Ice::Byte>&, const Ice::CommunicatorPtr&);
    PyObject* unmarshalException(const std::vector<Ice::Byte>&, const Ice::CommunicatorPtr&);
    bool validateException(PyObject*) const;
};
typedef IceUtil::Handle<OperationI> OperationIPtr;

class AMICallback : public Ice::AMI_Object_ice_invoke
{
public:

    AMICallback(const OperationIPtr&, const Ice::CommunicatorPtr&, PyObject*);
    ~AMICallback();

    virtual void ice_response(bool, const vector<Ice::Byte>&);
    virtual void ice_exception(const Ice::Exception&);

private:

    OperationIPtr _op;
    Ice::CommunicatorPtr _communicator;
    PyObject* _callback;
};

struct OperationObject
{
    PyObject_HEAD
    OperationPtr* op;
};

struct AMDCallbackObject
{
    PyObject_HEAD
    OperationIPtr* op;
    Ice::CommunicatorPtr* communicator;
    Ice::AMD_Object_ice_invokePtr* cb;
};

extern PyTypeObject OperationType;
extern PyTypeObject AMDCallbackType;

}

#ifdef WIN32
extern "C"
#endif
static OperationObject*
operationNew(PyObject* /*arg*/)
{
    OperationObject* self = PyObject_New(OperationObject, &OperationType);
    if (self == NULL)
    {
        return NULL;
    }
    self->op = 0;
    return self;
}

#ifdef WIN32
extern "C"
#endif
static int
operationInit(OperationObject* self, PyObject* args, PyObject* /*kwds*/)
{
    char* name;
    PyObject* modeType = lookupType("Ice.OperationMode");
    assert(modeType != NULL);
    PyObject* mode;
    int amd;
    PyObject* inParams;
    PyObject* outParams;
    PyObject* returnType;
    PyObject* exceptions;
    if(!PyArg_ParseTuple(args, "sO!iO!O!OO!", &name, modeType, &mode, &amd, &PyTuple_Type, &inParams,
                         &PyTuple_Type, &outParams, &returnType, &PyTuple_Type, &exceptions))
    {
        return -1;
    }

    OperationIPtr op = new OperationI;

    op->name = name;
    op->amd = amd ? true : false;

    //
    // mode
    //
    PyObjectHandle modeValue = PyObject_GetAttrString(mode, "value");
    assert(PyInt_Check(modeValue.get()));
    op->mode = (Ice::OperationMode)static_cast<int>(PyInt_AS_LONG(modeValue.get()));

    //
    // inParams
    //
    int i, sz;
    sz = PyTuple_GET_SIZE(inParams);
    for(i = 0; i < sz; ++i)
    {
        ParamInfoPtr param = new ParamInfo;
        param->type = getType(PyTuple_GET_ITEM(inParams, i));
        op->inParams.push_back(param);
    }

    //
    // outParams
    //
    sz = PyTuple_GET_SIZE(outParams);
    for(i = 0; i < sz; ++i)
    {
        ParamInfoPtr param = new ParamInfo;
        param->type = getType(PyTuple_GET_ITEM(outParams, i));
        op->outParams.push_back(param);
    }

    //
    // returnType
    //
    if(returnType != Py_None)
    {
        op->returnType = new ParamInfo;
        op->returnType->type = getType(returnType);
    }

    //
    // exceptions
    //
    sz = PyTuple_GET_SIZE(exceptions);
    for(i = 0; i < sz; ++i)
    {
        op->exceptions.push_back(getException(PyTuple_GET_ITEM(exceptions, i)));
    }

    self->op = new OperationPtr(op);

    return 0;
}

#ifdef WIN32
extern "C"
#endif
static void
operationDealloc(OperationObject* self)
{
    delete self->op;
    PyObject_Del(self);
}

#ifdef WIN32
extern "C"
#endif
static PyObject*
operationInvoke(OperationObject* self, PyObject* args)
{
    PyObject* pyProxy;
    PyObject* opArgs;
    PyObject* ctx;
    if(!PyArg_ParseTuple(args, "O!O!O", &ProxyType, &pyProxy, &PyTuple_Type, &opArgs, &ctx))
    {
        return NULL;
    }

    if(ctx != Py_None && !PyDict_Check(ctx))
    {
        PyErr_Format(PyExc_ValueError, "context argument must be None or a dictionary");
        return NULL;
    }

    Ice::ObjectPrx prx = getProxy(pyProxy);
    Ice::CommunicatorPtr communicator = getProxyCommunicator(pyProxy);

    assert(self->op);
    return (*self->op)->invoke(prx, communicator, opArgs, ctx);
}

#ifdef WIN32
extern "C"
#endif
static PyObject*
operationInvokeAsync(OperationObject* self, PyObject* args)
{
    PyObject* pyProxy;
    PyObject* cb;
    PyObject* opArgs;
    PyObject* ctx;
    if(!PyArg_ParseTuple(args, "O!OO!O", &ProxyType, &pyProxy, &cb, &PyTuple_Type, &opArgs, &ctx))
    {
        return NULL;
    }

    if(ctx != Py_None && !PyDict_Check(ctx))
    {
        PyErr_Format(PyExc_ValueError, "context argument must be None or a dictionary");
        return NULL;
    }

    Ice::ObjectPrx prx = getProxy(pyProxy);
    Ice::CommunicatorPtr communicator = getProxyCommunicator(pyProxy);

    assert(self->op);
    return (*self->op)->invokeAsync(prx, communicator, cb, opArgs, ctx);
}

#ifdef WIN32
extern "C"
#endif
static AMDCallbackObject*
amdCallbackNew(PyObject* /*arg*/)
{
    AMDCallbackObject* self = PyObject_New(AMDCallbackObject, &AMDCallbackType);
    if (self == NULL)
    {
        return NULL;
    }
    self->op = 0;
    self->communicator = 0;
    self->cb = 0;
    return self;
}

#ifdef WIN32
extern "C"
#endif
static void
amdCallbackDealloc(AMDCallbackObject* self)
{
    delete self->op;
    delete self->communicator;
    delete self->cb;
    PyObject_Del(self);
}

#ifdef WIN32
extern "C"
#endif
static PyObject*
amdCallbackIceResponse(AMDCallbackObject* self, PyObject* args)
{
    try
    {
        assert(self->op);
        (*self->op)->sendResponse(*self->cb, args, *self->communicator);
    }
    catch(const Ice::Exception& ex)
    {
        (*self->cb)->ice_exception(ex);
    }
    catch(...)
    {
        //
        // No exceptions should propagate to Python.
        //
        assert(false);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

#ifdef WIN32
extern "C"
#endif
static PyObject*
amdCallbackIceException(AMDCallbackObject* self, PyObject* args)
{
    PyObject* ex;
    if(!PyArg_ParseTuple(args, "O", &ex))
    {
        return NULL;
    }

    try
    {
        assert(self->op);
        (*self->op)->sendException(*self->cb, ex, *self->communicator);
    }
    catch(const Ice::Exception& ex)
    {
        (*self->cb)->ice_exception(ex);
    }
    catch(...)
    {
        //
        // No exceptions should propagate to Python.
        //
        assert(false);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

//
// Operation implementation.
//
IcePy::Operation::~Operation()
{
}

//
// ParamInfo implementation.
//
void
IcePy::ParamInfo::unmarshaled(PyObject* val, PyObject* target, void* closure)
{
    assert(PyTuple_Check(target));
    int i = reinterpret_cast<int>(closure);
    PyTuple_SET_ITEM(target, i, val);
    Py_INCREF(val); // PyTuple_SET_ITEM steals a reference.
}

//
// AMICallback implementation.
//
IcePy::AMICallback::AMICallback(const OperationIPtr& op, const Ice::CommunicatorPtr& communicator,
                                PyObject* callback) :
    _op(op), _communicator(communicator), _callback(callback)
{
    Py_INCREF(_callback);
}

IcePy::AMICallback::~AMICallback()
{
    AdoptThread adoptThread; // Ensure the current thread is able to call into Python.

    Py_DECREF(_callback);
}

void
IcePy::AMICallback::ice_response(bool ok, const vector<Ice::Byte>& result)
{
    AdoptThread adoptThread; // Ensure the current thread is able to call into Python.

    _op->responseAsync(_callback, ok, result, _communicator);
}

void
IcePy::AMICallback::ice_exception(const Ice::Exception& ex)
{
    AdoptThread adoptThread; // Ensure the current thread is able to call into Python.

    PyObjectHandle exh = convertException(ex);
    assert(exh.get() != NULL);
    _op->responseAsyncException(_callback, exh.get());
}

//
// OperationI implementation.
//
PyObject*
IcePy::OperationI::invoke(const Ice::ObjectPrx& proxy, const Ice::CommunicatorPtr& communicator, PyObject* args,
                          PyObject* pyctx)
{
    //
    // Marshal the input parameters to a byte sequence.
    //
    Ice::ByteSeq params;
    if(!prepareRequest(communicator, args, false, params))
    {
        return NULL;
    }

    try
    {
        //
        // Invoke the operation. Use name here, not fixedName.
        //
        Ice::ByteSeq result;
        bool status;
        {
            if(pyctx != Py_None)
            {
                Ice::Context ctx;

                if(!PyDict_Check(pyctx))
                {
                    PyErr_Format(PyExc_ValueError, "context argument must be None or a dictionary");
                    return NULL;
                }

                if(!dictionaryToContext(pyctx, ctx))
                {
                    return NULL;
                }

                AllowThreads allowThreads; // Release Python's global interpreter lock during remote invocations.
                status = proxy->ice_invoke(name, (Ice::OperationMode)mode, params, result, ctx);
            }
            else
            {
                AllowThreads allowThreads; // Release Python's global interpreter lock during remote invocations.
                status = proxy->ice_invoke(name, (Ice::OperationMode)mode, params, result);
            }
        }

        //
        // Process the reply.
        //
        if(proxy->ice_isTwoway())
        {
            if(!status)
            {
                //
                // Unmarshal a user exception.
                //
                PyObjectHandle ex = unmarshalException(result, communicator);

                //
                // Set the Python exception.
                //
                assert(PyInstance_Check(ex.get()));
                PyObject* type = (PyObject*)((PyInstanceObject*)ex.get())->in_class;
                Py_INCREF(type);
                PyErr_Restore(type, ex.release(), NULL);

                return NULL;
            }
            else if(outParams.size() > 0 || returnType)
            {
                //
                // Unmarshal the results. If there is more than one value to be returned, then return them
                // in a tuple of the form (result, outParam1, ...). Otherwise just return the value.
                //
                PyObjectHandle results = unmarshalResults(result, communicator);
                if(results.get() == NULL)
                {
                    return NULL;
                }

                if(PyTuple_GET_SIZE(results.get()) > 1)
                {
                    return results.release();
                }
                else
                {
                    PyObject* ret = PyTuple_GET_ITEM(results.get(), 0);
                    Py_INCREF(ret);
                    return ret;
                }
            }
        }
    }
    catch(const AbortMarshaling&)
    {
        return NULL;
    }
    catch(const Ice::Exception& ex)
    {
        setPythonException(ex);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject*
IcePy::OperationI::invokeAsync(const Ice::ObjectPrx& proxy, const Ice::CommunicatorPtr& communicator,
                               PyObject* callback, PyObject* args, PyObject* pyctx)
{
    //
    // Marshal the input parameters to a byte sequence.
    //
    Ice::ByteSeq params;
    if(!prepareRequest(communicator, args, true, params))
    {
        return NULL;
    }

    try
    {
        Ice::AMI_Object_ice_invokePtr cb = new AMICallback(this, communicator, callback);

        //
        // Invoke the operation asynchronously. Use name here, not fixedName.
        //
        if(pyctx != Py_None)
        {
            Ice::Context ctx;

            if(!PyDict_Check(pyctx))
            {
                PyErr_Format(PyExc_ValueError, "context argument must be None or a dictionary");
                return NULL;
            }

            if(!dictionaryToContext(pyctx, ctx))
            {
                return NULL;
            }

            AllowThreads allowThreads; // Release Python's global interpreter lock during remote invocations.
            proxy->ice_invoke_async(cb, name, mode, params, ctx);
        }
        else
        {
            AllowThreads allowThreads; // Release Python's global interpreter lock during remote invocations.
            proxy->ice_invoke_async(cb, name, mode, params);
        }
    }
    catch(const Ice::Exception& ex)
    {
        setPythonException(ex);
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

void
IcePy::OperationI::dispatch(PyObject* servant, const Ice::AMD_Object_ice_invokePtr& cb,
                            const std::vector<Ice::Byte>& inBytes, const Ice::Current& current)
{
    string fixedName = fixIdent(current.operation);
    Ice::CommunicatorPtr communicator = current.adapter->getCommunicator();

    //
    // Unmarshal the in parameters.
    //
    int count = static_cast<int>(inParams.size()) + 1; // Leave room for a trailing Ice::Current object.

    int start = 0;
    if(amd)
    {
        ++count; // Leave room for a leading AMD callback argument.
        start = 1;
    }

    PyObjectHandle args = PyTuple_New(count);
    if(args.get() == NULL)
    {
        throwPythonException();
    }

    Ice::InputStreamPtr is = Ice::createInputStream(communicator, inBytes);

    try
    {
        int i = start;
        for(ParamInfoList::iterator p = inParams.begin(); p != inParams.end(); ++p, ++i)
        {
            (*p)->type->unmarshal(is, *p, args.get(), (void*)i);
        }
        is->finished();
    }
    catch(const AbortMarshaling&)
    {
        throwPythonException();
    }

    //
    // Create an object to represent Ice::Current. We need to append this to the argument tuple.
    //
    PyObjectHandle curr = createCurrent(current);
    if(PyTuple_SET_ITEM(args.get(), PyTuple_GET_SIZE(args.get()) - 1, curr.get()) < 0)
    {
        throwPythonException();
    }
    curr.release(); // PyTuple_SET_ITEM steals a reference.

    string methodName;
    if(amd)
    {
        methodName = fixedName + "_async";
        //
        // Create the callback object and pass it as the first argument.
        //
        AMDCallbackObject* obj = amdCallbackNew(NULL);
        if(obj == NULL)
        {
            throwPythonException();
        }
        obj->op = new OperationIPtr(this);
        obj->communicator = new Ice::CommunicatorPtr(communicator);
        obj->cb = new Ice::AMD_Object_ice_invokePtr(cb);
        if(PyTuple_SET_ITEM(args.get(), 0, (PyObject*)obj) < 0) // PyTuple_SET_ITEM steals a reference.
        {
            Py_DECREF(obj);
            throwPythonException();
        }
    }
    else
    {
        methodName = fixedName;
    }

    //
    // Dispatch the operation. Use fixedName here, not current.operation.
    //
    PyObjectHandle method = PyObject_GetAttrString(servant, const_cast<char*>(methodName.c_str()));
    if(method.get() == NULL)
    {
        ostringstream ostr;
        ostr << "servant for identity " << Ice::identityToString(current.id) << " does not define operation `"
             << methodName << "'";
        string str = ostr.str();
        PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
        Ice::UnknownException ex(__FILE__, __LINE__);
        ex.unknown = str;
        throw ex;
    }

    PyObjectHandle result = PyObject_Call(method.get(), args.get(), NULL);

    //
    // Check for exceptions.
    //
    if(PyErr_Occurred())
    {
        PyObjectHandle ex = getPythonException(); // Retrieve it before another Python API call clears it.
        sendException(cb, ex.get(), communicator);
        return;
    }

    if(!amd)
    {
        sendResponse(cb, result.get(), communicator);
    }
}

void
IcePy::OperationI::responseAsync(PyObject* callback, bool ok, const vector<Ice::Byte>& results,
                                 const Ice::CommunicatorPtr& communicator)
{
    try
    {
        if(ok)
        {
            //
            // Unmarshal the results.
            //
            PyObjectHandle args = unmarshalResults(results, communicator);
            if(args.get() == NULL)
            {
                assert(PyErr_Occurred());
                PyErr_Print();
                return;
            }

            PyObjectHandle method = PyObject_GetAttrString(callback, "ice_response");
            if(method.get() == NULL)
            {
                ostringstream ostr;
                ostr << "AMI callback object for operation `" << name << "' does not define ice_response()";
                string str = ostr.str();
                PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
            }
            else
            {
                PyObjectHandle tmp = PyObject_Call(method.get(), args.get(), NULL);
                if(PyErr_Occurred())
                {
                    PyErr_Print();
                }
            }
        }
        else
        {
            PyObjectHandle ex = unmarshalException(results, communicator);
            responseAsyncException(callback, ex.get());
        }
    }
    catch(const AbortMarshaling&)
    {
        assert(PyErr_Occurred());
        PyErr_Print();
    }
    catch(const Ice::Exception& ex)
    {
        ostringstream ostr;
        ostr << "Exception occurred during AMI response for operation `" << name << "':" << ex;
        string str = ostr.str();
        PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
    }
}

void
IcePy::OperationI::responseAsyncException(PyObject* callback, PyObject* ex)
{
    PyObjectHandle method = PyObject_GetAttrString(callback, "ice_exception");
    if(method.get() == NULL)
    {
        ostringstream ostr;
        ostr << "AMI callback object for operation `" << name << "' does not define ice_exception()";
        string str = ostr.str();
        PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
    }
    else
    {
        PyObjectHandle args = Py_BuildValue("(O)", ex);
        PyObjectHandle tmp = PyObject_Call(method.get(), args.get(), NULL);
        if(PyErr_Occurred())
        {
            PyErr_Print();
        }
    }
}

void
IcePy::OperationI::sendResponse(const Ice::AMD_Object_ice_invokePtr& cb, PyObject* args,
                                const Ice::CommunicatorPtr& communicator)
{
    //
    // Marshal the results. If there is more than one value to be returned, then they must be
    // returned in a tuple of the form (result, outParam1, ...).
    //
    Ice::OutputStreamPtr os = Ice::createOutputStream(communicator);
    try
    {
        int i = returnType ? 1 : 0;
        int numResults = static_cast<int>(outParams.size()) + i;
        if(numResults > 1)
        {
            if(!PyTuple_Check(args) || PyTuple_GET_SIZE(args) != numResults)
            {
                ostringstream ostr;
                ostr << "operation `" << fixIdent(name) << "' should return a tuple of length " << numResults;
                string str = ostr.str();
                PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
                throw Ice::MarshalException(__FILE__, __LINE__);
            }
        }

        ObjectMap objectMap;

        for(ParamInfoList::iterator p = outParams.begin(); p != outParams.end(); ++p, ++i)
        {
            PyObject* arg;
            if(amd || numResults > 1)
            {
                arg = PyTuple_GET_ITEM(args, i);
            }
            else
            {
                arg = args;
                assert(outParams.size() == 1);
            }

            if(!(*p)->type->validate(arg))
            {
                // TODO: Provide the parameter name instead?
                ostringstream ostr;
                ostr << "invalid value for out argument " << (i + 1) << " in operation `" << fixIdent(name)
                     << (amd ? "_async" : "") << "'";
                string str = ostr.str();
                PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
                throw Ice::MarshalException(__FILE__, __LINE__);
            }
            (*p)->type->marshal(arg, os, &objectMap);
        }

        if(returnType)
        {
            PyObject* res;
            if(amd || numResults > 1)
            {
                res = PyTuple_GET_ITEM(args, 0);
            }
            else
            {
                assert(outParams.size() == 0);
                res = args;
            }
            if(!returnType->type->validate(res))
            {
                ostringstream ostr;
                ostr << "invalid return value for operation `" << fixIdent(name) << "'";
                string str = ostr.str();
                PyErr_Warn(PyExc_RuntimeWarning, const_cast<char*>(str.c_str()));
                throw Ice::MarshalException(__FILE__, __LINE__);
            }
            returnType->type->marshal(res, os, &objectMap);
        }

        Ice::ByteSeq outBytes;
        os->finished(outBytes);
        cb->ice_response(true, outBytes);
    }
    catch(const AbortMarshaling&)
    {
        throwPythonException();
    }
}

void
IcePy::OperationI::sendException(const Ice::AMD_Object_ice_invokePtr& cb, PyObject* ex,
                                 const Ice::CommunicatorPtr& communicator)
{
    try
    {
        PyObject* exType = (PyObject*)((PyInstanceObject*)ex)->in_class;

        //
        // A servant that calls sys.exit() will raise the SystemExit exception.
        // This is normally caught by the interpreter, causing it to exit.
        // However, we have no way to pass this exception to the interpreter,
        // so we act on it directly.
        //
        if(PyErr_GivenExceptionMatches(exType, PyExc_SystemExit))
        {
            handleSystemExit(ex); // Does not return.
        }

        PyObject* userExceptionType = lookupType("Ice.UserException");

        if(PyErr_GivenExceptionMatches(exType, userExceptionType))
        {
            //
            // Get the exception's type and verify that it is legal to be thrown from this operation.
            //
            PyObjectHandle iceType = PyObject_GetAttrString(ex, "ice_type");
            assert(iceType.get() != NULL);
            ExceptionInfoPtr info = ExceptionInfoPtr::dynamicCast(getException(iceType.get()));
            assert(info);
            if(!validateException(ex))
            {
                throwPythonException(ex); // Raises UnknownUserException.
            }
            else
            {
                Ice::OutputStreamPtr os = Ice::createOutputStream(communicator);
                ObjectMap objectMap;
                info->marshal(ex, os, &objectMap);

                Ice::ByteSeq bytes;
                os->finished(bytes);
                cb->ice_response(false, bytes);
            }
        }
        else
        {
            throwPythonException(ex);
        }
    }
    catch(const AbortMarshaling&)
    {
        throwPythonException();
    }
}

bool
IcePy::OperationI::prepareRequest(const Ice::CommunicatorPtr& communicator, PyObject* args, bool async,
                                  vector<Ice::Byte>& bytes)
{
    assert(PyTuple_Check(args));

    //
    // Validate the number of arguments.
    //
    int argc = PyTuple_GET_SIZE(args);
    int paramCount = static_cast<int>(inParams.size());
    if(argc != paramCount)
    {
        string fixedName = fixIdent(name);
        PyErr_Format(PyExc_RuntimeError, "%s expects %d in parameters", fixedName.c_str(), paramCount);
        return false;
    }

    try
    {
        //
        // Marshal the in parameters.
        //
        Ice::OutputStreamPtr os = Ice::createOutputStream(communicator);

        ObjectMap objectMap;
        int i = 0;
        for(ParamInfoList::iterator p = inParams.begin(); p != inParams.end(); ++p, ++i)
        {
            PyObject* arg = PyTuple_GET_ITEM(args, i);
            if(!(*p)->type->validate(arg))
            {
                string opName;
                if(async)
                {
                    opName = fixIdent(name) + "_async";
                }
                else
                {
                    opName = fixIdent(name);
                }
                PyErr_Format(PyExc_ValueError, "invalid value for argument %d in operation `%s'",
                             async ? i + 2 : i + 1, const_cast<char*>(opName.c_str()));
                return false;
            }
            (*p)->type->marshal(arg, os, &objectMap);
        }

        os->finished(bytes);
    }
    catch(const AbortMarshaling&)
    {
        return false;
    }
    catch(const Ice::Exception& ex)
    {
        setPythonException(ex);
        return false;
    }

    return true;
}

PyObject*
IcePy::OperationI::unmarshalResults(const vector<Ice::Byte>& bytes, const Ice::CommunicatorPtr& communicator)
{
    int i = returnType ? 1 : 0;
    int numResults = static_cast<int>(outParams.size()) + i;

    PyObjectHandle results = PyTuple_New(numResults);
    if(results.get() != NULL && numResults > 0)
    {
        //
        // Unmarshal the results. If there is more than one value to be returned, then return them
        // in a tuple of the form (result, outParam1, ...). Otherwise just return the value.
        //
        Ice::InputStreamPtr is = Ice::createInputStream(communicator, bytes);
        for(ParamInfoList::iterator p = outParams.begin(); p != outParams.end(); ++p, ++i)
        {
            (*p)->type->unmarshal(is, *p, results.get(), (void*)i);
        }

        if(returnType)
        {
            returnType->type->unmarshal(is, returnType, results.get(), 0);
        }

        is->finished();
    }

    return results.release();
}

PyObject*
IcePy::OperationI::unmarshalException(const std::vector<Ice::Byte>& bytes, const Ice::CommunicatorPtr& communicator)
{
    Ice::InputStreamPtr is = Ice::createInputStream(communicator, bytes);

    is->readBool(); // usesClasses

    string id = is->readString();
    while(!id.empty())
    {
        ExceptionInfoPtr info = lookupExceptionInfo(id);
        if(info)
        {
            PyObjectHandle ex = info->unmarshal(is);
            is->finished();

            if(validateException(ex.get()))
            {
                return ex.release();
            }
            else
            {
                throwPythonException(ex.get());
            }
        }
        else
        {
            is->skipSlice();
            id = is->readString();
        }
    }

    //
    // Getting here should be impossible: we can get here only if the
    // sender has marshaled a sequence of type IDs, none of which we
    // have factory for. This means that sender and receiver disagree
    // about the Slice definitions they use.
    //
    throw Ice::UnknownUserException(__FILE__, __LINE__);
}

bool
IcePy::OperationI::validateException(PyObject* ex) const
{
    for(ExceptionInfoList::const_iterator p = exceptions.begin(); p != exceptions.end(); ++p)
    {
        if(PyObject_IsInstance(ex, (*p)->pythonType.get()))
        {
            return true;
        }
    }

    return false;
}

static PyMethodDef OperationMethods[] =
{
    { "invoke", (PyCFunction)operationInvoke, METH_VARARGS, PyDoc_STR("internal function") },
    { "invokeAsync", (PyCFunction)operationInvokeAsync, METH_VARARGS, PyDoc_STR("internal function") },
    { NULL, NULL} /* sentinel */
};

static PyMethodDef AMDCallbackMethods[] =
{
    { "ice_response", (PyCFunction)amdCallbackIceResponse, METH_VARARGS, PyDoc_STR("internal function") },
    { "ice_exception", (PyCFunction)amdCallbackIceException, METH_VARARGS, PyDoc_STR("internal function") },
    { NULL, NULL} /* sentinel */
};

namespace IcePy
{

PyTypeObject OperationType =
{
    /* The ob_type field must be initialized in the module init function
     * to be portable to Windows without using C++. */
    PyObject_HEAD_INIT(NULL)
    0,                               /* ob_size */
    "IcePy.Operation",               /* tp_name */
    sizeof(OperationObject),         /* tp_basicsize */
    0,                               /* tp_itemsize */
    /* methods */
    (destructor)operationDealloc,    /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_compare */
    0,                               /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,              /* tp_flags */
    0,                               /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    OperationMethods,                /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    (initproc)operationInit,         /* tp_init */
    0,                               /* tp_alloc */
    (newfunc)operationNew,           /* tp_new */
    0,                               /* tp_free */
    0,                               /* tp_is_gc */
};

PyTypeObject AMDCallbackType =
{
    /* The ob_type field must be initialized in the module init function
     * to be portable to Windows without using C++. */
    PyObject_HEAD_INIT(NULL)
    0,                               /* ob_size */
    "IcePy.AMDCallback",             /* tp_name */
    sizeof(AMDCallbackObject),       /* tp_basicsize */
    0,                               /* tp_itemsize */
    /* methods */
    (destructor)amdCallbackDealloc,  /* tp_dealloc */
    0,                               /* tp_print */
    0,                               /* tp_getattr */
    0,                               /* tp_setattr */
    0,                               /* tp_compare */
    0,                               /* tp_repr */
    0,                               /* tp_as_number */
    0,                               /* tp_as_sequence */
    0,                               /* tp_as_mapping */
    0,                               /* tp_hash */
    0,                               /* tp_call */
    0,                               /* tp_str */
    0,                               /* tp_getattro */
    0,                               /* tp_setattro */
    0,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,              /* tp_flags */
    0,                               /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    AMDCallbackMethods,              /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    0,                               /* tp_init */
    0,                               /* tp_alloc */
    (newfunc)amdCallbackNew,         /* tp_new */
    0,                               /* tp_free */
    0,                               /* tp_is_gc */
};

}

bool
IcePy::initOperation(PyObject* module)
{
    if(PyType_Ready(&OperationType) < 0)
    {
        return false;
    }
    if(PyModule_AddObject(module, "Operation", (PyObject*)&OperationType) < 0)
    {
        return false;
    }

    if(PyType_Ready(&AMDCallbackType) < 0)
    {
        return false;
    }
    if(PyModule_AddObject(module, "AMDCallback", (PyObject*)&AMDCallbackType) < 0)
    {
        return false;
    }

    return true;
}

IcePy::OperationPtr
IcePy::getOperation(PyObject* p)
{
    assert(PyObject_IsInstance(p, (PyObject*)&OperationType) == 1);
    OperationObject* obj = (OperationObject*)p;
    return *obj->op;
}
