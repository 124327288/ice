// **********************************************************************
//
// Copyright (c) 2003-2004 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

using System.Collections;
using System.Diagnostics;
using System.Text;

namespace IceInternal
{
    public abstract class Patcher
    {
        public Patcher()
        {
        }

        public Patcher(System.Type type)
        {
            _type = type;
        }

	public abstract void patch(Ice.Object v);

        public string type()
        {
            Debug.Assert(_type != null);
            return _type.FullName;
        }

        protected System.Type _type;
    }

    public sealed class ParamPatcher : Patcher
    {
	public ParamPatcher(System.Type type) : base(type)
	{
	}

	public override void patch(Ice.Object v)
	{
            Debug.Assert(_type != null);
	    if(!_type.IsInstanceOfType(v))
	    {
                throw new System.InvalidCastException("expected element of type " + type()
                    + " but received " + v.GetType().FullName);
	    }
	    value = v;
	}

	public Ice.Object value;
    }

    public sealed class SequencePatcher : Patcher
    {
	public SequencePatcher(ICollection seq, System.Type type, int index) : base(type)
	{
	    _seq = seq;
	    _index = index;
	}
    
        private static object dummyObject = new object();

	public override void patch(Ice.Object v)
	{
            try 
            {
                if(_seq is IList)
                {
                    if(_index >= _seq.Count)
                    {
                        for(int i = _seq.Count; i <= _index; i++)
                        {
                            ((IList)_seq).Add(dummyObject); // IList implementation does not permit adding null :-(
                        }
                    }
                    ((IList)_seq)[_index] = v;
                }
                else
                {
                    ((System.Array)_seq).SetValue(v, _index);
                }
            }
            catch(System.Exception)
            {
                throw new System.InvalidCastException("expected element of type " + type()
                                                      + " but received " + v.GetType().FullName); 
            }
	}
    
	private ICollection _seq;
	private int _index;
    }
}
