// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef ICEPY_UTIL_H
#define ICEPY_UTIL_H

#include <Python.h>
#include <Ice/BuiltinSequences.h>
#include <Ice/Current.h>
#include <Ice/Exception.h>

namespace IcePy
{

//
// Invokes Py_DECREF on a Python object.
//
class PyObjectHandle
{
public:

    PyObjectHandle(PyObject* = NULL);
    PyObjectHandle(const PyObjectHandle&);
    ~PyObjectHandle();

    void operator=(PyObject*);
    void operator=(const PyObjectHandle&);

    PyObject* get();
    PyObject* release();

private:

    PyObject* _p;
};

//
// Release Python's Global Interpreter Lock during potentially time-consuming
// (and non-Python related) work.
//
class AllowThreads
{
public:

    AllowThreads();
    ~AllowThreads();

private:

    PyThreadState* _state;
};

//
// Ensure that the current thread is capable of calling into Python.
//
class AdoptThread
{
public:

    AdoptThread();
    ~AdoptThread();

private:

    PyGILState_STATE _state;
};

//
// Convert Ice::StringSeq to and from a Python list.
//
bool listToStringSeq(PyObject*, Ice::StringSeq&);
bool stringSeqToList(const Ice::StringSeq&, PyObject*);

//
// Convert Ice::Context to and from a Python dictionary.
//
bool dictionaryToContext(PyObject*, Ice::Context&);
bool contextToDictionary(const Ice::Context&, PyObject*);

//
// Returns a borrowed reference to the Python type object corresponding
// to the given Python type name.
//
PyObject* lookupType(const std::string&);

//
// Returns the current Python exception.
//
PyObject* getPythonException(bool = true);

//
// Creates an exception instance of the given type.
//
PyObject* createExceptionInstance(PyObject*);

//
// Converts an Ice exception into a Python exception.
//
void setPythonException(const Ice::Exception&);

//
// Converts a Python exception into an Ice exception and throws it.
//
void throwPythonException(PyObject* = NULL);

//
// Handle the SystemExit exception.
//
void handleSystemExit();

}

#endif
