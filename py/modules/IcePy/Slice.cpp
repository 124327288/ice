// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Slice.h>
#include <Util.h>
#include <Slice/Preprocessor.h>
#include <Slice/PythonUtil.h>

//
// Python headers needed for PyEval_EvalCode.
//
#include <compile.h>
#include <eval.h>

using namespace std;
using namespace IcePy;
using namespace Slice;
using namespace Slice::Python;

extern "C"
PyObject*
Ice_loadSlice(PyObject* /*self*/, PyObject* args)
{
    char* cmd;
    if(!PyArg_ParseTuple(args, "s", &cmd))
    {
        return NULL;
    }

    Ice::StringSeq argSeq;
    if(!Slice::Python::splitString(cmd, argSeq))
    {
        return NULL;
    }

    string cppArgs;
    Ice::StringSeq files;
    bool debug = false;
    bool ice = true; // This must be true so that we can create Ice::Identity when necessary.
    bool caseSensitive = false;
    bool all = false;

    vector<string>::const_iterator p;
    for(p = argSeq.begin(); p != argSeq.end(); ++p)
    {
        string arg = *p;
        if(arg.substr(0, 2) == "-I" || arg.substr(0, 2) == "-D" || arg.substr(0, 2) == "-U")
        {
            cppArgs += ' ';
            if(arg.find(' ') != string::npos)
            {
                cppArgs += "'";
                cppArgs += arg;
                cppArgs += "'";
            }
            else
            {
                cppArgs += arg;
            }
        }
        else if(arg == "--case-sensitive")
        {
            caseSensitive = true;
        }
        else if(arg == "--all")
        {
            all = true;
        }
        else if(arg[0] == '-')
        {
            PyErr_Format(PyExc_RuntimeError, "unknown option `%s'", arg.c_str());
            return NULL;
        }
        else
        {
            files.push_back(arg);
        }
    }

    if(files.empty())
    {
        PyErr_Format(PyExc_RuntimeError, "no Slice files specified in `%s'", cmd);
        return NULL;
    }

    bool ignoreRedefs = false;

    for(p = files.begin(); p != files.end(); ++p)
    {
        string file = *p;
        Slice::Preprocessor icecpp("icecpp", file, cppArgs);
        FILE* cppHandle = icecpp.preprocess(false);

        if(cppHandle == 0)
        {
            PyErr_Format(PyExc_RuntimeError, "Slice preprocessing failed for `%s'", cmd);
            return NULL;
        }

        UnitPtr unit = Slice::Unit::createUnit(ignoreRedefs, all, ice, caseSensitive);
        int parseStatus = unit->parse(cppHandle, debug);

        if(!icecpp.close() || parseStatus == EXIT_FAILURE)
        {
            PyErr_Format(PyExc_RuntimeError, "Slice parsing failed for `%s'", cmd);
            unit->destroy();
            return NULL;
        }

        //
        // Generate the Python code into a string stream.
        //
        ostringstream codeStream;
        IceUtil::Output out(codeStream);
        generate(unit, out);
        unit->destroy();

        string code = codeStream.str();
        PyObjectHandle src = Py_CompileString(const_cast<char*>(code.c_str()), const_cast<char*>(file.c_str()),
                                              Py_file_input);
        if(src.get() == NULL)
        {
            return NULL;
        }

        PyObjectHandle globals = PyDict_New();
        if(globals.get() == NULL)
        {
            return NULL;
        }
        PyDict_SetItemString(globals.get(), "__builtins__", PyEval_GetBuiltins());

        PyObjectHandle val = PyEval_EvalCode((PyCodeObject*)src.get(), globals.get(), 0);
        if(val.get() == NULL)
        {
            return NULL;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}
