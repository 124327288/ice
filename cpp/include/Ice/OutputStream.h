// **********************************************************************
//
// Copyright (c) 2003-2015 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef ICE_OUTPUT_STREAM_H
#define ICE_OUTPUT_STREAM_H

#include <IceUtil/StringConverter.h>
#include <Ice/CommunicatorF.h>
#include <Ice/InstanceF.h>
#include <Ice/Object.h>
#include <Ice/ValueF.h>
#include <Ice/ProxyF.h>
#include <Ice/Buffer.h>
#include <Ice/Protocol.h>
#include <Ice/SlicedDataF.h>
#include <Ice/StreamHelpers.h>
#include <Ice/Traits.h>

namespace Ice
{

class UserException;

class ICE_API OutputStream : public IceInternal::Buffer
{
public:

    typedef size_t size_type;

    //
    // Constructing an OutputStream without providing a communicator means the stream will
    // use the default encoding version, the default format for class encoding, and will not
    // use string converters. You can supply a communicator later by calling initialize().
    //
    OutputStream();

    //
    // This constructor uses the communicator's default encoding version.
    //
    OutputStream(const CommunicatorPtr&);

    //
    // This constructor uses the given communicator and encoding version.
    //
    OutputStream(const CommunicatorPtr&, const EncodingVersion&);

    ~OutputStream()
    {
        // Inlined for performance reasons.

        if(_currentEncaps != &_preAllocatedEncaps)
        {
            clear(); // Not inlined.
        }
    }

    //
    // Initializes the stream to use the communicator's default encoding version, class
    // encoding format, and string converters.
    //
    void initialize(const CommunicatorPtr&);

    //
    // Initializes the stream to use the given encoding version and the communicator's
    // default class encoding format and string converters.
    //
    void initialize(const CommunicatorPtr&, const EncodingVersion&);

    void clear();

    //
    // Must return Instance*, because we don't hold an InstancePtr for
    // optimization reasons (see comments below).
    //
    IceInternal::Instance* instance() const { return _instance; } // Inlined for performance reasons.

    void setStringConverters(const IceUtil::StringConverterPtr&, const IceUtil::WstringConverterPtr&);

    void setFormat(FormatType);

    void* getClosure() const;
    void* setClosure(void*);

    void swap(OutputStream&);
    void resetEncapsulation();

    void resize(Container::size_type sz)
    {
        b.resize(sz);
        i = b.end();
    }

    void startObject(const SlicedDataPtr& data)
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->startInstance(ObjectSlice, data);
    }
    void endObject()
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->endInstance();
    }

    void startException(const SlicedDataPtr& data)
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->startInstance(ExceptionSlice, data);
    }
    void endException()
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->endInstance();
    }

    void startEncapsulation();

    void startEncapsulation(const EncodingVersion& encoding, FormatType format)
    {
        IceInternal::checkSupportedEncoding(encoding);

        Encaps* oldEncaps = _currentEncaps;
        if(!oldEncaps) // First allocated encaps?
        {
            _currentEncaps = &_preAllocatedEncaps;
        }
        else
        {
            _currentEncaps = new Encaps();
            _currentEncaps->previous = oldEncaps;
        }
        _currentEncaps->format = format;
        _currentEncaps->encoding = encoding;
        _currentEncaps->start = b.size();

        write(Int(0)); // Placeholder for the encapsulation length.
        write(_currentEncaps->encoding);
    }
    void endEncapsulation()
    {
        assert(_currentEncaps);

        // Size includes size and version.
        const Int sz = static_cast<Int>(b.size() - _currentEncaps->start);
        write(sz, &(*(b.begin() + _currentEncaps->start)));

        Encaps* oldEncaps = _currentEncaps;
        _currentEncaps = _currentEncaps->previous;
        if(oldEncaps == &_preAllocatedEncaps)
        {
            oldEncaps->reset();
        }
        else
        {
            delete oldEncaps;
        }
    }

    void writeEmptyEncapsulation(const EncodingVersion& encoding)
    {
        IceInternal::checkSupportedEncoding(encoding);
        write(Int(6)); // Size
        write(encoding);
    }
    void writeEncapsulation(const Byte* v, Int sz)
    {
        if(sz < 6)
        {
            throwEncapsulationException(__FILE__, __LINE__);
        }

        Container::size_type position = b.size();
        resize(position + sz);
        memcpy(&b[position], &v[0], sz);
    }

    const EncodingVersion& getEncoding() const
    {
        return _currentEncaps ? _currentEncaps->encoding : _encoding;
    }

    void startSlice(const std::string& typeId, int compactId, bool last)
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->startSlice(typeId, compactId, last);
    }
    void endSlice()
    {
        assert(_currentEncaps && _currentEncaps->encoder);
        _currentEncaps->encoder->endSlice();
    }

    void writePendingObjects();

    void writeSize(Int v) // Inlined for performance reasons.
    {
        assert(v >= 0);
        if(v > 254)
        {
            write(Byte(255));
            write(v);
        }
        else
        {
            write(static_cast<Byte>(v));
        }
    }
    void rewriteSize(Int v, Container::iterator dest)
    {
        assert(v >= 0);
        if(v > 254)
        {
            *dest++ = Byte(255);
            write(v, dest);
        }
        else
        {
            *dest = static_cast<Byte>(v);
        }
    }

    size_type startSize()
    {
        size_type position = b.size();
        write(Int(0));
        return position;
    }

    void endSize(size_type position)
    {
        rewrite(static_cast<Int>(b.size() - position) - 4, position);
    }

    void writeBlob(const std::vector<Byte>&);

    void writeBlob(const Byte* v, Container::size_type sz)
    {
        if(sz > 0)
        {
            Container::size_type position = b.size();
            resize(position + sz);
            memcpy(&b[position], &v[0], sz);
        }
    }

    template<typename T> void write(const T& v)
    {
        StreamHelper<T, StreamableTraits<T>::helper>::write(this, v);
    }

    template<typename T> void write(Int tag, const IceUtil::Optional<T>& v)
    {
        if(!v)
        {
            return; // Optional not set
        }

        if(writeOpt(tag, StreamOptionalHelper<T,
                                                   StreamableTraits<T>::helper,
                                                   StreamableTraits<T>::fixedLength>::optionalFormat))
        {
            StreamOptionalHelper<T,
                                      StreamableTraits<T>::helper,
                                      StreamableTraits<T>::fixedLength>::write(this, *v);
        }
    }

    //
    // Template functions for sequences and custom sequences
    //
    template<typename T> void write(const std::vector<T>& v)
    {
        if(v.empty())
        {
            writeSize(0);
        }
        else
        {
            write(&v[0], &v[0] + v.size());
        }
    }
    template<typename T> void write(const T* begin, const T* end)
    {
        writeSize(static_cast<Int>(end - begin));
        for(const T* p = begin; p != end; ++p)
        {
            write(*p);
        }
    }

    // Write type and tag for optionals
    bool writeOpt(Int tag, OptionalFormat format)
    {
        assert(_currentEncaps);
        if(_currentEncaps->encoder)
        {
            return _currentEncaps->encoder->writeOpt(tag, format);
        }
        else
        {
            return writeOptImpl(tag, format);
        }
    }

    // Byte
    void write(Byte v)
    {
        b.push_back(v);
    }
    void write(const Byte*, const Byte*);

    // Bool
    void write(bool v)
    {
        b.push_back(static_cast<Byte>(v));
    }
    void write(const std::vector<bool>&);
    void write(const bool*, const bool*);

    // Short
    void write(Short);
    void write(const Short*, const Short*);

    // Int
    void write(Int v) // Inlined for performance reasons.
    {
        Container::size_type position = b.size();
        resize(position + sizeof(Int));
        write(v, &b[position]);
    }
    void write(Int v, Container::iterator dest)
    {
#ifdef ICE_BIG_ENDIAN
        const Byte* src = reinterpret_cast<const Byte*>(&v) + sizeof(Int) - 1;
        *dest++ = *src--;
        *dest++ = *src--;
        *dest++ = *src--;
        *dest = *src;
#else
        const Byte* src = reinterpret_cast<const Byte*>(&v);
        *dest++ = *src++;
        *dest++ = *src++;
        *dest++ = *src++;
        *dest = *src;
#endif
    }

    void write(const Int*, const Int*);

    // Long

#ifdef ICE_CPP11_MAPPING
    void write(long long int);
#else
    void write(Long);
#endif

    void write(const Long*, const Long*);

    // Float
    void write(Float);
    void write(const Float*, const Float*);

    // Double
    void write(Double);
    void write(const Double*, const Double*);

    // String
    void write(const std::string& v, bool convert = true)
    {
        Int sz = static_cast<Int>(v.size());
        if(convert && sz > 0 && _stringConverter != 0)
        {
            writeConverted(v.data(), static_cast<size_t>(sz));
        }
        else
        {
            writeSize(sz);
            if(sz > 0)
            {
                Container::size_type position = b.size();
                resize(position + sz);
                memcpy(&b[position], v.data(), sz);
            }
        }
    }

    // for custom strings
    void write(const char* vdata, size_t vsize, bool convert = true)
    {
        Int sz = static_cast<Int>(vsize);
        if(convert && sz > 0 && _stringConverter != 0)
        {
            writeConverted(vdata, vsize);
        }
        else
        {
            writeSize(sz);
            if(sz > 0)
            {
                Container::size_type position = b.size();
                resize(position + sz);
                memcpy(&b[position], vdata, vsize);
            }
        }
    }

    // Null-terminated C string
    void write(const char* vdata, bool convert = true)
    {
        write(vdata, strlen(vdata), convert);
    }

    void write(const std::string*, const std::string*, bool = true);

    void write(const std::wstring& v);
    void write(const std::wstring*, const std::wstring*);

    // Proxy
#ifdef ICE_CPP11_MAPPING
    void writeProxy(const ObjectPrxPtr&);

    template<typename T, typename ::std::enable_if<::std::is_base_of<ObjectPrx, T>::value>::type* = nullptr>
    void write(const ::std::shared_ptr<T>& v)
    {
        writeProxy(::std::static_pointer_cast<ObjectPrx>(v));
    }
#else
    void write(const ObjectPrx&);
    template<typename T> void write(const IceInternal::ProxyHandle<T>& v)
    {
        write(ObjectPrx(upCast(v.get())));
    }
#endif

    // Class
#ifdef ICE_CPP11_MAPPING // C++11 mapping
    template<typename T, typename ::std::enable_if<::std::is_base_of<Value, T>::value>::type* = nullptr>
    void write(const ::std::shared_ptr<T>& v)
    {
        initEncaps();
        _currentEncaps->encoder->write(v);
    }
#else // C++98 mapping
    void write(const ObjectPtr& v)
    {
        initEncaps();
        _currentEncaps->encoder->write(v);
    }
    template<typename T> void write(const IceInternal::Handle<T>& v)
    {
        write(ObjectPtr(upCast(v.get())));
    }
#endif

    // Enum
    void writeEnum(Int, Int);

    // Exception
    void writeException(const UserException&);

    // Optionals
    bool writeOptImpl(Int, OptionalFormat);

    size_type pos()
    {
        return i - b.begin();
    }

    void rewrite(Int value, size_type p)
    {
        write(value, b.begin() + p);
    }

    OutputStream(IceInternal::Instance*, const EncodingVersion&);

    void initialize(IceInternal::Instance*, const EncodingVersion&);

    void finished(std::vector<Byte>&);
    virtual std::pair<const Byte*, const Byte*> finished();

private:

    //
    // String
    //
    void writeConverted(const char*, size_t);

    //
    // We can't throw this exception from inline functions from within
    // this file, because we cannot include the header with the
    // exceptions. Doing so would screw up the whole include file
    // ordering.
    //
    void throwEncapsulationException(const char*, int);

    //
    // Optimization. The instance may not be deleted while a
    // stack-allocated stream still holds it.
    //
    IceInternal::Instance* _instance;

    //
    // The public stream API needs to attach data to a stream.
    //
    void* _closure;

    class Encaps;
    enum SliceType { NoSlice, ObjectSlice, ExceptionSlice };

    typedef std::vector<ValuePtr> ObjectList;

    class ICE_API EncapsEncoder : private ::IceUtil::noncopyable
    {
    public:

        virtual ~EncapsEncoder() { }

        virtual void write(const ValuePtr&) = 0;
        virtual void write(const UserException&) = 0;

        virtual void startInstance(SliceType, const SlicedDataPtr&) = 0;
        virtual void endInstance() = 0;
        virtual void startSlice(const std::string&, int, bool) = 0;
        virtual void endSlice() = 0;

        virtual bool writeOpt(Int, OptionalFormat)
        {
            return false;
        }

        virtual void writePendingObjects()
        {
        }

    protected:

        EncapsEncoder(OutputStream* stream, Encaps* encaps) : _stream(stream), _encaps(encaps), _typeIdIndex(0)
        {
        }

        Int registerTypeId(const std::string&);

        OutputStream* _stream;
        Encaps* _encaps;

        typedef std::map<ValuePtr, Int> PtrToIndexMap;
        typedef std::map<std::string, Int> TypeIdMap;

        // Encapsulation attributes for object marshalling.
        PtrToIndexMap _marshaledMap;

    private:

        // Encapsulation attributes for object marshalling.
        TypeIdMap _typeIdMap;
        Int _typeIdIndex;
    };

    class ICE_API EncapsEncoder10 : public EncapsEncoder
    {
    public:

        EncapsEncoder10(OutputStream* stream, Encaps* encaps) :
            EncapsEncoder(stream, encaps), _sliceType(NoSlice), _objectIdIndex(0)
        {
        }

        virtual void write(const ValuePtr&);
        virtual void write(const UserException&);

        virtual void startInstance(SliceType, const SlicedDataPtr&);
        virtual void endInstance();
        virtual void startSlice(const std::string&, int, bool);
        virtual void endSlice();

        virtual void writePendingObjects();

    private:

        Int registerObject(const ValuePtr&);

        // Instance attributes
        SliceType _sliceType;

        // Slice attributes
        Container::size_type _writeSlice; // Position of the slice data members

        // Encapsulation attributes for object marshalling.
        Int _objectIdIndex;
        PtrToIndexMap _toBeMarshaledMap;
    };

    class ICE_API EncapsEncoder11 : public EncapsEncoder
    {
    public:

        EncapsEncoder11(OutputStream* stream, Encaps* encaps) :
            EncapsEncoder(stream, encaps), _preAllocatedInstanceData(0), _current(0), _objectIdIndex(1)
        {
        }

        virtual void write(const ValuePtr&);
        virtual void write(const UserException&);

        virtual void startInstance(SliceType, const SlicedDataPtr&);
        virtual void endInstance();
        virtual void startSlice(const std::string&, int, bool);
        virtual void endSlice();

        virtual bool writeOpt(Int, OptionalFormat);

    private:

        void writeSlicedData(const SlicedDataPtr&);
        void writeInstance(const ValuePtr&);

        struct InstanceData
        {
            InstanceData(InstanceData* p) : previous(p), next(0)
            {
                if(previous)
                {
                    previous->next = this;
                }
            }

            ~InstanceData()
            {
                if(next)
                {
                    delete next;
                }
            }

            // Instance attributes
            SliceType sliceType;
            bool firstSlice;

            // Slice attributes
            Byte sliceFlags;
            Container::size_type writeSlice;    // Position of the slice data members
            Container::size_type sliceFlagsPos; // Position of the slice flags
            PtrToIndexMap indirectionMap;
            ObjectList indirectionTable;

            InstanceData* previous;
            InstanceData* next;
        };
        InstanceData _preAllocatedInstanceData;
        InstanceData* _current;

        Int _objectIdIndex; // The ID of the next object to marhsal
    };

    class Encaps : private ::IceUtil::noncopyable
    {

    public:

        Encaps() : format(DefaultFormat), encoder(0), previous(0)
        {
            // Inlined for performance reasons.
        }
        ~Encaps()
        {
            // Inlined for performance reasons.
            delete encoder;
        }
        void reset()
        {
            // Inlined for performance reasons.
            delete encoder;
            encoder = 0;

            previous = 0;
        }

        Container::size_type start;
        EncodingVersion encoding;
        FormatType format;

        EncapsEncoder* encoder;

        Encaps* previous;
    };

    //
    // The encoding version to use when there's no encapsulation to
    // read from or write to. This is for example used to read message
    // headers or when the user is using the streaming API with no
    // encapsulation.
    //
    EncodingVersion _encoding;

    FormatType _format;

    Encaps* _currentEncaps;

    void initEncaps();

    Encaps _preAllocatedEncaps;

    IceUtil::StringConverterPtr _stringConverter;
    IceUtil::WstringConverterPtr _wstringConverter;
};

} // End namespace Ice

#endif
