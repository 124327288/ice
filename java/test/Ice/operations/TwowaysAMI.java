// **********************************************************************
//
// Copyright (c) 2002
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

class TwowaysAMI
{
    private static void
    test(boolean b)
    {
        if(!b)
        {
            throw new RuntimeException();
        }
    }

    private static class Callback
    {
	Callback()
	{
	    _called = false;
	}

	public synchronized boolean
	check()
	{
	    while(!_called)
	    {
		try
		{
		    wait(5000);
		}
		catch(InterruptedException ex)
		{
		    continue;
		}

		if(!_called)
		{
		    return false; // Must be timeout.
		}
	    }

	    return true;
	}
	
	public synchronized void
	called()
	{
	    assert(!_called);
	    _called = true;
	    notify();
	}

	private boolean _called;
    };

    private static class AMI_MyClass_opVoidI extends Test.AMI_MyClass_opVoid
    {
	public void
	ice_response()
	{
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opByteI extends Test.AMI_MyClass_opByte
    {
	public void
	ice_response(byte r, byte b)
	{
	    test(b == (byte)0xf0);
	    test(r == (byte)0xff);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opBoolI extends Test.AMI_MyClass_opBool
    {
	public void
	ice_response(boolean r, boolean b)
	{
	    test(b);
	    test(!r);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opShortIntLongI extends Test.AMI_MyClass_opShortIntLong
    {
	public void
	ice_response(long r, short s, int i, long l)
	{
	    test(s == 10);
	    test(i == 11);
	    test(l == 12);
	    test(r == 12);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opFloatDoubleI extends Test.AMI_MyClass_opFloatDouble
    {
	public void
	ice_response(double r, float f, double d)
	{
	    test(f == 3.14f);
	    test(d == 1.1E10);
	    test(r == 1.1E10);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStringI extends Test.AMI_MyClass_opString
    {
	public void
	ice_response(String r, String s)
	{
	    test(s.equals("world hello"));
	    test(r.equals("hello world"));
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opMyEnumI extends Test.AMI_MyClass_opMyEnum
    {
	public void
	ice_response(Test.MyEnum r, Test.MyEnum e)
	{
            test(e == Test.MyEnum.enum2);
            test(r == Test.MyEnum.enum3);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opMyClassI extends Test.AMI_MyClass_opMyClass
    {
	public void
	ice_response(Test.MyClassPrx r, Test.MyClassPrx c1, Test.MyClassPrx c2)
	{
            test(c1.ice_getIdentity().equals(Ice.Util.stringToIdentity("test")));
            test(c2.ice_getIdentity().equals(Ice.Util.stringToIdentity("noSuchIdentity")));
            test(r.ice_getIdentity().equals(Ice.Util.stringToIdentity("test")));
            r.opVoid();
            c1.opVoid();
            try
            {
                c2.opVoid();
                test(false);
            }
            catch(Ice.ObjectNotExistException ex)
            {
            }
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStructI extends Test.AMI_MyClass_opStruct
    {
	public void
	ice_response(Test.Structure rso, Test.Structure so)
	{
            test(rso.p == null);
            test(rso.e == Test.MyEnum.enum2);
            test(rso.s.s.equals("def"));
            test(so.e == Test.MyEnum.enum3);
            test(so.s.s.equals("a new string"));
            so.p.opVoid();
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opByteSI extends Test.AMI_MyClass_opByteS
    {
	public void
	ice_response(byte[] rso, byte[] bso)
	{
	    test(bso.length == 4);
	    test(bso[0] == (byte)0x22);
	    test(bso[1] == (byte)0x12);
	    test(bso[2] == (byte)0x11);
	    test(bso[3] == (byte)0x01);
	    test(rso.length == 8);
	    test(rso[0] == (byte)0x01);
	    test(rso[1] == (byte)0x11);
	    test(rso[2] == (byte)0x12);
	    test(rso[3] == (byte)0x22);
	    test(rso[4] == (byte)0xf1);
	    test(rso[5] == (byte)0xf2);
	    test(rso[6] == (byte)0xf3);
	    test(rso[7] == (byte)0xf4);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opBoolSI extends Test.AMI_MyClass_opBoolS
    {
	public void
	ice_response(boolean[] rso, boolean[] bso)
	{
	    test(bso.length == 4);
	    test(bso[0]);
	    test(bso[1]);
	    test(!bso[2]);
	    test(!bso[3]);
	    test(rso.length == 3);
	    test(!rso[0]);
	    test(rso[1]);
	    test(rso[2]);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opShortIntLongSI extends Test.AMI_MyClass_opShortIntLongS
    {
	public void
	ice_response(long[] rso, short[] sso, int[] iso,
		     long[] lso)
	{
	    test(sso.length == 3);
	    test(sso[0] == 1);
	    test(sso[1] == 2);
	    test(sso[2] == 3);
	    test(iso.length == 4);
	    test(iso[0] == 8);
	    test(iso[1] == 7);
	    test(iso[2] == 6);
	    test(iso[3] == 5);
	    test(lso.length == 6);
	    test(lso[0] == 10);
	    test(lso[1] == 30);
	    test(lso[2] == 20);
	    test(lso[3] == 10);
	    test(lso[4] == 30);
	    test(lso[5] == 20);
	    test(rso.length == 3);
	    test(rso[0] == 10);
	    test(rso[1] == 30);
	    test(rso[2] == 20);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opFloatDoubleSI extends Test.AMI_MyClass_opFloatDoubleS
    {
	public void
	ice_response(double[] rso, float[] fso, double[] dso)
	{
            test(fso.length == 2);
            test(fso[0] == 3.14f);
            test(fso[1] == 1.11f);
            test(dso.length == 3);
            test(dso[0] == 1.3E10);
            test(dso[1] == 1.2E10);
            test(dso[2] == 1.1E10);
            test(rso.length == 5);
            test(rso[0] == 1.1E10);
            test(rso[1] == 1.2E10);
            test(rso[2] == 1.3E10);
            test((float)rso[3] == 3.14f);
            test((float)rso[4] == 1.11f);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStringSI extends Test.AMI_MyClass_opStringS
    {
	public void
	ice_response(String[] rso, String[] sso)
	{
            test(sso.length == 4);
            test(sso[0].equals("abc"));
            test(sso[1].equals("de"));
            test(sso[2].equals("fghi"));
            test(sso[3].equals("xyz"));
            test(rso.length == 3);
            test(rso[0].equals("fghi"));
            test(rso[1].equals("de"));
            test(rso[2].equals("abc"));
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opByteSSI extends Test.AMI_MyClass_opByteSS
    {
	public void
	ice_response(byte[][] rso, byte[][] bso)
	{
	    test(bso.length == 2);
	    test(bso[0].length == 1);
	    test(bso[0][0] == (byte)0xff);
	    test(bso[1].length == 3);
	    test(bso[1][0] == (byte)0x01);
	    test(bso[1][1] == (byte)0x11);
	    test(bso[1][2] == (byte)0x12);
	    test(rso.length == 4);
	    test(rso[0].length == 3);
	    test(rso[0][0] == (byte)0x01);
	    test(rso[0][1] == (byte)0x11);
	    test(rso[0][2] == (byte)0x12);
	    test(rso[1].length == 1);
	    test(rso[1][0] == (byte)0xff);
	    test(rso[2].length == 1);
	    test(rso[2][0] == (byte)0x0e);
	    test(rso[3].length == 2);
	    test(rso[3][0] == (byte)0xf2);
	    test(rso[3][1] == (byte)0xf1);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opBoolSSI extends Test.AMI_MyClass_opBoolSS
    {
	public void
	ice_response(boolean[][] rso, boolean[][] bso)
	{
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opShortIntLongSSI extends Test.AMI_MyClass_opShortIntLongSS
    {
	public void
	ice_response(long[][] rso, short[][] sso, int[][] iso, long[][] lso)
	{
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opFloatDoubleSSI extends Test.AMI_MyClass_opFloatDoubleSS
    {
	public void
	ice_response(double[][] rso, float[][] fso, double[][] dso)
	{
	    test(fso.length == 3);
	    test(fso[0].length == 1);
	    test(fso[0][0] == 3.14f);
	    test(fso[1].length == 1);
	    test(fso[1][0] == 1.11f);
	    test(fso[2].length == 0);
	    test(dso.length == 1);
	    test(dso[0].length == 3);
	    test(dso[0][0] == 1.1E10);
	    test(dso[0][1] == 1.2E10);
	    test(dso[0][2] == 1.3E10);
	    test(rso.length == 2);
	    test(rso[0].length == 3);
	    test(rso[0][0] == 1.1E10);
	    test(rso[0][1] == 1.2E10);
	    test(rso[0][2] == 1.3E10);
	    test(rso[1].length == 3);
	    test(rso[1][0] == 1.1E10);
	    test(rso[1][1] == 1.2E10);
	    test(rso[1][2] == 1.3E10);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStringSSI extends Test.AMI_MyClass_opStringSS
    {
	public void
	ice_response(String[][] rso, String[][] sso)
	{
            test(sso.length == 5);
            test(sso[0].length == 1);
            test(sso[0][0].equals("abc"));
            test(sso[1].length == 2);
            test(sso[1][0].equals("de"));
            test(sso[1][1].equals("fghi"));
            test(sso[2].length == 0);
            test(sso[3].length == 0);
            test(sso[4].length == 1);
            test(sso[4][0].equals("xyz"));
            test(rso.length == 3);
            test(rso[0].length == 1);
            test(rso[0][0].equals("xyz"));
            test(rso[1].length == 0);
            test(rso[2].length == 0);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opByteBoolDI extends Test.AMI_MyClass_opByteBoolD
    {
	public void
	ice_response(java.util.Map ro, java.util.Map _do)
	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Byte((byte)10), Boolean.TRUE);
            di1.put(new Byte((byte)100), Boolean.FALSE);
            test(_do.equals(di1));
            test(ro.size() == 4);
            test(((Boolean)ro.get(new Byte((byte)10))).booleanValue() == true);
            test(((Boolean)ro.get(new Byte((byte)11))).booleanValue() == false);
            test(((Boolean)ro.get(new Byte((byte)100))).booleanValue() == false);
            test(((Boolean)ro.get(new Byte((byte)101))).booleanValue() == true);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opShortIntDI extends Test.AMI_MyClass_opShortIntD
    {
	public void
	ice_response(java.util.Map ro, java.util.Map _do)
	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Short((short)110), new Integer(-1));
            di1.put(new Short((short)1100), new Integer(123123));
            test(_do.equals(di1));
            test(ro.size() == 4);
            test(((Integer)ro.get(new Short((short)110))).intValue() == -1);
            test(((Integer)ro.get(new Short((short)111))).intValue() == -100);
            test(((Integer)ro.get(new Short((short)1100))).intValue() == 123123);
            test(((Integer)ro.get(new Short((short)1101))).intValue() == 0);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opLongFloatDI extends Test.AMI_MyClass_opLongFloatD
    {
	public void
	ice_response(java.util.Map ro, java.util.Map _do)
	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Long(999999110L), new Float(-1.1f));
            di1.put(new Long(9999991100L), new Float(123123.2f));
            test(_do.equals(di1));
            test(ro.size() == 4);
            test(((Float)ro.get(new Long(999999110L))).floatValue() == -1.1f);
            test(((Float)ro.get(new Long(999999111L))).floatValue() == -100.4f);
            test(((Float)ro.get(new Long(9999991100L))).floatValue() == 123123.2f);
            test(((Float)ro.get(new Long(9999991101L))).floatValue() == 0.5f);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStringStringDI extends Test.AMI_MyClass_opStringStringD
    {
	public void
	ice_response(java.util.Map ro, java.util.Map _do)
	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put("foo", "abc -1.1");
            di1.put("bar", "abc 123123.2");
            test(_do.equals(di1));
            test(ro.size() == 4);
            test(((String)ro.get("foo")).equals("abc -1.1"));
            test(((String)ro.get("FOO")).equals("abc -100.4"));
            test(((String)ro.get("bar")).equals("abc 123123.2"));
            test(((String)ro.get("BAR")).equals("abc 0.5"));
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyClass_opStringMyEnumDI extends Test.AMI_MyClass_opStringMyEnumD
    {
	public void
	ice_response(java.util.Map ro, java.util.Map _do)
	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put("abc", Test.MyEnum.enum1);
            di1.put("", Test.MyEnum.enum2);
            test(_do.equals(di1));
            test(ro.size() == 4);
            test(((Test.MyEnum)ro.get("abc")) == Test.MyEnum.enum1);
            test(((Test.MyEnum)ro.get("qwerty")) == Test.MyEnum.enum3);
            test(((Test.MyEnum)ro.get("")) == Test.MyEnum.enum2);
            test(((Test.MyEnum)ro.get("Hello!!")) == Test.MyEnum.enum2);
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    private static class AMI_MyDerivedClass_opDerivedI extends Test.AMI_MyDerivedClass_opDerived
    {
	public void
	ice_response()
	{
	    callback.called();
	}

	public void
	ice_exception(Ice.LocalException ex)
	{
	    test(false);
	}

	public boolean
	check()
	{
	    return callback.check();
	}

	private Callback callback = new Callback();
    };

    static void
    twowaysAMI(Test.MyClassPrx p)
    {
	{
	    AMI_MyClass_opVoidI cb = new AMI_MyClass_opVoidI();
	    p.opVoid_async(cb);
	    test(cb.check());
	}

	{
	    AMI_MyClass_opByteI cb = new AMI_MyClass_opByteI();
	    p.opByte_async(cb, (byte)0xff, (byte)0x0f);
	    test(cb.check());
	}

	{
	    AMI_MyClass_opBoolI cb = new AMI_MyClass_opBoolI();
	    p.opBool_async(cb, true, false);
	    test(cb.check());
	}
	
	{
	    AMI_MyClass_opShortIntLongI cb = new AMI_MyClass_opShortIntLongI();
	    p.opShortIntLong_async(cb, (short)10, 11, 12L);
	    test(cb.check());
	}
	
	{
	    AMI_MyClass_opFloatDoubleI cb = new AMI_MyClass_opFloatDoubleI();
	    p.opFloatDouble_async(cb, 3.14f, 1.1E10);
	    test(cb.check());
	}
	
	{
	    AMI_MyClass_opStringI cb = new AMI_MyClass_opStringI();
	    p.opString_async(cb, "hello", "world");
	    test(cb.check());
	}
	
	{
	    AMI_MyClass_opMyEnumI cb = new AMI_MyClass_opMyEnumI();
	    p.opMyEnum_async(cb, Test.MyEnum.enum2);
	    test(cb.check());
	}
	
	{
	    AMI_MyClass_opMyClassI cb = new AMI_MyClass_opMyClassI();
	    p.opMyClass_async(cb, p);
	    test(cb.check());
	}
	
	{
            Test.Structure si1 = new Test.Structure();
            si1.p = p;
            si1.e = Test.MyEnum.enum3;
            si1.s = new Test.AnotherStruct();
            si1.s.s = "abc";
            Test.Structure si2 = new Test.Structure();
            si2.p = null;
            si2.e = Test.MyEnum.enum2;
            si2.s = new Test.AnotherStruct();
            si2.s.s = "def";
	    
	    AMI_MyClass_opStructI cb = new AMI_MyClass_opStructI();
	    p.opStruct_async(cb, si1, si2);
	    test(cb.check());
	}
	
	{
            final byte[] bsi1 =
		{
		    (byte)0x01,
		    (byte)0x11,
		    (byte)0x12,
		    (byte)0x22
		};
            final byte[] bsi2 =
		{
		    (byte)0xf1,
		    (byte)0xf2,
		    (byte)0xf3,
		    (byte)0xf4
		};
	    
	    AMI_MyClass_opByteSI cb = new AMI_MyClass_opByteSI();
	    p.opByteS_async(cb, bsi1, bsi2);
	    test(cb.check());
	}
	
	{
            final boolean[] bsi1 = { true, true, false };
            final boolean[] bsi2 = { false };
	    
	    AMI_MyClass_opBoolSI cb = new AMI_MyClass_opBoolSI();
	    p.opBoolS_async(cb, bsi1, bsi2);
	    test(cb.check());
	}
	
	{
            final short[] ssi = { 1, 2, 3 };
            final int[] isi = { 5, 6, 7, 8 };
            final long[] lsi = { 10, 30, 20 };
	    
	    AMI_MyClass_opShortIntLongSI cb = new AMI_MyClass_opShortIntLongSI();
	    p.opShortIntLongS_async(cb, ssi, isi, lsi);
	    test(cb.check());
	}
	
	{
            final float[] fsi = { 3.14f, 1.11f };
            final double[] dsi = { 1.1E10, 1.2E10, 1.3E10 };
	    
	    AMI_MyClass_opFloatDoubleSI cb = new AMI_MyClass_opFloatDoubleSI();
	    p.opFloatDoubleS_async(cb, fsi, dsi);
	    test(cb.check());
	}
	
	{
            final String[] ssi1 = { "abc", "de", "fghi" };
            final String[] ssi2 = { "xyz" };
	    
	    AMI_MyClass_opStringSI cb = new AMI_MyClass_opStringSI();
	    p.opStringS_async(cb, ssi1, ssi2);
	    test(cb.check());
	}
	
	{
            final byte[][] bsi1 =
		{
		    { (byte)0x01, (byte)0x11, (byte)0x12 },
		    { (byte)0xff }
		};
            final byte[][] bsi2 =
		{
		    { (byte)0x0e },
		    { (byte)0xf2, (byte)0xf1 }
		};

	    AMI_MyClass_opByteSSI cb = new AMI_MyClass_opByteSSI();
	    p.opByteSS_async(cb, bsi1, bsi2);
	    test(cb.check());
	}

	{
            final float[][] fsi =
		{
		    { 3.14f },
		    { 1.11f },
		    { },
		};
            final double[][] dsi =
		{
		    { 1.1E10, 1.2E10, 1.3E10 }
		};

	    AMI_MyClass_opFloatDoubleSSI cb = new AMI_MyClass_opFloatDoubleSSI();
	    p.opFloatDoubleSS_async(cb, fsi, dsi);
	    test(cb.check());
	}

	{
            final String[][] ssi1 =
		{
		    { "abc" },
		    { "de", "fghi" }
		};
            final String[][] ssi2 =
		{
		    { },
		    { },
		    { "xyz" }
		};

	    AMI_MyClass_opStringSSI cb = new AMI_MyClass_opStringSSI();
	    p.opStringSS_async(cb, ssi1, ssi2);
	    test(cb.check());
	}

	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Byte((byte)10), Boolean.TRUE);
            di1.put(new Byte((byte)100), Boolean.FALSE);
            java.util.Map di2 = new java.util.HashMap();
            di2.put(new Byte((byte)10), Boolean.TRUE);
            di2.put(new Byte((byte)11), Boolean.FALSE);
            di2.put(new Byte((byte)101), Boolean.TRUE);

	    AMI_MyClass_opByteBoolDI cb = new AMI_MyClass_opByteBoolDI();
	    p.opByteBoolD_async(cb, di1, di2);
	    test(cb.check());
	}

	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Short((short)110), new Integer(-1));
            di1.put(new Short((short)1100), new Integer(123123));
            java.util.Map di2 = new java.util.HashMap();
            di2.put(new Short((short)110), new Integer(-1));
            di2.put(new Short((short)111), new Integer(-100));
            di2.put(new Short((short)1101), new Integer(0));

	    AMI_MyClass_opShortIntDI cb = new AMI_MyClass_opShortIntDI();
	    p.opShortIntD_async(cb, di1, di2);
	    test(cb.check());
	}

	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put(new Long(999999110L), new Float(-1.1f));
            di1.put(new Long(9999991100L), new Float(123123.2f));
            java.util.Map di2 = new java.util.HashMap();
            di2.put(new Long(999999110L), new Float(-1.1f));
            di2.put(new Long(999999111L), new Float(-100.4f));
            di2.put(new Long(9999991101L), new Float(0.5f));

	    AMI_MyClass_opLongFloatDI cb = new AMI_MyClass_opLongFloatDI();
	    p.opLongFloatD_async(cb, di1, di2);
	    test(cb.check());
	}

	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put("foo", "abc -1.1");
            di1.put("bar", "abc 123123.2");
            java.util.Map di2 = new java.util.HashMap();
            di2.put("foo", "abc -1.1");
            di2.put("FOO", "abc -100.4");
            di2.put("BAR", "abc 0.5");

	    AMI_MyClass_opStringStringDI cb = new AMI_MyClass_opStringStringDI();
	    p.opStringStringD_async(cb, di1, di2);
	    test(cb.check());
	}

	{
            java.util.Map di1 = new java.util.HashMap();
            di1.put("abc", Test.MyEnum.enum1);
            di1.put("", Test.MyEnum.enum2);
            java.util.Map di2 = new java.util.HashMap();
            di2.put("abc", Test.MyEnum.enum1);
            di2.put("qwerty", Test.MyEnum.enum3);
            di2.put("Hello!!", Test.MyEnum.enum2);

	    AMI_MyClass_opStringMyEnumDI cb = new AMI_MyClass_opStringMyEnumDI();
	    p.opStringMyEnumD_async(cb, di1, di2);
	    test(cb.check());
	}

	{
	    Test.MyDerivedClassPrx derived = Test.MyDerivedClassPrxHelper.checkedCast(p);
	    test(derived != null);
	    AMI_MyDerivedClass_opDerivedI cb = new AMI_MyDerivedClass_opDerivedI();
	    derived.opDerived_async(cb);
	    test(cb.check());
	}
    }
};
