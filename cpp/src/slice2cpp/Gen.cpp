// **********************************************************************
//
// Copyright (c) 2003-2015 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <IceUtil/DisableWarnings.h>
#include "Gen.h"
#include <Slice/Util.h>
#include <Slice/CPlusPlusUtil.h>
#include <IceUtil/Functional.h>
#include <IceUtil/Iterator.h>
#include <Slice/Checksum.h>
#include <Slice/FileTracker.h>

#include <limits>

#include <sys/stat.h>
#include <string.h>

using namespace std;
using namespace Slice;
using namespace IceUtil;
using namespace IceUtilInternal;

namespace
{

bool
isConstexprType(const TypePtr& type)
{
    BuiltinPtr bp = BuiltinPtr::dynamicCast(type);
    if(bp)
    {
        switch(bp->kind())
        {
            case Builtin::KindByte:
            case Builtin::KindBool:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            case Builtin::KindFloat:
            case Builtin::KindDouble:
            case Builtin::KindValue:
            case Builtin::KindObject:
            case Builtin::KindObjectProxy:
            {
                return true;
            }
            default:
            {
                return false;
            }
        }
    }
    else if(EnumPtr::dynamicCast(type) || ProxyPtr::dynamicCast(type) || ClassDeclPtr::dynamicCast(type))
    {
        return true;
    }
    else
    {
        StructPtr s = StructPtr::dynamicCast(type);
        if(s)
        {
            DataMemberList members = s->dataMembers();
            for(DataMemberList::const_iterator i = members.begin(); i != members.end(); ++i)
            {
                if(!isConstexprType((*i)->type()))
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
}

string
getDeprecateSymbol(const ContainedPtr& p1, const ContainedPtr& p2)
{
    string deprecateMetadata, deprecateSymbol;
    if(p1->findMetaData("deprecate", deprecateMetadata) ||
       (p2 != 0 && p2->findMetaData("deprecate", deprecateMetadata)))
    {
        string msg = "is deprecated";
        if(deprecateMetadata.find("deprecate:") == 0 && deprecateMetadata.size() > 10)
        {
            msg = deprecateMetadata.substr(10);
        }
        deprecateSymbol = "ICE_DEPRECATED_API(\"" + msg + "\") ";
    }
    return deprecateSymbol;
}

void
writeConstantValue(IceUtilInternal::Output& out, const TypePtr& type, const SyntaxTreeBasePtr& valueType,
                   const string& value, int useWstring, const StringList& metaData, bool cpp11 = false)
{
    ConstPtr constant = ConstPtr::dynamicCast(valueType);
    if(constant)
    {
        out << fixKwd(constant->scoped());
    }
    else
    {
        BuiltinPtr bp = BuiltinPtr::dynamicCast(type);
        if(bp && bp->kind() == Builtin::KindString)
        {
            //
            // Expand strings into the basic source character set. We can't use isalpha() and the like
            // here because they are sensitive to the current locale.
            //
            static const string basicSourceChars = "abcdefghijklmnopqrstuvwxyz"
                                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                   "0123456789"
                                                   "_{}[]#()<>%:;.?*+-/^&|~!=,\\\"' ";
            static const set<char> charSet(basicSourceChars.begin(), basicSourceChars.end());

            if((useWstring & TypeContextUseWstring) || findMetaData(metaData) == "wstring")
            {
                out << 'L';
            }
            out << "\"";                                    // Opening "

            for(string::const_iterator c = value.begin(); c != value.end(); ++c)
            {
                if(charSet.find(*c) == charSet.end())
                {
                    unsigned char uc = *c;                  // char may be signed, so make it positive
                    ostringstream s;
                    s << "\\";                              // Print as octal if not in basic source character set
                    s.width(3);
                    s.fill('0');
                    s << oct;
                    s << static_cast<unsigned>(uc);
                    out << s.str();
                }
                else
                {
                    switch(*c)
                    {
                        case '\\':
                        case '"':
                        {
                            out << "\\";
                            break;
                        }
                    }
                    out << *c;                              // Print normally if in basic source character set
                }
            }

            out << "\"";                                    // Closing "
        }
        else if(bp && bp->kind() == Builtin::KindLong)
        {
            if(cpp11)
            {
                out << value << "LL";
            }
            else
            {
                out << "ICE_INT64(" << value << ")";
            }
        }
        else if(bp && bp->kind() == Builtin::KindFloat)
        {
            out << value;
            if(value.find(".") == string::npos)
            {
                out << ".0";
            }
            out << "F";
        }
        else
        {
            EnumPtr ep = EnumPtr::dynamicCast(type);
            if(ep)
            {
                bool unscoped = findMetaData(ep->getMetaData()) == "%unscoped";
                if(!cpp11 || unscoped)
                {
                    out << fixKwd(value);
                }
                else
                {
                    string v = value;
                    string scope;
                    size_t pos = value.rfind("::");
                    if(pos != string::npos)
                    {
                        v = value.substr(pos + 2);
                        scope = value.substr(0, value.size() - v.size());
                    }

                    out << fixKwd(scope + ep->name() + "::" + v);
                }
            }
            else
            {
                out << value;
            }
        }
    }
}

void
writeMarshalUnmarshalDataMember(IceUtilInternal::Output& C, const DataMemberPtr& p, bool marshal)
{
    writeMarshalUnmarshalCode(C, p->type(), p->optional(), p->tag(), fixKwd(p->name()), marshal, p->getMetaData());
}

void
writeMarshalUnmarshalDataMemberInHolder(IceUtilInternal::Output& C, const string& holder, const DataMemberPtr& p, bool marshal)
{
    writeMarshalUnmarshalCode(C, p->type(), p->optional(), p->tag(), holder + fixKwd(p->name()), marshal, p->getMetaData());
}

void
writeMarshalUnmarshalDataMembers(IceUtilInternal::Output& C,
                                 const DataMemberList& dataMembers,
                                 const DataMemberList& optionalDataMembers,
                                 bool marshal)
{
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        if(!(*q)->optional())
        {
            writeMarshalUnmarshalDataMember(C, *q, marshal);
        }
    }
    for(DataMemberList::const_iterator q = optionalDataMembers.begin(); q != optionalDataMembers.end(); ++q)
    {
        writeMarshalUnmarshalDataMember(C, *q, marshal);
    }
}

void
writeDataMemberInitializers(IceUtilInternal::Output& C, const DataMemberList& members, int useWstring, bool cpp11 = false)
{
    bool first = true;
    for(DataMemberList::const_iterator p = members.begin(); p != members.end(); ++p)
    {
        if((*p)->defaultValueType())
        {
            string memberName = fixKwd((*p)->name());

            if(first)
            {
                first = false;
            }
            else
            {
                C << ',';
            }
            C << nl << memberName << '(';
            writeConstantValue(C, (*p)->type(), (*p)->defaultValueType(), (*p)->defaultValue(), useWstring,
                               (*p)->getMetaData(), cpp11);
            C << ')';
        }
    }
}

}

Slice::Gen::Gen(const string& base, const string& headerExtension, const string& sourceExtension,
                const vector<string>& extraHeaders, const string& include,
                const vector<string>& includePaths, const string& dllExport, const string& dir,
                bool implCpp98, bool implCpp11, bool checksum, bool ice) :
    _base(base),
    _headerExtension(headerExtension),
    _implHeaderExtension(headerExtension),
    _sourceExtension(sourceExtension),
    _extraHeaders(extraHeaders),
    _include(include),
    _includePaths(includePaths),
    _dllExport(dllExport),
    _dir(dir),
    _implCpp98(implCpp98),
    _implCpp11(implCpp11),
    _checksum(checksum),
    _ice(ice)
{
    for(vector<string>::iterator p = _includePaths.begin(); p != _includePaths.end(); ++p)
    {
        *p = fullPath(*p);
    }

    string::size_type pos = _base.find_last_of("/\\");
    if(pos != string::npos)
    {
        _base.erase(0, pos + 1);
    }
}

Slice::Gen::~Gen()
{
    H << "\n\n#include <IceUtil/PopDisableWarnings.h>";
    H << "\n#endif\n";
    C << '\n';

    if(_implCpp98 || _implCpp11)
    {
        implH << "\n\n#endif\n";
        implC << '\n';
    }
}

void
Slice::Gen::generateChecksumMap(const UnitPtr& p)
{
    if(_checksum)
    {
        ChecksumMap map = createChecksums(p);
        if(!map.empty())
        {
            C << sp << nl << "namespace";
            C << nl << "{";
            C << sp << nl << "const char* __sliceChecksums[] =";
            C << sb;
            for(ChecksumMap::const_iterator q = map.begin(); q != map.end(); ++q)
            {
                C << nl << "\"" << q->first << "\", \"";
                ostringstream str;
                str.flags(ios_base::hex);
                str.fill('0');
                for(vector<unsigned char>::const_iterator r = q->second.begin(); r != q->second.end(); ++r)
                {
                    str << static_cast<int>(*r);
                }
                C << str.str() << "\",";
            }
            C << nl << "0";
            C << eb << ';';
            C << nl << "const IceInternal::SliceChecksumInit __sliceChecksumInit(__sliceChecksums);";
            C << sp << nl << "}";
        }
    }
}

void
Slice::Gen::generate(const UnitPtr& p)
{
    string file = p->topLevelFile();

    //
    // Give precedence to header-ext global metadata.
    //
    string headerExtension = getHeaderExt(file, p);
    if(!headerExtension.empty())
    {
        _headerExtension = headerExtension;
    }

    if(_implCpp98 || _implCpp11)
    {
        string fileImplH = _base + "I." + _implHeaderExtension;
        string fileImplC = _base + "I." + _sourceExtension;
        if(!_dir.empty())
        {
            fileImplH = _dir + '/' + fileImplH;
            fileImplC = _dir + '/' + fileImplC;
        }

        struct stat st;
        if(stat(fileImplH.c_str(), &st) == 0)
        {
            ostringstream os;
            os << fileImplH << "' already exists - will not overwrite";
            throw FileException(__FILE__, __LINE__, os.str());
        }
        if(stat(fileImplC.c_str(), &st) == 0)
        {
            ostringstream os;
            os << fileImplC << "' already exists - will not overwrite";
            throw FileException(__FILE__, __LINE__, os.str());
        }

        implH.open(fileImplH.c_str());
        if(!implH)
        {
            ostringstream os;
            os << "cannot open `" << fileImplH << "': " << strerror(errno);
            throw FileException(__FILE__, __LINE__, os.str());
        }
        FileTracker::instance()->addFile(fileImplH);

        implC.open(fileImplC.c_str());
        if(!implC)
        {
            ostringstream os;
            os << "cannot open `" << fileImplC << "': " << strerror(errno);
            throw FileException(__FILE__, __LINE__, os.str());
        }
        FileTracker::instance()->addFile(fileImplC);

        string s = _base + "I." + _implHeaderExtension;
        if(_include.size())
        {
            s = _include + '/' + s;
        }
        transform(s.begin(), s.end(), s.begin(), ToIfdef());
        implH << "#ifndef __" << s << "__";
        implH << "\n#define __" << s << "__";
        implH << '\n';
    }

    string fileH = _base + "." + _headerExtension;
    string fileC = _base + "." + _sourceExtension;
    if(!_dir.empty())
    {
        fileH = _dir + '/' + fileH;
        fileC = _dir + '/' + fileC;
    }

    H.open(fileH.c_str());
    if(!H)
    {
        ostringstream os;
        os << "cannot open `" << fileH << "': " << strerror(errno);
        throw FileException(__FILE__, __LINE__, os.str());
    }
    FileTracker::instance()->addFile(fileH);

    C.open(fileC.c_str());
    if(!C)
    {
        ostringstream os;
        os << "cannot open `" << fileC << "': " << strerror(errno);
        throw FileException(__FILE__, __LINE__, os.str());
    }
    FileTracker::instance()->addFile(fileC);

    printHeader(H);
    printGeneratedHeader(H, _base + ".ice");
    printHeader(C);
    printGeneratedHeader(C, _base + ".ice");


    string s = _base + "." + _headerExtension;;
    if(_include.size())
    {
        s = _include + '/' + s;
    }
    transform(s.begin(), s.end(), s.begin(), ToIfdef());
    H << "\n#ifndef __" << s << "__";
    H << "\n#define __" << s << "__";
    H << '\n';

    validateMetaData(p);

    writeExtraHeaders(C);

    if(_dllExport.size())
    {
        C << "\n#ifndef " << _dllExport << "_EXPORTS";
        C << "\n#   define " << _dllExport << "_EXPORTS";
        C << "\n#endif";
    }

    C << "\n#include <";
    if(_include.size())
    {
        C << _include << '/';
    }
    C << _base << "." << _headerExtension << ">";
    C <<  "\n#include <IceUtil/PushDisableWarnings.h>";

    H << "\n#include <IceUtil/PushDisableWarnings.h>";
    H << "\n#include <Ice/ProxyF.h>";
    H << "\n#include <Ice/ObjectF.h>";
    H << "\n#include <Ice/ValueF.h>";
    H << "\n#include <Ice/Exception.h>";
    H << "\n#include <Ice/LocalObject.h>";
    H << "\n#include <Ice/StreamHelpers.h>";

    if(p->hasNonLocalClassDefs())
    {
        H << "\n#include <Ice/Proxy.h>";
        H << "\n#include <Ice/Object.h>";
        H << "\n#include <Ice/GCObject.h>";
        H << "\n#include <Ice/Value.h>";
        H << "\n#include <Ice/Incoming.h>";
        if(p->hasContentsWithMetaData("amd"))
        {
            H << "\n#include <Ice/IncomingAsync.h>";
        }
        C << "\n#include <Ice/LocalException.h>";
        C << "\n#include <Ice/ValueFactory.h>";
        C << "\n#include <Ice/Outgoing.h>";
        C << "\n#include <Ice/OutgoingAsync.h>";
    }
    else if(p->hasLocalClassDefsWithAsync())
    {
        H << "\n#include <Ice/OutgoingAsync.h>";
    }
    else if(p->hasNonLocalClassDecls())
    {
        H << "\n#include <Ice/Proxy.h>";
    }

    if(p->hasNonLocalClassDefs() || p->hasNonLocalExceptions())
    {
        H << "\n#include <Ice/FactoryTableInit.h>";
    }

    H << "\n#include <IceUtil/ScopedArray.h>";
    H << "\n#include <IceUtil/Optional.h>";

    if(p->usesNonLocals())
    {
        C << "\n#include <Ice/InputStream.h>";
        C << "\n#include <Ice/OutputStream.h>";
    }

    if(p->hasNonLocalExceptions())
    {
        C << "\n#include <Ice/LocalException.h>";
    }

    if(p->hasContentsWithMetaData("preserve-slice"))
    {
        H << "\n#include <Ice/SlicedDataF.h>";
        C << "\n#include <Ice/SlicedData.h>";
    }

    if(_checksum)
    {
        C << "\n#include <Ice/SliceChecksums.h>";
    }

    C << "\n#include <IceUtil/Iterator.h>";
    C << "\n#include <IceUtil/PopDisableWarnings.h>";

    StringList includes = p->includeFiles();

    for(StringList::const_iterator q = includes.begin(); q != includes.end(); ++q)
    {
        string extension = getHeaderExt((*q), p);
        if(extension.empty())
        {
            extension = _headerExtension;
        }
        H << "\n#include <" << changeInclude(*q, _includePaths) << "." << extension << ">";
    }

    H << "\n#include <IceUtil/UndefSysMacros.h>";

    //
    // Emit #include statements for any cpp:include metadata directives
    // in the top-level Slice file.
    //
    {
        DefinitionContextPtr dc = p->findDefinitionContext(file);
        assert(dc);
        StringList globalMetaData = dc->getMetaData();
        for(StringList::const_iterator q = globalMetaData.begin(); q != globalMetaData.end(); ++q)
        {
            string s = *q;
            static const string includePrefix = "cpp:include:";
            if(s.find(includePrefix) == 0 && s.size() > includePrefix.size())
            {
                H << nl << "#include <" << s.substr(includePrefix.size()) << ">";
            }
        }
    }

    printVersionCheck(H);
    printVersionCheck(C);

    printDllExportStuff(H, _dllExport);
    if(_dllExport.size())
    {
        _dllExport += " ";
    }

    H << sp;
    H.zeroIndent();
    H << nl << "#ifdef ICE_CPP11_MAPPING // C++11 mapping";
    H.restoreIndent();

    C << sp;
    C.zeroIndent();
    C << nl << "#ifdef ICE_CPP11_MAPPING // C++11 mapping";
    C.restoreIndent();
    {
        Cpp11DeclVisitor declVisitor(H, C, _dllExport);
        p->visit(&declVisitor, false);

        Cpp11TypesVisitor typesVisitor(H, C, _dllExport);
        p->visit(&typesVisitor, false);

        Cpp11StreamVisitor streamVisitor(H, C, _dllExport);
        p->visit(&streamVisitor, false);

        Cpp11ProxyVisitor proxyVisitor(H, C, _dllExport);
        p->visit(&proxyVisitor, false);

        Cpp11LocalObjectVisitor localObjectVisitor(H, C, _dllExport);
        p->visit(&localObjectVisitor, false);

        Cpp11InterfaceVisitor interfaceVisitor(H, C, _dllExport);
        p->visit(&interfaceVisitor, false);

        Cpp11ValueVisitor valueVisitor(H, C, _dllExport);
        p->visit(&valueVisitor, false);

        if(_implCpp11)
        {
            implH << "\n#include <";
            if(_include.size())
            {
                implH << _include << '/';
            }
            implH << _base << "." << _headerExtension << ">";
            implH << nl << "//base";
            writeExtraHeaders(implC);

            implC << "\n#include <";
            if(_include.size())
            {
                implC << _include << '/';
            }
            implC << _base << "I." << _implHeaderExtension << ">";

            Cpp11ImplVisitor implVisitor(implH, implC, _dllExport);
            p->visit(&implVisitor, false);
        }

        Cpp11CompatibilityVisitor compatibilityVisitor(H, C, _dllExport);
        p->visit(&compatibilityVisitor, false);

        generateChecksumMap(p);
    }
    H << sp;
    H.zeroIndent();
    H << nl << "#else // C++98 mapping";
    H.restoreIndent();

    C << sp;
    C.zeroIndent();
    C << nl << "#else // C++98 mapping";
    C.restoreIndent();
    {
        ProxyDeclVisitor proxyDeclVisitor(H, C, _dllExport);
        p->visit(&proxyDeclVisitor, false);

        ObjectDeclVisitor objectDeclVisitor(H, C, _dllExport);
        p->visit(&objectDeclVisitor, false);

        TypesVisitor typesVisitor(H, C, _dllExport);
        p->visit(&typesVisitor, false);

        StreamVisitor streamVisitor(H, C, _dllExport);
        p->visit(&streamVisitor, false);

        AsyncVisitor asyncVisitor(H, C, _dllExport);
        p->visit(&asyncVisitor, false);

        AsyncImplVisitor asyncImplVisitor(H, C, _dllExport);
        p->visit(&asyncImplVisitor, false);

        //
        // The templates are emitted before the proxy definition
        // so the derivation hierarchy is known to the proxy:
        // the proxy relies on knowing the hierarchy to make the begin_
        // methods type-safe.
        //
        AsyncCallbackVisitor asyncCallbackVisitor(H, C, _dllExport);
        p->visit(&asyncCallbackVisitor, false);

        ProxyVisitor proxyVisitor(H, C, _dllExport);
        p->visit(&proxyVisitor, false);

        ObjectVisitor objectVisitor(H, C, _dllExport);
        p->visit(&objectVisitor, false);

        //
        // We need to delay generating the template after the proxy
        // definition, because completed calls the begin_ method in the
        // proxy.
        //
        AsyncCallbackTemplateVisitor asyncCallbackTemplateVisitor(H, C, _dllExport);
        p->visit(&asyncCallbackTemplateVisitor, false);

        if(_implCpp98)
        {
            implH << "\n#include <";
            if(_include.size())
            {
                implH << _include << '/';
            }
            implH << _base << "." << _headerExtension << ">";

            writeExtraHeaders(implC);

            implC << "\n#include <";
            if(_include.size())
            {
                implC << _include << '/';
            }
            implC << _base << "I." << _implHeaderExtension << ">";

            ImplVisitor implVisitor(implH, implC, _dllExport);
            p->visit(&implVisitor, false);
        }

        generateChecksumMap(p);
    }

    H << sp;
    H.zeroIndent();
    H << nl << "#endif";
    H.restoreIndent();

    C << sp;
    C.zeroIndent();
    C << nl << "#endif";
    C.restoreIndent();
}

void
Slice::Gen::closeOutput()
{
    H.close();
    C.close();
    implH.close();
    implC.close();
}

void
Slice::Gen::writeExtraHeaders(IceUtilInternal::Output& out)
{
    for(vector<string>::const_iterator i = _extraHeaders.begin(); i != _extraHeaders.end(); ++i)
    {
        string hdr = *i;
        string guard;
        string::size_type pos = hdr.rfind(',');
        if(pos != string::npos)
        {
            hdr = i->substr(0, pos);
            guard = i->substr(pos + 1);
        }
        if(!guard.empty())
        {
            out << "\n#ifndef " << guard;
            out << "\n#define " << guard;
        }
        out << "\n#include <";
        out << hdr << '>';
        if(!guard.empty())
        {
            out << "\n#endif";
        }
    }
}

Slice::Gen::TypesVisitor::TypesVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _doneStaticSymbol(false), _useWstring(false)
{
}

bool
Slice::Gen::TypesVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasOtherConstructedOrExceptions())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    H << sp << nl << "namespace " << fixKwd(p->name()) << nl << '{';

    return true;
}

void
Slice::Gen::TypesVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::TypesVisitor::visitClassDefStart(const ClassDefPtr&)
{
    return false;
}

bool
Slice::Gen::TypesVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    ExceptionPtr base = p->base();
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();
    bool hasDefaultValues = p->hasDefaultValues();

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;
    vector<string> baseParams;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
        allTypes.push_back(typeName);
        allParamDecls.push_back(typeName + " __ice_" + fixKwd((*q)->name()));
    }

    if(base)
    {
        DataMemberList baseDataMembers = base->allDataMembers();
        for(DataMemberList::const_iterator q = baseDataMembers.begin(); q != baseDataMembers.end(); ++q)
        {
            baseParams.push_back("__ice_" + fixKwd((*q)->name()));
        }
    }

    H << sp << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    H << "public ";
    if(!base)
    {
        H << (p->isLocal() ? "::Ice::LocalException" : "::Ice::UserException");
    }
    else
    {
        H << fixKwd(base->scoped());
    }
    H.restoreIndent();
    H << sb;

    H.dec();
    H << nl << "public:";
    H.inc();

    H << sp << nl << name << spar;
    if(p->isLocal())
    {
        H << "const char*" << "int";
    }
    H << epar;
    if(!p->isLocal() && !hasDefaultValues)
    {
        H << " {}";
    }
    else
    {
        H << ';';
    }
    if(!allTypes.empty())
    {
        H << nl;
        if(!p->isLocal() && allTypes.size() == 1)
        {
            H << "explicit ";
        }
        H << name << spar;
        if(p->isLocal())
        {
            H << "const char*" << "int";
        }
        H << allTypes << epar << ';';
    }
    H << nl << "virtual ~" << name << "() throw();";
    H << sp;

    if(!p->isLocal())
    {
        string initName = p->flattenedScope() + p->name() + "_init";

        C << sp << nl << "namespace";
        C << nl << "{";

        C << sp << nl << "const ::IceInternal::DefaultUserExceptionFactoryInit< " << scoped << "> "
          << initName << "(\"" << p->scoped() << "\");";

        C << sp << nl << "}";
    }

    if(p->isLocal())
    {
        C << sp << nl << scoped.substr(2) << "::" << name << spar << "const char* __file" << "int __line" << epar
          << " :";
        C.inc();
        emitUpcall(base, "(__file, __line)", true);
        if(p->hasDefaultValues())
        {
            C << ", ";
            writeDataMemberInitializers(C, dataMembers, _useWstring);
        }
        C.dec();
        C << sb;
        C << eb;
    }
    else if(hasDefaultValues)
    {
        C << sp << nl << scoped.substr(2) << "::" << name << "() :";
        C.inc();
        writeDataMemberInitializers(C, dataMembers, _useWstring);
        C.dec();
        C << sb;
        C << eb;
    }

    if(!allTypes.empty())
    {
        C << sp << nl;
        C << scoped.substr(2) << "::" << name << spar;
        if(p->isLocal())
        {
            C << "const char* __file" << "int __line";
        }
        C << allParamDecls << epar;
        if(p->isLocal() || !baseParams.empty() || !params.empty())
        {
            C << " :";
            C.inc();
            string upcall;
            if(!allParamDecls.empty())
            {
                upcall = "(";
                if(p->isLocal())
                {
                    upcall += "__file, __line";
                }
                for(vector<string>::const_iterator pi = baseParams.begin(); pi != baseParams.end(); ++pi)
                {
                    if(p->isLocal() || pi != baseParams.begin())
                    {
                        upcall += ", ";
                    }
                    upcall += *pi;
                }
                upcall += ")";
            }
            if(!params.empty())
            {
                upcall += ",";
            }
            emitUpcall(base, upcall, p->isLocal());
        }
        for(vector<string>::const_iterator pi = params.begin(); pi != params.end(); ++pi)
        {
            if(pi != params.begin())
            {
                C << ",";
            }
            C << nl << *pi << "(__ice_" << *pi << ')';
        }
        if(p->isLocal() || !baseParams.empty() || !params.empty())
        {
            C.dec();
        }
        C << sb;
        C << eb;
    }

    C << sp << nl;
    C << scoped.substr(2) << "::~" << name << "() throw()";
    C << sb;
    C << eb;

    H << nl << "virtual ::std::string ice_id() const;";
    C << sp << nl << "::std::string" << nl << scoped.substr(2) << "::ice_id() const";
    C << sb;
    C << nl << "return \"" << p->scoped() << "\";";
    C << eb;

    StringList metaData = p->getMetaData();
    if(find(metaData.begin(), metaData.end(), "cpp:ice_print") != metaData.end())
    {
        H << nl << "virtual void ice_print(::std::ostream&) const;";
    }

    H << nl << "virtual " << name << "* ice_clone() const;";
    C << sp << nl << scoped.substr(2) << "*" << nl << scoped.substr(2) << "::ice_clone() const";
    C << sb;
    C << nl << "return new " << name << "(*this);";
    C << eb;

    H << nl << "virtual void ice_throw() const;";
    C << sp << nl << "void" << nl << scoped.substr(2) << "::ice_throw() const";
    C << sb;
    C << nl << "throw *this;";
    C << eb;

    if(!p->isLocal() && p->usesClasses(false))
    {
        if(!base || (base && !base->usesClasses(false)))
        {
            H << sp << nl << "virtual bool __usesClasses() const;";

            C << sp << nl << "bool";
            C << nl << scoped.substr(2) << "::__usesClasses() const";
            C << sb;
            C << nl << "return true;";
            C << eb;
        }
    }

    if(!dataMembers.empty())
    {
        H << sp;
    }
    return true;
}

void
Slice::Gen::TypesVisitor::visitExceptionEnd(const ExceptionPtr& p)
{
    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    string factoryName;

    if(!p->isLocal())
    {
        ExceptionPtr base = p->base();
        bool basePreserved = p->inheritsMetaData("preserve-slice");
        bool preserved = p->hasMetaData("preserve-slice");

        if(preserved && !basePreserved)
        {
            H << sp << nl << "virtual void __write(::Ice::OutputStream*) const;";
            H << nl << "virtual void __read(::Ice::InputStream*);";

            string baseName = base ? fixKwd(base->scoped()) : string("::Ice::UserException");
            H << nl << "using " << baseName << "::__write;";
            H << nl << "using " << baseName << "::__read;";
        }

        H.dec();
        H << sp << nl << "protected:";
        H.inc();

        H << nl << "virtual void __writeImpl(::Ice::OutputStream*) const;";
        H << nl << "virtual void __readImpl(::Ice::InputStream*);";

        string baseName = base ? fixKwd(base->scoped()) : string("::Ice::UserException");
        //H << nl << "using " << baseName << "::__writeImpl;";
        //H << nl << "using " << baseName << "::__readImpl;";

        if(preserved && !basePreserved)
        {

            H << sp << nl << "::Ice::SlicedDataPtr __slicedData;";

            C << sp << nl << "void" << nl << scoped.substr(2) << "::__write(::Ice::OutputStream* __os) const";
            C << sb;
            C << nl << "__os->startException(__slicedData);";
            C << nl << "__writeImpl(__os);";
            C << nl << "__os->endException();";
            C << eb;

            C << sp << nl << "void" << nl << scoped.substr(2) << "::__read(::Ice::InputStream* __is)";
            C << sb;
            C << nl << "__is->startException();";
            C << nl << "__readImpl(__is);";
            C << nl << "__slicedData = __is->endException(true);";
            C << eb;
        }

        C << sp << nl << "void" << nl << scoped.substr(2) << "::__writeImpl(::Ice::OutputStream* __os) const";
        C << sb;
        C << nl << "__os->startSlice(\"" << p->scoped() << "\", -1, " << (!base ? "true" : "false") << ");";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), true);
        C << nl << "__os->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__writeImpl(__os);");
        }
        C << eb;

        C << sp << nl << "void" << nl << scoped.substr(2) << "::__readImpl(::Ice::InputStream* __is)";
        C << sb;
        C << nl << "__is->startSlice();";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), false);
        C << nl << "__is->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__readImpl(__is);");
        }
        C << eb;
    }
    H << eb << ';';

    if(!p->isLocal())
    {
        //
        // We need an instance here to trigger initialization if the implementation is in a static library.
        // But we do this only once per source file, because a single instance is sufficient to initialize
        // all of the globals in a compilation unit.
        //
        if(!_doneStaticSymbol)
        {
            _doneStaticSymbol = true;
            H << sp << nl << "static " << name << " __" << p->name() << "_init;";
        }
    }

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::TypesVisitor::visitStructStart(const StructPtr& p)
{
    DataMemberList dataMembers = p->dataMembers();
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    bool classMetaData = findMetaData(p->getMetaData()) == "%class";
    if(classMetaData)
    {
        H << sp << nl << "class " << _dllExport << name << " : public IceUtil::Shared";
        H << sb;
        H.dec();
        H << nl << "public:";
        H.inc();
        H << nl;
        if(p->hasDefaultValues())
        {
            H << nl << name << "() :";
            H.inc();
            writeDataMemberInitializers(H, dataMembers, _useWstring);
            H.dec();
            H << sb;
            H << eb;
        }
        else
        {
            H << nl << name << "() {}";
        }
    }
    else
    {
        H << sp << nl << "struct " << name;
        H << sb;
        if(p->hasDefaultValues())
        {
            H << nl << name << "() :";

            H.inc();
            writeDataMemberInitializers(H, dataMembers, _useWstring);
            H.dec();
            H << sb;
            H << eb << nl;
        }
    }

    //
    // Generate a one-shot constructor if the struct uses the class mapping, or if at least
    // one of its members has a default value.
    //
    if(!dataMembers.empty() && (findMetaData(p->getMetaData()) == "%class" || p->hasDefaultValues()))
    {
        vector<string> paramDecls;
        vector<string> types;
        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
            types.push_back(typeName);
            paramDecls.push_back(typeName + " __ice_" + (*q)->name());
        }

        H << nl;
        if(!classMetaData)
        {
            H << _dllExport;
        }
        if(paramDecls.size() == 1)
        {
            H << "explicit ";
        }
        H << fixKwd(p->name()) << spar << paramDecls << epar << " :";
        H.inc();

        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            if(q != dataMembers.begin())
            {
                H << ',';
            }
            string memberName = fixKwd((*q)->name());
            H << nl << memberName << '(' << "__ice_" << (*q)->name() << ')';
        }

        H.dec();
        H << sb;
        H << eb;
        H << nl;
    }

    H << sp;

    return true;
}

void
Slice::Gen::TypesVisitor::visitStructEnd(const StructPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    DataMemberList dataMembers = p->dataMembers();

    vector<string> params;
    vector<string>::const_iterator pi;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    bool containsSequence = false;
    if((Dictionary::legalKeyType(p, containsSequence) && !containsSequence) || p->hasMetaData("cpp:comparable"))
    {
        H << sp << nl << "bool operator==(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "if(this == &__rhs)";
        H << sb;
        H << nl << "return true;";
        H << eb;
        for(vector<string>::const_iterator pi = params.begin(); pi != params.end(); ++pi)
        {
            H << nl << "if(" << *pi << " != __rhs." << *pi << ')';
            H << sb;
            H << nl << "return false;";
            H << eb;
        }
        H << nl << "return true;";
        H << eb;
        H << sp << nl << "bool operator<(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "if(this == &__rhs)";
        H << sb;
        H << nl << "return false;";
        H << eb;
        for(vector<string>::const_iterator pi = params.begin(); pi != params.end(); ++pi)
        {
            H << nl << "if(" << *pi << " < __rhs." << *pi << ')';
            H << sb;
            H << nl << "return true;";
            H << eb;
            H << nl << "else if(__rhs." << *pi << " < " << *pi << ')';
            H << sb;
            H << nl << "return false;";
            H << eb;
        }
        H << nl << "return false;";
        H << eb;

        H << sp << nl << "bool operator!=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator==(__rhs);";
        H << eb;
        H << nl << "bool operator<=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return operator<(__rhs) || operator==(__rhs);";
        H << eb;
        H << nl << "bool operator>(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator<(__rhs) && !operator==(__rhs);";
        H << eb;
        H << nl << "bool operator>=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator<(__rhs);";
        H << eb;
    }
    H << eb << ';';

    if(findMetaData(p->getMetaData()) == "%class")
    {
        H << sp << nl << "typedef ::IceUtil::Handle< " << scoped << "> " << p->name() + "Ptr;";
    }

    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::TypesVisitor::visitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    H << nl << typeToString(p->type(), p->optional(), p->getMetaData(), _useWstring) << ' ' << name << ';';
}

void
Slice::Gen::TypesVisitor::visitSequence(const SequencePtr& p)
{
    string name = fixKwd(p->name());
    TypePtr type = p->type();
    string s = typeToString(type, p->typeMetaData(), _useWstring);
    StringList metaData = p->getMetaData();

    string seqType = findMetaData(metaData, _useWstring);
    H << sp;

    if(!seqType.empty())
    {
        H << nl << "typedef " << seqType << ' ' << name << ';';
    }
    else
    {
        H << nl << "typedef ::std::vector<" << (s[0] == ':' ? " " : "") << s << "> " << name << ';';
    }
}

void
Slice::Gen::TypesVisitor::visitDictionary(const DictionaryPtr& p)
{
    string name = fixKwd(p->name());
    string dictType = findMetaData(p->getMetaData());

    if(dictType.empty())
    {
        //
        // A default std::map dictionary
        //

        TypePtr keyType = p->keyType();
        TypePtr valueType = p->valueType();
        string ks = typeToString(keyType, p->keyMetaData(), _useWstring);
        if(ks[0] == ':')
        {
            ks.insert(0, " ");
        }
        string vs = typeToString(valueType, p->valueMetaData(), _useWstring);

        H << sp << nl << "typedef ::std::map<" << ks << ", " << vs << "> " << name << ';';
    }
    else
    {
        //
        // A custom dictionary
        //
        H << sp << nl << "typedef " << dictType << ' ' << name << ';';
    }
}

void
Slice::Gen::TypesVisitor::visitEnum(const EnumPtr& p)
{
    string name = fixKwd(p->name());
    EnumeratorList enumerators = p->getEnumerators();

    //
    // Check if any of the enumerators were assigned an explicit value.
    //
    const bool explicitValue = p->explicitValue();

    H << sp << nl << "enum " << name;
    H << sb;

    EnumeratorList::const_iterator en = enumerators.begin();
    while(en != enumerators.end())
    {
        H << nl << fixKwd((*en)->name());
        //
        // If any of the enumerators were assigned an explicit value, we emit
        // an explicit value for *all* enumerators.
        //
        if(explicitValue)
        {
            H << " = " << int64ToString((*en)->value());
        }
        if(++en != enumerators.end())
        {
            H << ',';
        }
    }
    H << eb << ';';
}

void
Slice::Gen::TypesVisitor::visitConst(const ConstPtr& p)
{
    H << sp;
    H << nl << "const " << typeToString(p->type(), p->typeMetaData(), _useWstring) << " " << fixKwd(p->name())
      << " = ";
    writeConstantValue(H, p->type(), p->valueType(), p->value(), _useWstring, p->typeMetaData());
    H << ';';
}

void
Slice::Gen::TypesVisitor::emitUpcall(const ExceptionPtr& base, const string& call, bool isLocal)
{
    C << nl << (base ? fixKwd(base->scoped()) : string(isLocal ? "::Ice::LocalException" : "::Ice::UserException"))
      << call;
}

Slice::Gen::ProxyDeclVisitor::ProxyDeclVisitor(Output& h, Output&, const string& dllExport) :
    H(h), _dllExport(dllExport)
{
}

bool
Slice::Gen::ProxyDeclVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    H << sp << nl << "namespace IceProxy" << nl << '{';

    return true;
}

void
Slice::Gen::ProxyDeclVisitor::visitUnitEnd(const UnitPtr&)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::ProxyDeclVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
        return false;
    }

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ProxyDeclVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';
}

void
Slice::Gen::ProxyDeclVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    if(p->isLocal())
    {
        return;
    }

    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());

    H << sp << nl << "class " << name << ';';
    H << nl << _dllExport << "void __read(::Ice::InputStream*, ::IceInternal::ProxyHandle< ::IceProxy"
      << scoped << ">&);";
    H << nl << _dllExport << "::IceProxy::Ice::Object* upCast(::IceProxy" << scoped << "*);";
}

Slice::Gen::ProxyVisitor::ProxyVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::ProxyVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDefs())
    {
        return false;
    }

    H << sp << nl << "namespace IceProxy" << nl << '{';

    return true;
}

void
Slice::Gen::ProxyVisitor::visitUnitEnd(const UnitPtr&)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::ProxyVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ProxyVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::ProxyVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    if(bases.size() > 1)
    {
        //
        // Generated helper class to deal with multiple inheritance
        // when using Proxy template.
        //
        H << sp << nl << "class _" << _dllExport << fixKwd(p->name() + "PrxBase") << " : ";
        H.useCurrentPosAsIndent();
        for(ClassList::const_iterator q = bases.begin(); q != bases.end();)
        {
            H << "public virtual ::IceProxy" << fixKwd((*q)->scoped());
            if(++q != bases.end())
            {
                H << ", " << nl;
            }
        }
        H.restoreIndent();
        H << sb;

        H.dec();
        H << nl << "public:";
        H.inc();

        H << sp << nl << "virtual Object* __newInstance() const = 0;";
        H << eb << ';';
    }

    H << sp << nl << "class " << _dllExport << name << " : ";
    H << "public virtual ::IceProxy::Ice::Proxy< ::IceProxy" << scoped << ", ";
    if(bases.empty())
    {
        H << "::IceProxy::Ice::Object";
    }
    else if(bases.size() == 1)
    {
        H << "::IceProxy" << fixKwd(bases.front()->scoped());
    }
    else
    {
        H << "_" << fixKwd(p->name() + "PrxBase");
    }
    H << ">";

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    if(_dllExport != "")
    {
        //
        // To export the virtual table
        //
        C << nl << "#ifdef __SUNPRO_CC";
        C << nl << "class " << _dllExport
          << "IceProxy" << scoped << ";";
        C << nl << "#endif";
    }

    C << nl
      << _dllExport
      << "::IceProxy::Ice::Object* ::IceProxy" << scope << "upCast(::IceProxy" << scoped << "* p) { return p; }";

    C << sp;
    C << nl << "void" << nl << "::IceProxy" << scope << "__read(::Ice::InputStream* __is, "
      << "::IceInternal::ProxyHandle< ::IceProxy" << scoped << ">& v)";
    C << sb;
    C << nl << "::Ice::ObjectPrx proxy;";
    C << nl << "__is->read(proxy);";
    C << nl << "if(!proxy)";
    C << sb;
    C << nl << "v = 0;";
    C << eb;
    C << nl << "else";
    C << sb;
    C << nl << "v = new ::IceProxy" << scoped << ';';
    C << nl << "v->__copyFrom(proxy);";
    C << eb;
    C << eb;

    return true;
}

void
Slice::Gen::ProxyVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    H << nl << nl << "static const ::std::string& ice_staticId();";
    H << eb << ';';

    C << sp;
    C << nl << "const ::std::string&" << nl << "IceProxy" << scoped << "::ice_staticId()";
    C << sb;
    C << nl << "return "<< scoped << "::ice_staticId();";
    C << eb;

    _useWstring = resetUseWstring(_useWstringHist);
}

namespace
{

bool
usePrivateEnd(const OperationPtr& p)
{
    TypePtr ret = p->returnType();
    bool retIsOpt = p->returnIsOptional();
    string retSEnd = returnTypeToString(ret, retIsOpt, p->getMetaData(), TypeContextAMIEnd);
    string retSPrivateEnd = returnTypeToString(ret, retIsOpt, p->getMetaData(), TypeContextAMIPrivateEnd);

    ParamDeclList outParams;
    vector<string> outDeclsEnd;
    vector<string> outDeclsPrivateEnd;

    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if((*q)->isOutParam())
        {
            outDeclsEnd.push_back(outputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(),
                                                     TypeContextAMIEnd));
            outDeclsPrivateEnd.push_back(outputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(),
                                                            TypeContextAMIPrivateEnd));
        }
    }

    return retSEnd != retSPrivateEnd || outDeclsEnd != outDeclsPrivateEnd;
}

}

void
Slice::Gen::ProxyVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string flatName = p->flattenedScope() + p->name() + "_name";
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    TypePtr ret = p->returnType();

    bool retIsOpt = p->returnIsOptional();
    string retS = returnTypeToString(ret, retIsOpt, p->getMetaData(), _useWstring | TypeContextAMIEnd);
    string retSEndAMI = returnTypeToString(ret, retIsOpt, p->getMetaData(), _useWstring | TypeContextAMIPrivateEnd);
    string retInS = retS != "void" ? inputTypeToString(ret, retIsOpt, p->getMetaData(), _useWstring) : "";

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    string clName = cl->name();
    string clScope = fixKwd(cl->scope());
    string delName = "Callback_" + clName + "_" + name;
    string delNameScoped = clScope + delName;

    vector<string> params;
    vector<string> paramsDecl;
    vector<string> args;

    vector<string> paramsAMI;
    vector<string> paramsDeclAMI;
    vector<string> argsAMI;
    vector<string> outParamsAMI;
    vector<string> outParamNamesAMI;
    vector<string> outParamsDeclAMI;
    vector<string> outParamsDeclEndAMI;
    vector<string> outDecls;

    ParamDeclList paramList = p->parameters();
    ParamDeclList inParams;
    ParamDeclList outParams;


    vector<string> outEndArgs;

    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd(paramPrefix + (*q)->name());

        StringList metaData = (*q)->getMetaData();
        string typeString;
        string typeStringEndAMI;
        if((*q)->isOutParam())
        {
            typeString = outputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring | TypeContextAMIEnd);
            typeStringEndAMI = outputTypeToString((*q)->type(), (*q)->optional(), metaData,
                                                  _useWstring | TypeContextAMIPrivateEnd);
        }
        else
        {
            typeString = inputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring);
        }

        params.push_back(typeString);
        paramsDecl.push_back(typeString + ' ' + paramName);
        args.push_back(paramName);

        if(!(*q)->isOutParam())
        {
            paramsAMI.push_back(typeString);
            paramsDeclAMI.push_back(typeString + ' ' + paramName);
            argsAMI.push_back(paramName);
            inParams.push_back(*q);
        }
        else
        {
            outParamsAMI.push_back(typeString);
            outParamNamesAMI.push_back(paramName);
            outParamsDeclAMI.push_back(typeString + ' ' + paramName);
            outParamsDeclEndAMI.push_back(typeStringEndAMI + ' ' + paramName);
            outParams.push_back(*q);
            outDecls.push_back(inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring));
            outEndArgs.push_back(getEndArg((*q)->type(), (*q)->getMetaData(), outParamNamesAMI.back()));
        }
    }

    //
    // Check if we need to generate a private ___end_ method. This is the case if the
    // when using certain mapping features such as cpp:array or cpp:range:array. While
    // the regular end_ method can't return pair<const TYPE*, const TYPE*> because the
    // pointers would be invalid once end_ returns, we still want to allow using this
    // alternate mapping with AMI response callbacks (to allow zero-copy for instance).
    // For this purpose, we generate a special ___end method which is used by the
    // completed implementation of the generated Callback_Inft_opName operation
    // delegate.
    //
    bool generatePrivateEnd = retS != retSEndAMI || outParamsDeclAMI != outParamsDeclEndAMI;
    if(ret && generatePrivateEnd)
    {
        string typeStringEndAMI = outputTypeToString(ret, p->returnIsOptional(), p->getMetaData(),
                                                     _useWstring | TypeContextAMIPrivateEnd);
        outParamsDeclEndAMI.push_back(typeStringEndAMI + ' ' + "__ret");
    }

    string thisPointer = fixKwd(scope.substr(0, scope.size() - 2)) + "*";

    string deprecateSymbol = getDeprecateSymbol(p, cl);
    H << nl << deprecateSymbol << retS << ' ' << fixKwd(name) << spar << paramsDecl
      << "const ::Ice::Context& __ctx = ::Ice::noExplicitContext" << epar << ";";

    H << sp << nl << "::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
      << "const ::Ice::Context& __ctx = ::Ice::noExplicitContext" << epar;
    H << sb;
    H << nl << "return __begin_" << name << spar << argsAMI << "__ctx" << "::IceInternal::__dummyCallback" << "0"
      << epar << ';';
    H << eb;

    H << sp << nl << "::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
      << "const ::Ice::CallbackPtr& __del"
      << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar;
    H << sb;
    H << nl << "return __begin_" << name << spar << argsAMI << "::Ice::noExplicitContext" << "__del" << "__cookie" << epar << ';';
    H << eb;

    H << sp << nl << "::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
      << "const ::Ice::Context& __ctx"
      << "const ::Ice::CallbackPtr& __del"
      << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar;
    H << sb;
    H << nl << "return __begin_" << name << spar << argsAMI << "__ctx" << "__del" << "__cookie" << epar << ';';
    H << eb;

    H << sp << nl << "::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
      << "const " + delNameScoped + "Ptr& __del"
      << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar;
    H << sb;
    H << nl << "return __begin_" << name << spar << argsAMI << "::Ice::noExplicitContext" << "__del" << "__cookie" << epar << ';';
    H << eb;

    H << sp << nl << "::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
      << "const ::Ice::Context& __ctx"
      << "const " + delNameScoped + "Ptr& __del"
      << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar;
    H << sb;
    H << nl << "return __begin_" << name << spar << argsAMI << "__ctx" << "__del" << "__cookie" << epar << ';';
    H << eb;

    H << sp << nl << retS << " end_" << name << spar << outParamsDeclAMI
      << "const ::Ice::AsyncResultPtr&" << epar << ';';
    if(generatePrivateEnd)
    {
        H << sp << nl << " void ___end_" << name << spar << outParamsDeclEndAMI;
        H << "const ::Ice::AsyncResultPtr&" << epar << ';';
    }

    H << nl;
    H.dec();
    H << nl << "private:";
    H.inc();
    H << nl <<  "::Ice::AsyncResultPtr __begin_" << name << spar << paramsAMI << "const ::Ice::Context&"
      << "const ::IceInternal::CallbackBasePtr&"
      << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar << ';';
    H << nl;
    H.dec();
    H << nl << "public:";
    H.inc();

    C << sp << nl << retS << nl << "IceProxy" << scoped << spar << paramsDecl << "const ::Ice::Context& __ctx" << epar;
    C << sb;
    if(p->returnsData())
    {
        C << nl << "__checkTwowayOnly(" << flatName << ");";
    }
    C << nl << "::IceInternal::Outgoing __og(this, " << flatName << ", " << operationModeToString(p->sendMode())
      << ", __ctx);";
    if(inParams.empty())
    {
        C << nl << "__og.writeEmptyParams();";
    }
    else
    {
        C << nl << "try";
        C << sb;
        C << nl<< "::Ice::OutputStream* __os = __og.startWriteParams(" << opFormatTypeToString(p) << ");";
        writeMarshalCode(C, inParams, 0, true, TypeContextInParam);
        if(p->sendsClasses(false))
        {
            C << nl << "__os->writePendingObjects();";
        }
        C << nl << "__og.endWriteParams();";
        C << eb;
        C << nl << "catch(const ::Ice::LocalException& __ex)";
        C << sb;
        C << nl << "__og.abort(__ex);";
        C << eb;
    }

    if(!p->returnsData())
    {
        C << nl << "__invoke(__og);"; // Use helpers for methods that don't return data.
    }
    else
    {
        C << nl << "if(!__og.invoke())";
        C << sb;
        C << nl << "try";
        C << sb;
        C << nl << "__og.throwUserException();";
        C << eb;

        //
        // Generate a catch block for each legal user exception. This is necessary
        // to prevent an "impossible" user exception to be thrown if client and
        // and server use different exception specifications for an operation. For
        // example:
        //
        // Client compiled with:
        // exception A {};
        // exception B {};
        // interface I {
        //     void op() throws A;
        // };
        //
        // Server compiled with:
        // exception A {};
        // exception B {};
        // interface I {
        //     void op() throws B; // Differs from client
        // };
        //
        // We need the catch blocks so, if the server throws B from op(), the
        // client receives UnknownUserException instead of B.
        //
        ExceptionList throws = p->throws();
        throws.sort();
        throws.unique();
#if defined(__SUNPRO_CC)
        throws.sort(derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif
        for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
        {
            C << nl << "catch(const " << fixKwd((*i)->scoped()) << "&)";
            C << sb;
            C << nl << "throw;";
            C << eb;
        }
        C << nl << "catch(const ::Ice::UserException& __ex)";
        C << sb;
        //
        // COMPILERFIX: Don't throw UnknownUserException directly. This is causing access
        // violation errors with Visual C++ 64bits optimized builds. See bug #2962.
        //
        C << nl << "::Ice::UnknownUserException __uue(__FILE__, __LINE__, __ex.ice_id());";
        C << nl << "throw __uue;";
        C << eb;
        C << eb;

        if(ret || !outParams.empty())
        {
            writeAllocateCode(C, ParamDeclList(), p, true, _useWstring);
            C << nl << "::Ice::InputStream* __is = __og.startReadParams();";
            writeUnmarshalCode(C, outParams, p, true);
            if(p->returnsClasses(false))
            {
                C << nl << "__is->readPendingObjects();";
            }
            C << nl << "__og.endReadParams();";
        }

        if(ret)
        {
            C << nl << "return __ret;";
        }
    }
    C << eb;

    C << sp << nl << "::Ice::AsyncResultPtr" << nl << "IceProxy" << scope << "__begin_" << name << spar << paramsDeclAMI
      << "const ::Ice::Context& __ctx" << "const ::IceInternal::CallbackBasePtr& __del"
      << "const ::Ice::LocalObjectPtr& __cookie" << epar;
    C << sb;
    if(p->returnsData())
    {
        C << nl << "__checkAsyncTwowayOnly(" << flatName <<  ");";
    }
    C << nl << "::IceInternal::OutgoingAsyncPtr __result = new ::IceInternal::CallbackOutgoing(this, " << flatName
        << ", __del, __cookie);";
    C << nl << "try";
    C << sb;
    C << nl << "__result->prepare(" << flatName << ", " << operationModeToString(p->sendMode()) << ", __ctx);";
    if(inParams.empty())
    {
        C << nl << "__result->writeEmptyParams();";
    }
    else
    {
        C << nl << "::Ice::OutputStream* __os = __result->startWriteParams(" << opFormatTypeToString(p) <<");";
        writeMarshalCode(C, inParams, 0, true, TypeContextInParam);
        if(p->sendsClasses(false))
        {
            C << nl << "__os->writePendingObjects();";
        }
        C << nl << "__result->endWriteParams();";
    }
    C << nl << "__result->invoke(" << flatName << ");";
    C << eb;
    C << nl << "catch(const ::Ice::Exception& __ex)";
    C << sb;
    C << nl << "__result->abort(__ex);";
    C << eb;
    C << nl << "return __result;";
    C << eb;

    C << sp << nl << retS << nl << "IceProxy" << scope << "end_" << name << spar << outParamsDeclAMI
      << "const ::Ice::AsyncResultPtr& __result" << epar;
    C << sb;
    if(p->returnsData())
    {
        C << nl << "::Ice::AsyncResult::__check(__result, this, " << flatName << ");";

        //
        // COMPILERFIX: It's necessary to generate the allocate code here before
        // this if(!__result->wait()). If generated after this if block, we get
        // access violations errors with the test/Ice/slicing/objects test on VC9
        // and Windows 64 bits when compiled with optimization (see bug 4400).
        //
        writeAllocateCode(C, ParamDeclList(), p, true, _useWstring | TypeContextAMIEnd);
        C << nl << "if(!__result->__wait())";
        C << sb;
        C << nl << "try";
        C << sb;
        C << nl << "__result->__throwUserException();";
        C << eb;
        //
        // Generate a catch block for each legal user exception.
        //
        ExceptionList throws = p->throws();
        throws.sort();
        throws.unique();
#if defined(__SUNPRO_CC)
        throws.sort(derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif
        for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
        {
            string scoped = (*i)->scoped();
            C << nl << "catch(const " << fixKwd((*i)->scoped()) << "&)";
            C << sb;
            C << nl << "throw;";
            C << eb;
        }
        C << nl << "catch(const ::Ice::UserException& __ex)";
        C << sb;
        C << nl << "throw ::Ice::UnknownUserException(__FILE__, __LINE__, __ex.ice_id());";
        C << eb;
        C << eb;
        if(ret || !outParams.empty())
        {
            C << nl << "::Ice::InputStream* __is = __result->__startReadParams();";
            writeUnmarshalCode(C, outParams, p, true, _useWstring | TypeContextAMIEnd);
            if(p->returnsClasses(false))
            {
                C << nl << "__is->readPendingObjects();";
            }
            C << nl << "__result->__endReadParams();";
        }
        else
        {
            C << nl << "__result->__readEmptyParams();";
        }
        if(ret)
        {
            C << nl << "return __ret;";
        }
    }
    else
    {
        C << nl << "__end(__result, " << flatName << ");";
    }
    C << eb;

    if(generatePrivateEnd)
    {
        assert(p->returnsData());

        C << sp << nl << "void IceProxy" << scope << "___end_" << name << spar << outParamsDeclEndAMI
          << "const ::Ice::AsyncResultPtr& __result" << epar;
        C << sb;
        C << nl << "::Ice::AsyncResult::__check(__result, this, " << flatName << ");";
        C << nl << "if(!__result->__wait())";
        C << sb;
        C << nl << "try";
        C << sb;
        C << nl << "__result->__throwUserException();";
        C << eb;
        //
        // Generate a catch block for each legal user exception.
        //
        ExceptionList throws = p->throws();
        throws.sort();
        throws.unique();
#if defined(__SUNPRO_CC)
        throws.sort(derivedToBaseCompare);
#else
        throws.sort(Slice::DerivedToBaseCompare());
#endif
        for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
        {
            string scoped = (*i)->scoped();
            C << nl << "catch(const " << fixKwd((*i)->scoped()) << "&)";
            C << sb;
            C << nl << "throw;";
            C << eb;
        }
        C << nl << "catch(const ::Ice::UserException& __ex)";
        C << sb;
        C << nl << "throw ::Ice::UnknownUserException(__FILE__, __LINE__, __ex.ice_id());";
        C << eb;
        C << eb;

        if(ret || !outParams.empty())
        {
            C << nl << "::Ice::InputStream* __is = __result->__startReadParams();";
            writeUnmarshalCode(C, outParams, p, true, _useWstring | TypeContextAMIPrivateEnd);
            if(p->returnsClasses(false))
            {
                C << nl << "__is->readPendingObjects();";
            }
            C << nl << "__result->__endReadParams();";
        }
        else
        {
            C << nl << "__result->__readEmptyParams();";
        }
        C << eb;
    }
}

Slice::Gen::ObjectDeclVisitor::ObjectDeclVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::ObjectDeclVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDecls())
    {
        return false;
    }

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';
    C << sp << nl << "namespace" << nl << "{";
    return true;
}

void
Slice::Gen::ObjectDeclVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';
    C << sp << nl << "}";
}

void
Slice::Gen::ObjectDeclVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());

    H << sp << nl << "class " << name << ';';
    H << nl << "bool operator==(const " << name << "&, const " << name << "&);";
    H << nl << "bool operator<(const " << name << "&, const " << name << "&);";
    if(!p->isLocal())
    {
        H << nl << _dllExport << "::Ice::Object* upCast(" << scoped << "*);";
        H << nl << "typedef ::IceInternal::Handle< " << scoped << "> " << p->name() << "Ptr;";
        H << nl << "typedef ::IceInternal::ProxyHandle< ::IceProxy" << scoped << "> " << p->name() << "Prx;";
        H << nl << "typedef " << p->name() << "Prx " << p->name() << "PrxPtr;";
        H << nl << _dllExport << "void __patch(" << p->name() << "Ptr&, const ::Ice::ObjectPtr&);";
    }
    else
    {
        H << nl << _dllExport << "::Ice::LocalObject* upCast(" << scoped << "*);";
        H << nl << "typedef ::IceInternal::Handle< " << scoped << "> " << p->name() << "Ptr;";
    }
}

void
Slice::Gen::ObjectDeclVisitor::visitOperation(const OperationPtr& p)
{
    ClassDefPtr cl = ClassDefPtr::dynamicCast(p->container());
    if(cl && !cl->isLocal())
    {
        string flatName = p->flattenedScope() + p->name() + "_name";
        C << sp << nl << "const ::std::string " << flatName << " = \"" << p->name() << "\";";
    }
}

Slice::Gen::ObjectVisitor::ObjectVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _doneStaticSymbol(false), _useWstring(false)
{
}

bool
Slice::Gen::ObjectVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ObjectVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::ObjectVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();
    bool basePreserved = p->inheritsMetaData("preserve-slice");
    bool preserved = basePreserved || p->hasMetaData("preserve-slice");

    H << sp << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    if(bases.empty())
    {
        if(p->isLocal())
        {
            H << "public virtual ::Ice::LocalObject";
        }
        else
        {
            H << "public virtual ::Ice::Object";
        }
    }
    else
    {
        ClassList::const_iterator q = bases.begin();
        bool virtualInheritance = p->hasMetaData("cpp:virtual") || p->isInterface();
        while(q != bases.end())
        {
            if(virtualInheritance || (*q)->isInterface())
            {
                H << "virtual ";
            }

            H << "public " << fixKwd((*q)->scoped());
            if(++q != bases.end())
            {
                H << ',' << nl;
            }
        }
    }

    bool hasBaseClass = !bases.empty() && !bases.front()->isInterface();
    bool override = p->canBeCyclic() && (!hasBaseClass || !bases.front()->canBeCyclic());
    bool hasGCObjectBaseClass = basePreserved || override || preserved;
    if(!basePreserved && (override || preserved))
    {
        H << ", public ::IceInternal::GCObject";
    }

    H.restoreIndent();
    H << sb;
    H.dec();
    H << nl << "public:" << sp;
    H.inc();

    //
    // In C++, a nested type cannot have the same name as the enclosing type
    //
    if(!p->isLocal() && p->name() != "ProxyType")
    {
        H << nl << "typedef " << p->name() << "Prx ProxyType;";
    }

    if(p->name() != "PointerType")
    {
        H << nl << "typedef " << p->name() << "Ptr PointerType;";
    }

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
        allTypes.push_back(typeName);
        allParamDecls.push_back(typeName + " __ice_" + (*q)->name());
    }

    if(!p->isInterface())
    {
        if(p->hasDefaultValues())
        {
            H << sp << nl << name << "() :";
            H.inc();
            writeDataMemberInitializers(H, dataMembers, _useWstring);
            H.dec();
            H << sb;
            H << eb;
        }
        else
        {
            H << sp << nl << name << "()";
            H << sb << eb;
        }

        /*
         * Strong guarantee: commented-out code marked "Strong guarantee" generates
         * a copy-assignment operator that provides the strong exception guarantee.
         * For now, this is commented out, and we use the compiler-generated
         * copy-assignment operator. However, that one does not provide the strong
         * guarantee.

        H << ';';
        if(!p->isAbstract())
        {
            H << nl << name << "& operator=(const " << name << "&)";
            if(allDataMembers.empty())
            {
                H << " { return *this; }";
            }
            H << ';';
        }

        //
        // __swap() is static because classes may be abstract, so we
        // can't use a non-static member function when we do an upcall
        // from a non-abstract derived __swap to the __swap in an abstract base.
        //
        H << sp << nl << "static void __swap(" << name << "&, " << name << "&) throw()";
        if(allDataMembers.empty())
        {
            H << " {}";
        }
        H << ';';
        H << nl << "void swap(" << name << "& rhs) throw()";
        H << sb;
        if(!allDataMembers.empty())
        {
            H << nl << "__swap(*this, rhs);";
        }
        H << eb;

         * Strong guarantee
         */

        emitOneShotConstructor(p);
        H << sp;

        /*
         * Strong guarantee

        if(!allDataMembers.empty())
        {
            C << sp << nl << "void";
            C << nl << scoped.substr(2) << "::__swap(" << name << "& __lhs, " << name << "& __rhs) throw()";
            C << sb;

            if(base)
            {
                emitUpcall(base, "::__swap(__lhs, __rhs);");
            }

            //
            // We use a map to remember for which types we have already declared
            // a temporary variable and reuse that variable if a class has
            // more than one member of the same type. That way, we don't use more
            // temporaries than necessary. (::std::swap() instantiates a new temporary
            // each time it is used.)
            //
            map<string, int> tmpMap;
            map<string, int>::iterator pos;
            int tmpCount = 0;

            for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
            {
                string memberName = fixKwd((*q)->name());
                TypePtr type = (*q)->type();
                BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
                if(builtin && builtin->kind() != Builtin::KindString
                   || EnumPtr::dynamicCast(type) || ProxyPtr::dynamicCast(type)
                   || ClassDeclPtr::dynamicCast(type) || StructPtr::dynamicCast(type))
                {
                    //
                    // For built-in types (except string), enums, proxies, structs, and classes,
                    // do the swap via a temporary variable.
                    //
                    string typeName = typeToString(type);
                    pos = tmpMap.find(typeName);
                    if(pos == tmpMap.end())
                    {
                        pos = tmpMap.insert(pos, make_pair(typeName, tmpCount));
                        C << nl << typeName << " __tmp" << tmpCount << ';';
                        tmpCount++;
                    }
                    C << nl << "__tmp" << pos->second << " = __rhs." << memberName << ';';
                    C << nl << "__rhs." << memberName << " = __lhs." << memberName << ';';
                    C << nl << "__lhs." << memberName << " = __tmp" << pos->second << ';';
                }
                else
                {
                    //
                    // For dictionaries, vectors, and maps, use the standard container's
                    // swap() (which is usually optimized).
                    //
                    C << nl << "__lhs." << memberName << ".swap(__rhs." << memberName << ");";
                }
            }
            C << eb;

            if(!p->isAbstract())
            {
                C << sp << nl << scoped << "&";
                C << nl << scoped.substr(2) << "::operator=(const " << name << "& __rhs)";
                C << sb;
                C << nl << name << " __tmp(__rhs);";
                C << nl << "__swap(*this, __tmp);";
                C << nl << "return *this;";
                C << eb;
            }
        }

         * Strong guarantee
         */
    }

    if(!p->isLocal())
    {
        C << sp << nl
          << _dllExport
          << "::Ice::Object* " << scope.substr(2) << "upCast(" << scoped << "* p) { return p; }";

        //
        // It would make sense to provide a covariant ice_clone(); unfortunately many compilers
        // (including VS2010) generate bad code for covariant types that use virtual inheritance
        //

        if(!p->isInterface())
        {
            H << nl << "virtual ::Ice::ObjectPtr ice_clone() const;";

             if(hasGCObjectBaseClass)
            {
                C.zeroIndent();
                C << sp;
                C << nl << "#if defined(_MSC_VER) && (_MSC_VER >= 1900)";
                C << nl << "#   pragma warning(push)";
                C << nl << "#   pragma warning(disable:4589)";
                C << nl << "#endif";
                C.restoreIndent();
            }
            C << nl << "::Ice::ObjectPtr";
            C << nl << scoped.substr(2) << "::ice_clone() const";
            C << sb;
            if(!p->isAbstract())
            {
                C << nl << "::Ice::Object* __p = new " << name << "(*this);";
                C << nl << "return __p;";
            }
            else
            {
                //
                // We need this ice_clone for abstract classes derived from concrete classes
                //
                C << nl << "throw ::Ice::CloneNotImplementedException(__FILE__, __LINE__);";
            }
            C << eb;
            if(hasGCObjectBaseClass)
            {
                C.zeroIndent();
                C << nl << "#if defined(_MSC_VER) && (_MSC_VER >= 1900)";
                C << nl << "#   pragma warning(pop)";
                C << nl << "#endif";
                C.restoreIndent();
            }
        }

        ClassList allBases = p->allBases();
        StringList ids;
#if defined(__IBMCPP__) && defined(NDEBUG)
//
// VisualAge C++ 6.0 does not see that ClassDef is a Contained,
// when inlining is on. The code below issues a warning: better
// than an error!
//
        transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun<string,ClassDef>(&Contained::scoped));
#else
        transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun(&Contained::scoped));
#endif
        StringList other;
        other.push_back(p->scoped());
        other.push_back("::Ice::Object");
        other.sort();
        ids.merge(other);
        ids.unique();
        StringList::const_iterator firstIter = ids.begin();
        StringList::const_iterator scopedIter = find(ids.begin(), ids.end(), p->scoped());
        assert(scopedIter != ids.end());
        StringList::difference_type scopedPos = IceUtilInternal::distance(firstIter, scopedIter);

        H << sp;
        H << nl << "virtual bool ice_isA"
          << "(const ::std::string&, const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
        H << nl << "virtual ::std::vector< ::std::string> ice_ids"
          << "(const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
        H << nl << "virtual const ::std::string& ice_id(const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
        H << nl << "static const ::std::string& ice_staticId();";

        if(!dataMembers.empty())
        {
            H << sp;
        }

        string flatName = p->flattenedScope() + p->name() + "_ids";

        C << sp << nl << "namespace";
        C << nl << "{";
        C << nl << "const ::std::string " << flatName << '[' << ids.size() << "] =";
        C << sb;

        for(StringList::const_iterator r = ids.begin(); r != ids.end();)
        {
            C << nl << '"' << *r << '"';
            if(++r != ids.end())
            {
                C << ',';
            }
        }
        C << eb << ';';
        C << sp << nl << "}";

        C << sp;
        C << nl << "bool" << nl << scoped.substr(2)
          << "::ice_isA(const ::std::string& _s, const ::Ice::Current&) const";
        C << sb;
        C << nl << "return ::std::binary_search(" << flatName << ", " << flatName << " + " << ids.size() << ", _s);";
        C << eb;

        C << sp;
        C << nl << "::std::vector< ::std::string>" << nl << scoped.substr(2)
          << "::ice_ids(const ::Ice::Current&) const";
        C << sb;
        C << nl << "return ::std::vector< ::std::string>(&" << flatName << "[0], &" << flatName
          << '[' << ids.size() << "]);";
        C << eb;

        C << sp;
        C << nl << "const ::std::string&" << nl << scoped.substr(2)
          << "::ice_id(const ::Ice::Current&) const";
        C << sb;
        C << nl << "return " << flatName << '[' << scopedPos << "];";
        C << eb;

        C << sp;
        C << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_staticId()";
        C << sb;
        C.zeroIndent();
        C << nl << "#ifdef ICE_HAS_THREAD_SAFE_LOCAL_STATIC";
        C.restoreIndent();
        C << nl << "static const ::std::string typeId = \"" << *scopedIter << "\";";
        C << nl << "return typeId;";
        C.zeroIndent();
        C << nl << "#else";
        C.restoreIndent();
        C << nl << "return " << flatName << '[' << scopedPos << "];";
        C.zeroIndent();
        C << nl << "#endif";
        C.restoreIndent();
        C << eb;

        emitGCFunctions(p);
    }
    else
    {
        C << sp << nl
          << _dllExport
          << "::Ice::LocalObject* " << scope.substr(2) << "upCast(" << scoped << "* p) { return p; }";
    }

    return true;
}

void
Slice::Gen::ObjectVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    bool basePreserved = p->inheritsMetaData("preserve-slice");
    bool preserved = p->hasMetaData("preserve-slice");

    if(!p->isLocal())
    {
        OperationList allOps = p->allOperations();
        if(!allOps.empty())
        {
            StringList allOpNames;
#if defined(__IBMCPP__) && defined(NDEBUG)
//
// See comment for transform above
//
            transform(allOps.begin(), allOps.end(), back_inserter(allOpNames),
                      ::IceUtil::constMemFun<string,Operation>(&Contained::name));
#else
            transform(allOps.begin(), allOps.end(), back_inserter(allOpNames),
                      ::IceUtil::constMemFun(&Contained::name));
#endif
            allOpNames.push_back("ice_id");
            allOpNames.push_back("ice_ids");
            allOpNames.push_back("ice_isA");
            allOpNames.push_back("ice_ping");
            allOpNames.sort();
            allOpNames.unique();

            H << sp;
            H << nl
              << "virtual ::Ice::DispatchStatus __dispatch(::IceInternal::Incoming&, const ::Ice::Current&);";

            string flatName = p->flattenedScope() + p->name() + "_all";
            C << sp << nl << "namespace";
            C << nl << "{";
            C << nl << "const ::std::string " << flatName << "[] =";
            C << sb;

            for(StringList::const_iterator q = allOpNames.begin(); q != allOpNames.end();)
            {
                C << nl << '"' << *q << '"';
                if(++q != allOpNames.end())
                {
                    C << ',';
                }
            }
            C << eb << ';';
            C << sp << nl << "}";
            C << sp;
            C << nl << "::Ice::DispatchStatus" << nl << scoped.substr(2)
              << "::__dispatch(::IceInternal::Incoming& in, const ::Ice::Current& current)";
            C << sb;

            C << nl << "::std::pair< const ::std::string*, const ::std::string*> r = "
              << "::std::equal_range(" << flatName << ", " << flatName << " + " << allOpNames.size()
              << ", current.operation);";
            C << nl << "if(r.first == r.second)";
            C << sb;
            C << nl << "throw ::Ice::OperationNotExistException(__FILE__, __LINE__, current.id, "
              << "current.facet, current.operation);";
            C << eb;
            C << sp;
            C << nl << "switch(r.first - " << flatName << ')';
            C << sb;
            int i = 0;
            for(StringList::const_iterator q = allOpNames.begin(); q != allOpNames.end(); ++q)
            {
                C << nl << "case " << i++ << ':';
                C << sb;
                C << nl << "return ___" << *q << "(in, current);";
                C << eb;
            }
            C << eb;
            C << sp;
            C << nl << "assert(false);";
            C << nl << "throw ::Ice::OperationNotExistException(__FILE__, __LINE__, current.id, "
              << "current.facet, current.operation);";
            C << eb;

            //
            // Check if we need to generate ice_operationAttributes()
            //
            map<string, int> attributesMap;
            for(OperationList::iterator r = allOps.begin(); r != allOps.end(); ++r)
            {
                int attributes = (*r)->attributes();
                if(attributes != 0)
                {
                    attributesMap.insert(map<string, int>::value_type((*r)->name(), attributes));
                }
            }

            if(!attributesMap.empty())
            {
                H << sp;
                H << nl
                  << "virtual ::Ice::Int ice_operationAttributes(const ::std::string&) const;";

                string opAttrFlatName = p->flattenedScope() + p->name() + "_operationAttributes";

                C << sp << nl << "namespace";
                C << nl << "{";
                C << nl << "const int " << opAttrFlatName << "[] = ";
                C << sb;

                for(StringList::const_iterator q = allOpNames.begin(); q != allOpNames.end();)
                {
                    int attributes = 0;
                    string opName = *q;
                    map<string, int>::iterator it = attributesMap.find(opName);
                    if(it != attributesMap.end())
                    {
                        attributes = it->second;
                    }
                    C << nl << attributes;

                    if(++q != allOpNames.end())
                    {
                        C << ',';
                    }
                    C << " // " << opName;
                }

                C << eb << ';';
                C << sp << nl << "}";

                C << sp;

                C << nl << "::Ice::Int" << nl << scoped.substr(2)
                  << "::ice_operationAttributes(const ::std::string& opName) const";
                C << sb;

                C << nl << "::std::pair< const ::std::string*, const ::std::string*> r = "
                  << "::std::equal_range(" << flatName << ", " << flatName << " + " << allOpNames.size()
                  << ", opName);";
                C << nl << "if(r.first == r.second)";
                C << sb;
                C << nl << "return -1;";
                C << eb;

                C << nl << "return " << opAttrFlatName << "[r.first - " << flatName << "];";
                C << eb;
            }
        }

        if(!p->isAbstract())
        {
            H << sp << nl << "static ::Ice::ValueFactoryPtr ice_factory();";
        }

        if(preserved && !basePreserved)
        {
            H << sp;
            H << nl << "virtual void __write(::Ice::OutputStream*) const;";
            H << nl << "virtual void __read(::Ice::InputStream*);";

            string baseName = base ? fixKwd(base->scoped()) : string("::Ice::Object");
            H << nl << "using " << baseName << "::__write;";
            H << nl << "using " << baseName << "::__read;";
        }

        H.dec();
        H << sp << nl << "protected:";
        H.inc();

        H << nl << "virtual void __writeImpl(::Ice::OutputStream*) const;";
        H << nl << "virtual void __readImpl(::Ice::InputStream*);";

        string baseName = base ? fixKwd(base->scoped()) : string("::Ice::Object");
        H << nl << "using " << baseName << "::__writeImpl;";
        H << nl << "using " << baseName << "::__readImpl;";

        if(preserved && !basePreserved)
        {
            C << sp;
            C << nl << "void" << nl << scoped.substr(2) << "::__write(::Ice::OutputStream* __os) const";
            C << sb;
            C << nl << "__os->startObject(__slicedData);";
            C << nl << "__writeImpl(__os);";
            C << nl << "__os->endObject();";
            C << eb;

            C << sp;
            C << nl << "void" << nl << scoped.substr(2) << "::__read(::Ice::InputStream* __is)";
            C << sb;
            C << nl << "__is->startObject();";
            C << nl << "__readImpl(__is);";
            C << nl << "__slicedData = __is->endObject(true);";
            C << eb;
        }

        C << sp;
        C << nl << "void" << nl << scoped.substr(2) << "::__writeImpl(::Ice::OutputStream* __os) const";
        C << sb;
        C << nl << "__os->startSlice(ice_staticId(), " << p->compactId() << (!base ? ", true" : ", false") << ");";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), true);
        C << nl << "__os->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__writeImpl(__os);");
        }
        C << eb;

        C << sp;
        C << nl << "void" << nl << scoped.substr(2) << "::__readImpl(::Ice::InputStream* __is)";
        C << sb;
        C << nl << "__is->startSlice();";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), false);
        C << nl << "__is->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__readImpl(__is);");
        }
        C << eb;

        if(!p->isAbstract() || p->compactId() >= 0)
        {
            C << sp << nl << "namespace";
            C << nl << "{";

            if(!p->isAbstract())
            {
                string initName = p->flattenedScope() + p->name() + "_init";
                C << nl << "const ::IceInternal::DefaultValueFactoryInit< " << scoped << "> "
                  << initName << "(\"" << p->scoped() << "\");";
            }
            if(p->compactId() >= 0)
            {
                string initName = p->flattenedScope() + p->name() + "_compactIdInit";
                C << nl << "const ::IceInternal::CompactIdInit "
                  << initName << "(\"" << p->scoped() << "\", " << p->compactId() << ");";
            }
            C << nl << "}";

            if(!p->isAbstract())
            {
                C << sp << nl << "::Ice::ValueFactoryPtr" << nl << scoped.substr(2) << "::ice_factory()";
                C << sb;
                C << nl << "return ::IceInternal::factoryTable->getValueFactory(" << scoped << "::ice_staticId());";
                C << eb;
            }
        }
    }

    //
    // Emit data members. Access visibility may be specified by metadata.
    //
    bool inProtected = true;
    DataMemberList dataMembers = p->dataMembers();
    bool prot = p->hasMetaData("protected");
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        if(prot || (*q)->hasMetaData("protected"))
        {
            if(!inProtected)
            {
                H.dec();
                H << sp << nl << "protected:";
                H.inc();
                inProtected = true;
            }
        }
        else
        {
            if(inProtected)
            {
                H.dec();
                H << sp << nl << "public:";
                H.inc();
                inProtected = false;
            }
        }

        emitDataMember(*q);
    }

    if(!p->isAbstract())
    {
        //
        // We add a protected destructor to force heap instantiation of the class.
        //
        if(!inProtected)
        {
            H.dec();
            H << nl << "protected:";
            H.inc();
            inProtected = true;
        }
        H << sp << nl << "virtual ~" << fixKwd(p->name()) << "() {}";
    }

    if(!p->isLocal() && preserved && !basePreserved)
    {
        if(!inProtected)
        {
            H.dec();
            H << sp << nl << "protected:";
            H.inc();
            inProtected = true;
        }
        H << sp << nl << "::Ice::SlicedDataPtr __slicedData;";
    }

    H << eb << ';';

    if(!p->isAbstract() && !p->isLocal() && !_doneStaticSymbol)
    {
        //
        // We need an instance here to trigger initialization if the implementation is in a static library.
        // But we do this only once per source file, because a single instance is sufficient to initialize
        // all of the globals in a compilation unit.
        //
        H << nl << "static ::Ice::ValueFactoryPtr _" << p->name() << "_init = " << p->scoped() << "::ice_factory();";
    }

    if(p->isLocal())
    {
        H << sp;
        H << nl << "inline bool operator==(const " << fixKwd(p->name()) << "& l, const " << fixKwd(p->name()) << "& r)";
        H << sb;
        H << nl << "return static_cast<const ::Ice::LocalObject&>(l) == static_cast<const ::Ice::LocalObject&>(r);";
        H << eb;
        H << sp;
        H << nl << "inline bool operator<(const " << fixKwd(p->name()) << "& l, const " << fixKwd(p->name()) << "& r)";
        H << sb;
        H << nl << "return static_cast<const ::Ice::LocalObject&>(l) < static_cast<const ::Ice::LocalObject&>(r);";
        H << eb;
    }
    else
    {
        C << sp << nl << "void " << _dllExport;
        C << nl << scope.substr(2) << "__patch(" << p->name() << "Ptr& handle, const ::Ice::ObjectPtr& v)";
        C << sb;
        C << nl << "handle = " << scope << p->name() << "Ptr::dynamicCast(v);";
        C << nl << "if(v && !handle)";
        C << sb;
        C << nl << "IceInternal::Ex::throwUOE(" << scoped << "::ice_staticId(), v);";
        C << eb;
        C << eb;

        H << sp;
        H << nl << "inline bool operator==(const " << fixKwd(p->name()) << "& l, const " << fixKwd(p->name()) << "& r)";
        H << sb;
        H << nl << "return static_cast<const ::Ice::Object&>(l) == static_cast<const ::Ice::Object&>(r);";
        H << eb;
        H << sp;
        H << nl << "inline bool operator<(const " << fixKwd(p->name()) << "& l, const " << fixKwd(p->name()) << "& r)";
        H << sb;
        H << nl << "return static_cast<const ::Ice::Object&>(l) < static_cast<const ::Ice::Object&>(r);";
        H << eb;
    }

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::ObjectVisitor::visitExceptionStart(const ExceptionPtr&)
{
    return false;
}

bool
Slice::Gen::ObjectVisitor::visitStructStart(const StructPtr&)
{
    return false;
}

void
Slice::Gen::ObjectVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring);

    string params = "(";
    string paramsDecl = "(";
    string args = "(";

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    string classNameAMD = "AMD_" + cl->name();
    string classScope = fixKwd(cl->scope());
    string classScopedAMD = classScope + classNameAMD;

    string paramsAMD = "(const " + classScopedAMD + '_' + name + "Ptr&, ";
    string paramsDeclAMD = "(const " + classScopedAMD + '_' + name + "Ptr& __cb, ";
    string argsAMD = "(__cb, ";

    ParamDeclList inParams;
    ParamDeclList outParams;
    ParamDeclList paramList = p->parameters();
    vector< string> outDecls;
    for(ParamDeclList::iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd(string(paramPrefix) + (*q)->name());
        TypePtr type = (*q)->type();
        bool isOutParam = (*q)->isOutParam();
        string typeString;
        if(isOutParam)
        {
            outParams.push_back(*q);
            typeString = outputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), _useWstring);
        }
        else
        {
            inParams.push_back(*q);
            typeString = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
        }

        if(q != paramList.begin())
        {
            params += ", ";
            paramsDecl += ", ";
            args += ", ";
        }

        params += typeString;
        paramsDecl += typeString;
        paramsDecl += ' ';
        paramsDecl += paramName;
        args += paramName;

        if(!isOutParam)
        {
            paramsAMD += typeString;
            paramsAMD += ", ";
            paramsDeclAMD += typeString;
            paramsDeclAMD += ' ';
            paramsDeclAMD += paramName;
            paramsDeclAMD += ", ";
            argsAMD += paramName;
            argsAMD += ", ";
        }
        else
        {
            outDecls.push_back(inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring));
        }
    }

    if(!cl->isLocal())
    {
        if(!paramList.empty())
        {
            params += ", ";
            paramsDecl += ", ";
            args += ", ";
        }

        params += "const ::Ice::Current& = ::Ice::noExplicitCurrent)";
        paramsDecl += "const ::Ice::Current& __current)";
        args += "__current)";
    }
    else
    {
        params += ')';
        paramsDecl += ')';
        args += ')';
    }

    paramsAMD += "const ::Ice::Current& = ::Ice::noExplicitCurrent)";
    paramsDeclAMD += "const ::Ice::Current& __current)";
    argsAMD += "__current)";

    string isConst = ((p->mode() == Operation::Nonmutating) || p->hasMetaData("cpp:const")) ? " const" : "";
    bool amd = !cl->isLocal() && (cl->hasMetaData("amd") || p->hasMetaData("amd"));

    string deprecateSymbol = getDeprecateSymbol(p, cl);

    H << sp;
    if(!amd)
    {
        H << nl << deprecateSymbol << "virtual " << retS << ' ' << fixKwd(name) << params << isConst << " = 0;";
    }
    else
    {
        H << nl << deprecateSymbol << "virtual void " << name << "_async" << paramsAMD << isConst << " = 0;";
    }

    if(!cl->isLocal())
    {
        H << nl << "::Ice::DispatchStatus ___" << name
          << "(::IceInternal::Incoming&, const ::Ice::Current&)" << isConst << ';';

        C << sp;
        C << nl << "::Ice::DispatchStatus" << nl << scope.substr(2) << "___" << name
          << "(::IceInternal::Incoming& __inS" << ", const ::Ice::Current& __current)" << isConst;
        C << sb;
        if(!amd)
        {
            ExceptionList throws = p->throws();
            throws.sort();
            throws.unique();

            //
            // Arrange exceptions into most-derived to least-derived order. If we don't
            // do this, a base exception handler can appear before a derived exception
            // handler, causing compiler warnings and resulting in the base exception
            // being marshaled instead of the derived exception.
            //

#if defined(__SUNPRO_CC)
            throws.sort(derivedToBaseCompare);
#else
            throws.sort(Slice::DerivedToBaseCompare());
#endif

            C << nl << "__checkMode(" << operationModeToString(p->mode()) << ", __current.mode);";

            if(!inParams.empty())
            {
                C << nl << "::Ice::InputStream* __is = __inS.startReadParams();";
                writeAllocateCode(C, inParams, 0, true, _useWstring | TypeContextInParam);
                writeUnmarshalCode(C, inParams, 0, true, TypeContextInParam);
                if(p->sendsClasses(false))
                {
                    C << nl << "__is->readPendingObjects();";
                }
                C << nl << "__inS.endReadParams();";
            }
            else
            {
                C << nl << "__inS.readEmptyParams();";
            }

            writeAllocateCode(C, outParams, 0, true, _useWstring);
            if(!throws.empty())
            {
                C << nl << "try";
                C << sb;
            }
            C << nl;
            if(ret)
            {
                C << retS << " __ret = ";
            }
            C << fixKwd(name) << args << ';';
            if(ret || !outParams.empty())
            {
                C << nl << "::Ice::OutputStream* __os = __inS.__startWriteParams("
                  << opFormatTypeToString(p) << ");";
                writeMarshalCode(C, outParams, p, true);
                if(p->returnsClasses(false))
                {
                    C << nl << "__os->writePendingObjects();";
                }
                C << nl << "__inS.__endWriteParams(true);";
            }
            else
            {
                C << nl << "__inS.__writeEmptyParams();";
            }
            C << nl << "return ::Ice::DispatchOK;";
            if(!throws.empty())
            {
                C << eb;
                ExceptionList::const_iterator r;
                for(r = throws.begin(); r != throws.end(); ++r)
                {
                    C << nl << "catch(const " << fixKwd((*r)->scoped()) << "& __ex)";
                    C << sb;
                    C << nl << "__inS.__writeUserException(__ex, " << opFormatTypeToString(p) << ");";
                    C << eb;
                }
                C << nl << "return ::Ice::DispatchUserException;";
            }
        }
        else
        {
            C << nl << "__checkMode(" << operationModeToString(p->mode()) << ", __current.mode);";

            if(!inParams.empty())
            {
                C << nl << "::Ice::InputStream* __is = __inS.startReadParams();";
                writeAllocateCode(C, inParams, 0, true, _useWstring | TypeContextInParam);
                writeUnmarshalCode(C, inParams, 0, true, TypeContextInParam);
                if(p->sendsClasses(false))
                {
                    C << nl << "__is->readPendingObjects();";
                }
                C << nl << "__inS.endReadParams();";
            }
            else
            {
                C << nl << "__inS.readEmptyParams();";
            }

            C << nl << classScopedAMD << '_' << name << "Ptr __cb = new IceAsync" << classScopedAMD << '_' << name
              << "(__inS);";
            C << nl << "try";
            C << sb;
            C << nl << name << "_async" << argsAMD << ';';
            C << eb;
            C << nl << "catch(const ::std::exception& __ex)";
            C << sb;
            C << nl << "__cb->ice_exception(__ex);";
            C << eb;
            C << nl << "catch(...)";
            C << sb;
            C << nl << "__cb->ice_exception();";
            C << eb;
            C << nl << "return ::Ice::DispatchAsync;";
        }
        C << eb;
    }

    if(cl->isLocal() && (cl->hasMetaData("async-oneway") || p->hasMetaData("async-oneway")))
    {
        vector<string> paramsDeclAMI;
        vector<string> outParamsDeclAMI;

        ParamDeclList paramList = p->parameters();
        for(ParamDeclList::const_iterator r = paramList.begin(); r != paramList.end(); ++r)
        {
            string paramName = fixKwd((*r)->name());

            StringList metaData = (*r)->getMetaData();
            string typeString;
            if((*r)->isOutParam())
            {
                typeString = outputTypeToString((*r)->type(), (*r)->optional(), metaData,
                                                _useWstring | TypeContextAMIEnd);
            }
            else
            {
                typeString = inputTypeToString((*r)->type(), (*r)->optional(), metaData, _useWstring);
            }

            if(!(*r)->isOutParam())
            {
                paramsDeclAMI.push_back(typeString + ' ' + paramName);
            }
            else
            {
                outParamsDeclAMI.push_back(typeString + ' ' + paramName);
            }
        }

        H << sp << nl << "virtual ::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI << epar << " = 0;";

        H << sp << nl << "virtual ::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
          << "const ::Ice::CallbackPtr& __del"
          << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar << " = 0;";

        string clScope = fixKwd(cl->scope());
        string delName = "Callback_" + cl->name() + "_" + name;
        string delNameScoped = clScope + delName;

        H << sp << nl << "virtual ::Ice::AsyncResultPtr begin_" << name << spar << paramsDeclAMI
          << "const " + delNameScoped + "Ptr& __del"
          << "const ::Ice::LocalObjectPtr& __cookie = 0" << epar << " = 0;";

        H << sp << nl << "virtual " << retS << " end_" << name << spar << outParamsDeclAMI
          << "const ::Ice::AsyncResultPtr&" << epar << " = 0;";
    }
}

void
Slice::Gen::ObjectVisitor::emitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    H << sp << nl << typeToString(p->type(), p->optional(), p->getMetaData(), _useWstring) << ' ' << name << ';';
}

void
Slice::Gen::ObjectVisitor::emitGCFunctions(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    DataMemberList dataMembers = p->dataMembers();

    //
    // A class can potentially be part of a cycle if it (recursively) contains class
    // members.
    //
    bool canBeCyclic = p->canBeCyclic();
    bool basePreserved = p->inheritsMetaData("preserve-slice");
    bool preserved = basePreserved || p->hasMetaData("preserve-slice");

    //
    // __gcVisit() is overridden by the basemost class that can be
    // cyclic, plus all classes derived from that class.
    //
    // We also override these methods for the initial preserved class in a
    // hierarchy, regardless of whether the class itself is cyclic.
    //
    if(canBeCyclic || (preserved && !basePreserved))
    {
        H << nl << "virtual void __gcVisitMembers(::IceInternal::GCVisitor&);";

        C << sp << nl << "void" << nl << scoped.substr(2) << "::__gcVisitMembers(::IceInternal::GCVisitor& _v)";
        C << sb;

        bool hasCyclicBase = base && base->canBeCyclic();
        if(hasCyclicBase || basePreserved)
        {
            emitUpcall(bases.front(), "::__gcVisitMembers(_v);");
        }

        if(preserved && !basePreserved)
        {
            C << nl << "if(__slicedData)";
            C << sb;
            C << nl << "__slicedData->__gcVisitMembers(_v);";
            C << eb;
        }

        for(DataMemberList::const_iterator i = dataMembers.begin(); i != dataMembers.end(); ++i)
        {
            if((*i)->type()->usesClasses())
            {
                if((*i)->optional())
                {
                    C << nl << "if(" << fixKwd((*i)->name()) << ')';
                    C << sb;
                    emitGCVisitCode((*i)->type(), getDataMemberRef(*i), "", 0);
                    C << eb;
                }
                else
                {
                    emitGCVisitCode((*i)->type(), getDataMemberRef(*i), "", 0);
                }
            }
        }
        C << eb;
    }
}

void
Slice::Gen::ObjectVisitor::emitGCVisitCode(const TypePtr& p, const string& prefix, const string& name, int level)
{
    BuiltinPtr builtin = BuiltinPtr::dynamicCast(p);
    if((builtin &&
       (BuiltinPtr::dynamicCast(p)->kind() == Builtin::KindObject || BuiltinPtr::dynamicCast(p)->kind() == Builtin::KindValue)) ||
       ClassDeclPtr::dynamicCast(p))
    {
        C << nl << "if(" << prefix << name << ')';
        C << sb;
        ClassDeclPtr decl = ClassDeclPtr::dynamicCast(p);
        if(decl)
        {
            string scope = fixKwd(decl->scope());
            C << nl << "if((" << scope << "upCast(" << prefix << name << ".get())->__gcVisit(_v)))";
        }
        else
        {
            C << nl << "if((" << prefix << name << ".get())->__gcVisit(_v))";
        }
        C << sb;
        C << nl << prefix << name << " = 0;";
        C << eb;
        C << eb;
    }
    else if(StructPtr::dynamicCast(p))
    {
        StructPtr s = StructPtr::dynamicCast(p);
        DataMemberList dml = s->dataMembers();
        for(DataMemberList::const_iterator i = dml.begin(); i != dml.end(); ++i)
        {
            if((*i)->type()->usesClasses())
            {
                emitGCVisitCode((*i)->type(), prefix + name + ".", fixKwd((*i)->name()), ++level);
            }
        }
    }
    else if(DictionaryPtr::dynamicCast(p))
    {
        DictionaryPtr d = DictionaryPtr::dynamicCast(p);
        string scoped = fixKwd(d->scoped());
        ostringstream tmp;
        tmp << "_i" << level;
        string iterName = tmp.str();
        C << sb;
        C << nl << "for(" << scoped << "::iterator " << iterName << " = " << prefix + name
          << ".begin(); " << iterName << " != " << prefix + name << ".end(); ++" << iterName << ")";
        C << sb;
        emitGCVisitCode(d->valueType(), "", string("(*") + iterName + ").second", ++level);
        C << eb;
        C << eb;
    }
    else if(SequencePtr::dynamicCast(p))
    {
        SequencePtr s = SequencePtr::dynamicCast(p);
        string scoped = fixKwd(s->scoped());
        ostringstream tmp;
        tmp << "_i" << level;
        string iterName = tmp.str();
        C << sb;
        C << nl << "for(" << scoped << "::iterator " << iterName << " = " << prefix + name
          << ".begin(); " << iterName << " != " << prefix + name << ".end(); ++" << iterName << ")";
        C << sb;
        emitGCVisitCode(s->type(), string("(*") + iterName + ")", "", ++level);
        C << eb;
        C << eb;
    }
}

bool
Slice::Gen::ObjectVisitor::emitVirtualBaseInitializers(const ClassDefPtr& p, bool virtualInheritance, bool direct)
{
    DataMemberList allDataMembers = p->allDataMembers();
    if(allDataMembers.empty())
    {
        return false;
    }

    ClassList bases = p->bases();
    if(!bases.empty() && !bases.front()->isInterface())
    {
        if(emitVirtualBaseInitializers(bases.front(), p->hasMetaData("cpp:virtual"), false))
        {
            H << ',';
        }
    }

    //
    // Do not call non direct base classes constructor if not using virtual inheritance.
    //
    if(!direct && !virtualInheritance)
    {
        return false;
    }

    string upcall = "(";
    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        if(q != allDataMembers.begin())
        {
            upcall += ", ";
        }
        upcall += "__ice_" + (*q)->name();
    }
    upcall += ")";

    H << nl << fixKwd(p->scoped()) << upcall;

    return true;
}

void
Slice::Gen::ObjectVisitor::emitOneShotConstructor(const ClassDefPtr& p)
{
    DataMemberList allDataMembers = p->allDataMembers();

    if(!allDataMembers.empty())
    {
        vector<string> allParamDecls;

        bool virtualInheritance = p->hasMetaData("cpp:virtual");
        bool callBaseConstuctors = !(p->isAbstract() && virtualInheritance);
        DataMemberList dataMembers = p->dataMembers();

        for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
        {

            string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
            bool dataMember = std::find(dataMembers.begin(), dataMembers.end(), (*q)) != dataMembers.end();
            allParamDecls.push_back(typeName + ((dataMember || callBaseConstuctors) ?
                                                    (" __ice_" + (*q)->name()) : (" /*__ice_" + (*q)->name() + "*/")));
        }

        H << sp << nl;
        if(allParamDecls.size() == 1)
        {
            H << "explicit ";
        }
        H << fixKwd(p->name()) << spar << allParamDecls << epar;
        if(callBaseConstuctors || !dataMembers.empty())
        {
            H << " :";
        }
        H.inc();

        ClassList bases = p->bases();
        ClassDefPtr base;

        if(!bases.empty() && !bases.front()->isInterface() && callBaseConstuctors)
        {
            if(emitVirtualBaseInitializers(bases.front(), virtualInheritance, true))
            {
                if(!dataMembers.empty())
                {
                    H << ',';
                }
            }
        }

        if(!dataMembers.empty())
        {
            H << nl;
        }
        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            if(q != dataMembers.begin())
            {
                H << ',' << nl;
            }
            string memberName = fixKwd((*q)->name());
            H << memberName << '(' << "__ice_" << (*q)->name() << ')';
        }

        H.dec();
        H << sb;
        H << eb;
    }
}

void
Slice::Gen::ObjectVisitor::emitUpcall(const ClassDefPtr& base, const string& call)
{
    C << nl << (base ? fixKwd(base->scoped()) : string("::Ice::Object")) << call;
}

Slice::Gen::AsyncCallbackVisitor::AsyncCallbackVisitor(Output& h, Output&, const string& dllExport) :
    H(h), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::AsyncCallbackVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDefs() && !p->hasContentsWithMetaData("async-oneway"))
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    H << sp << nl << "namespace " << fixKwd(p->name())  << nl << '{';

    return true;
}

void
Slice::Gen::AsyncCallbackVisitor::visitModuleEnd(const ModulePtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);

    H << sp << nl << '}';
}

bool
Slice::Gen::AsyncCallbackVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    return true;
}

void
Slice::Gen::AsyncCallbackVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::AsyncCallbackVisitor::visitOperation(const OperationPtr& p)
{
    ClassDefPtr cl = ClassDefPtr::dynamicCast(p->container());

    if(cl->isLocal() && !(cl->hasMetaData("async-oneway") || p->hasMetaData("async-oneway")))
    {
        return;
    }

    //
    // Write the callback base class and callback smart pointer.
    //
    string delName = "Callback_" + cl->name() + "_" + p->name();
    H << sp << nl << "class " << delName << "_Base : public virtual ::IceInternal::CallbackBase { };";
    H << nl << "typedef ::IceUtil::Handle< " << delName << "_Base> " << delName << "Ptr;";
}

Slice::Gen::AsyncCallbackTemplateVisitor::AsyncCallbackTemplateVisitor(Output& h, Output&, const string& dllExport)
    : H(h), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::AsyncCallbackTemplateVisitor::visitUnitStart(const UnitPtr& p)
{
    return p->hasNonLocalClassDefs();
}

void
Slice::Gen::AsyncCallbackTemplateVisitor::visitUnitEnd(const UnitPtr&)
{
}

bool
Slice::Gen::AsyncCallbackTemplateVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    H << sp << nl << "namespace " << fixKwd(p->name())  << nl << '{';
    return true;
}

void
Slice::Gen::AsyncCallbackTemplateVisitor::visitModuleEnd(const ModulePtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);
    H << sp << nl << '}';
}

bool
Slice::Gen::AsyncCallbackTemplateVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    return true;
}

void
Slice::Gen::AsyncCallbackTemplateVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::AsyncCallbackTemplateVisitor::visitOperation(const OperationPtr& p)
{
    generateOperation(p, false);
    generateOperation(p, true);
}

void
Slice::Gen::AsyncCallbackTemplateVisitor::generateOperation(const OperationPtr& p, bool withCookie)
{
    ClassDefPtr cl = ClassDefPtr::dynamicCast(p->container());
    if(cl->isLocal() || cl->operations().empty())
    {
        return;
    }

    string clName = cl->name();
    string clScope = fixKwd(cl->scope());
    string delName = "Callback_" + clName + "_" + p->name();
    string delTmplName = (withCookie ? "Callback_" : "CallbackNC_") + clName + "_" + p->name();

    TypePtr ret = p->returnType();
    string retS = inputTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring);
    string retEndArg = getEndArg(ret, p->getMetaData(), "__ret");

    ParamDeclList outParams;
    vector<string> outArgs;
    vector<string> outDecls;
    vector<string> outDeclsEnd;
    vector<string> outEndArgs;

    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if((*q)->isOutParam())
        {
            outParams.push_back(*q);
            outArgs.push_back(fixKwd((*q)->name()));
            outEndArgs.push_back(getEndArg((*q)->type(), (*q)->getMetaData(), outArgs.back()));
            outDecls.push_back(inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring));
        }
    }

    string baseD;
    string inheritD;
    if(withCookie)
    {
        baseD = "::IceInternal::Callback<T, CT>";
        H << sp << nl << "template<class T, typename CT>";
        inheritD = p->returnsData() ? "::IceInternal::TwowayCallback<T, CT>" : "::IceInternal::OnewayCallback<T, CT>";
    }
    else
    {
        baseD = "::IceInternal::CallbackNC<T>";
        H << sp << nl << "template<class T>";
        inheritD = p->returnsData() ? "::IceInternal::TwowayCallbackNC<T>" : "::IceInternal::OnewayCallbackNC<T>";
    }

    H << nl << "class " << delTmplName << " : public " << delName << "_Base, public " << inheritD;
    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    H << sp << nl << "typedef IceUtil::Handle<T> TPtr;";

    string cookieT;
    string comCookieT;
    if(withCookie)
    {
        cookieT = "const CT&";
        comCookieT = " , const CT&";
    }

    H << sp << nl << "typedef void (T::*Exception)(const ::Ice::Exception&" << comCookieT << ");";
    H << nl << "typedef void (T::*Sent)(bool" << comCookieT << ");";
    if(p->returnsData())
    {
        //
        // typedefs for callbacks.
        //
        H << nl << "typedef void (T::*Response)" << spar;
        if(ret)
        {
            H << retS;
        }
        H << outDecls;
        if(withCookie)
        {
            H << cookieT;
        }
        H << epar << ';';
    }
    else
    {
        H << nl << "typedef void (T::*Response)(" << cookieT << ");";
    }

    //
    // constructor.
    //
    H << sp;
    H << nl << delTmplName << spar << "const TPtr& obj";
    H << "Response cb";
    H << "Exception excb";
    H << "Sent sentcb" << epar;
    H.inc();
    if(p->returnsData())
    {
        H << nl << ": " << inheritD + "(obj, cb != 0, excb, sentcb), _response(cb)";
    }
    else
    {
        H << nl << ": " << inheritD + "(obj, cb, excb, sentcb)";
    }
    H.dec();
    H << sb;
    H << eb;

    if(p->returnsData())
    {
        //
        // completed.
        //
        H << sp << nl << "virtual void completed(const ::Ice::AsyncResultPtr& __result) const";
        H << sb;
        H << nl << clScope << clName << "Prx __proxy = " << clScope << clName
          << "Prx::uncheckedCast(__result->getProxy());";
        writeAllocateCode(H, outParams, p, false, _useWstring | TypeContextInParam | TypeContextAMICallPrivateEnd);
        H << nl << "try";
        H << sb;
        H << nl;
        if(!usePrivateEnd(p))
        {
            if(ret)
            {
                H << retEndArg << " = ";
            }
            H << "__proxy->end_" << p->name() << spar << outEndArgs << "__result" << epar << ';';
        }
        else
        {
            H << "__proxy->___end_" << p->name() << spar << outEndArgs;
            if(ret)
            {
                H << retEndArg;
            }
            H << "__result" << epar << ';';
        }
        writeEndCode(H, outParams, p);
        H << eb;
        H << nl << "catch(const ::Ice::Exception& ex)";
        H << sb;

        H << nl << "" << baseD << "::exception(__result, ex);";
        H << nl << "return;";
        H << eb;
        H << nl << "if(_response)";
        H << sb;
        H << nl << "(" << baseD << "::_callback.get()->*_response)" << spar;
        if(ret)
        {
            H << "__ret";
        }
        H << outArgs;
        if(withCookie)
        {
            H << "CT::dynamicCast(__result->getCookie())";
        }
        H << epar << ';';
        H << eb;
        H << eb;
        H << sp << nl << "private:";
        H << sp << nl << "Response _response;";
    }
    H << eb << ';';

    // Factory method
    for(int i = 0; i < 2; i++)
    {
        string callbackT = i == 0 ? "const IceUtil::Handle<T>&" : "T*";

        if(withCookie)
        {
            cookieT = "const CT&";
            comCookieT = ", const CT&";
            H << sp << nl << "template<class T, typename CT> " << delName << "Ptr";
        }
        else
        {
            H << sp << nl << "template<class T> " << delName << "Ptr";
        }

        H << nl << "new" << delName << "(" << callbackT << " instance, ";
        if(p->returnsData())
        {
            H  << "void (T::*cb)" << spar;
            if(ret)
            {
                H << retS;
            }
            H << outDecls;
            if(withCookie)
            {
                H << cookieT;
            }
            H << epar << ", ";
        }
        else
        {
            H  << "void (T::*cb)(" << cookieT << "), ";
        }
        H << "void (T::*excb)(" << "const ::Ice::Exception&" << comCookieT << "), ";
        H << "void (T::*sentcb)(bool" << comCookieT << ") = 0)";
        H << sb;
        if(withCookie)
        {
            H << nl << "return new " << delTmplName << "<T, CT>(instance, cb, excb, sentcb);";
        }
        else
        {
            H << nl << "return new " << delTmplName << "<T>(instance, cb, excb, sentcb);";
        }
        H << eb;

        if(!ret && outParams.empty())
        {
            if(withCookie)
            {
                H << sp << nl << "template<class T, typename CT> " << delName << "Ptr";
            }
            else
            {
                H << sp << nl << "template<class T> " << delName << "Ptr";
            }
            H << nl << "new" << delName << "(" << callbackT << " instance, ";
            H << "void (T::*excb)(" << "const ::Ice::Exception&" << comCookieT << "), ";
            H << "void (T::*sentcb)(bool" << comCookieT << ") = 0)";
            H << sb;
            if(withCookie)
            {
                H << nl << "return new " << delTmplName << "<T, CT>(instance, 0, excb, sentcb);";
            }
            else
            {
                H << nl << "return new " << delTmplName << "<T>(instance, 0, excb, sentcb);";
            }
            H << eb;
        }
    }
}

Slice::Gen::ImplVisitor::ImplVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _useWstring(false)
{
}

string
Slice::Gen::ImplVisitor::defaultValue(const TypePtr& type, const StringList& metaData) const
{
    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(builtin)
    {
        switch(builtin->kind())
        {
            case Builtin::KindBool:
            {
                return "false";
            }
            case Builtin::KindByte:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            {
                return "0";
            }
            case Builtin::KindFloat:
            case Builtin::KindDouble:
            {
                return "0.0";
            }
            case Builtin::KindString:
            {
                return "::std::string()";
            }
            case Builtin::KindValue:
            case Builtin::KindObject:
            case Builtin::KindObjectProxy:
            case Builtin::KindLocalObject:
            {
                return "0";
            }
        }
    }
    else
    {
        ProxyPtr prx = ProxyPtr::dynamicCast(type);

        if(ProxyPtr::dynamicCast(type) || ClassDeclPtr::dynamicCast(type))
        {
            return "0";
        }

        StructPtr st = StructPtr::dynamicCast(type);
        if(st)
        {
           return fixKwd(st->scoped()) + "()";
        }

        EnumPtr en = EnumPtr::dynamicCast(type);
        if(en)
        {
            EnumeratorList enumerators = en->getEnumerators();
            return fixKwd(en->scope() + enumerators.front()->name());
        }

        SequencePtr seq = SequencePtr::dynamicCast(type);
        if(seq)
        {
            return typeToString(seq, metaData, _useWstring, true) + "()";
        }

        DictionaryPtr dict = DictionaryPtr::dynamicCast(type);
        if(dict)
        {
            return fixKwd(dict->scoped()) + "()";
        }
    }

    assert(false);
    return "???";
}

void
Slice::Gen::ImplVisitor::writeReturn(Output& out, const TypePtr& type, const StringList& metaData)
{
    out << nl << "return " << defaultValue(type, metaData) << ";";
}

bool
Slice::Gen::ImplVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    H << sp << nl << "namespace " << fixKwd(p->name()) << nl << '{';
    return true;
}

void
Slice::Gen::ImplVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::ImplVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = p->name();
    string scope = fixKwd(p->scope());
    string cls = scope.substr(2) + name + "I";
    ClassList bases = p->bases();

    H << sp;
    H << nl << "class " << name << "I : public virtual " << fixKwd(name);

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    OperationList ops = p->allOperations();

    for(OperationList::const_iterator r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = (*r);
        string opName = op->name();
        string isConst = ((op->mode() == Operation::Nonmutating) || op->hasMetaData("cpp:const")) ? " const" : "";

        string classScopedAMD = scope + "AMD_" + ClassDefPtr::dynamicCast(op->container())->name();

        TypePtr ret = op->returnType();
        string retS = returnTypeToString(ret, op->returnIsOptional(), op->getMetaData(), _useWstring);

        if(!p->isLocal() && (p->hasMetaData("amd") || op->hasMetaData("amd")))
        {
            H << sp << nl << "virtual void " << opName << "_async(";
            H.useCurrentPosAsIndent();
            H << "const " << classScopedAMD << '_' << opName << "Ptr&";
            ParamDeclList paramList = op->parameters();

            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(!(*q)->isOutParam())
                {
                    H << ',' << nl << inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(),
                                                        _useWstring);
                }
            }
            H << ',' << nl << "const Ice::Current&";
            H.restoreIndent();
            H << ")" << isConst << ';';

            C << sp << nl << "void" << nl << scope << name << "I::" << opName << "_async(";
            C.useCurrentPosAsIndent();
            C << "const " << classScopedAMD << '_' << opName << "Ptr& " << opName << "CB";
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(!(*q)->isOutParam())
                {
                    C << ',' << nl << inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(),
                                                        _useWstring) << ' ' << fixKwd((*q)->name());
                }
            }
            C << ',' << nl << "const Ice::Current& current";
            C.restoreIndent();
            C << ")" << isConst;
            C << sb;

            string result = "r";
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if((*q)->name() == result)
                {
                    result = "_" + result;
                    break;
                }
            }

            C << nl << opName << "CB->ice_response(";
            if(ret)
            {
                C << defaultValue(ret, op->getMetaData());
            }
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if((*q)->isOutParam())
                {
                    if(ret || q != paramList.begin())
                    {
                        C << ", ";
                    }
                    C << defaultValue((*q)->type(), op->getMetaData());
                }
            }
            C << ");";

            C << eb;
        }
        else
        {
            H << sp << nl << "virtual " << retS << ' ' << fixKwd(opName) << '(';
            H.useCurrentPosAsIndent();
            ParamDeclList paramList = op->parameters();
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(q != paramList.begin())
                {
                    H << ',' << nl;
                }
                if((*q)->isOutParam())
                {
                    H << outputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
                }
                else
                {
                    H << inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
                }
            }
            if(!p->isLocal())
            {
                if(!paramList.empty())
                {
                    H << ',' << nl;
                }
                H << "const Ice::Current&";
            }
            H.restoreIndent();

            string isConst = ((op->mode() == Operation::Nonmutating) || op->hasMetaData("cpp:const")) ? " const" : "";

            H << ")" << isConst << ';';

            C << sp << nl << retS << nl;
            C << scope.substr(2) << name << "I::" << fixKwd(opName) << '(';
            C.useCurrentPosAsIndent();
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(q != paramList.begin())
                {
                    C << ',' << nl;
                }
                if((*q)->isOutParam())
                {
                    C << outputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring) << " "
                      << fixKwd((*q)->name());
                }
                else
                {
                    C << inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring) << " /*"
                      << fixKwd((*q)->name()) << "*/";
                }
            }
            if(!p->isLocal())
            {
                if(!paramList.empty())
                {
                    C << ',' << nl;
                }
                C << "const Ice::Current& current";
            }
            C.restoreIndent();
            C << ')';
            C << isConst;
            C << sb;

            if(ret)
            {
                writeReturn(C, ret, op->getMetaData());
            }

            C << eb;
        }
    }

    H << eb << ';';

    _useWstring = resetUseWstring(_useWstringHist);

    return true;
}

Slice::Gen::AsyncVisitor::AsyncVisitor(Output& h, Output&, const string& dllExport) :
    H(h), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::AsyncVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls() || !p->hasContentsWithMetaData("amd"))
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::AsyncVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::AsyncVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    return true;
}

void
Slice::Gen::AsyncVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::AsyncVisitor::visitOperation(const OperationPtr& p)
{
    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);

    if(cl->isLocal() || (!cl->hasMetaData("amd") && !p->hasMetaData("amd")))
    {
        return;
    }

    string name = p->name();

    string className = cl->name();
    string classNameAMD = "AMD_" + className;
    string classScope = fixKwd(cl->scope());
    string classScopedAMD = classScope + classNameAMD;
    string proxyName = classScope + className + "Prx";

    vector<string> params;
    vector<string> paramsAMD;
    vector<string> paramsDecl;
    vector<string> args;

    vector<string> paramsInvoke;
    vector<string> paramsDeclInvoke;

    paramsInvoke.push_back("const " + proxyName + "&");
    paramsDeclInvoke.push_back("const " + proxyName + "& __prx");

    TypePtr ret = p->returnType();
    string retS = inputTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring);

    if(ret)
    {
        params.push_back(retS);
        paramsAMD.push_back(inputTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring));
        paramsDecl.push_back(retS + " __ret");
        args.push_back("__ret");
    }

    ParamDeclList inParams;
    ParamDeclList outParams;
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd((*q)->name());
        TypePtr type = (*q)->type();
        string typeString = inputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), _useWstring);

        if((*q)->isOutParam())
        {
            params.push_back(typeString);
            paramsAMD.push_back(inputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), _useWstring));
            paramsDecl.push_back(typeString + ' ' + paramName);
            args.push_back(paramName);

            outParams.push_back(*q);
        }
        else
        {
            paramsInvoke.push_back(typeString);
            paramsDeclInvoke.push_back(typeString + ' ' + paramName);

            inParams.push_back(*q);
        }
    }

    paramsInvoke.push_back("const ::Ice::Context&");
    paramsDeclInvoke.push_back("const ::Ice::Context& __ctx");

    if(cl->hasMetaData("amd") || p->hasMetaData("amd"))
    {
        H << sp << nl << "class " << _dllExport << classNameAMD << '_' << name
          << " : public virtual ::Ice::AMDCallback";
        H << sb;
        H.dec();
        H << nl << "public:";
        H.inc();
        H << sp;
        H << nl << "virtual void ice_response" << spar << paramsAMD << epar << " = 0;";
        H << eb << ';';
        H << sp << nl << "typedef ::IceUtil::Handle< " << classScopedAMD << '_' << name << "> "
          << classNameAMD << '_' << name  << "Ptr;";
    }
}

Slice::Gen::AsyncImplVisitor::AsyncImplVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::AsyncImplVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDecls() || !p->hasContentsWithMetaData("amd"))
    {
        return false;
    }

    H << sp << nl << "namespace IceAsync" << nl << '{';

    return true;
}

void
Slice::Gen::AsyncImplVisitor::visitUnitEnd(const UnitPtr&)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::AsyncImplVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls() || !p->hasContentsWithMetaData("amd"))
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::AsyncImplVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::AsyncImplVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    return true;
}

void
Slice::Gen::AsyncImplVisitor::visitClassDefEnd(const ClassDefPtr&)
{
    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::AsyncImplVisitor::visitOperation(const OperationPtr& p)
{
    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);

    if(cl->isLocal() || (!cl->hasMetaData("amd") && !p->hasMetaData("amd")))
    {
        return;
    }

    string name = p->name();

    string classNameAMD = "AMD_" + cl->name();
    string classScope = fixKwd(cl->scope());
    string classScopedAMD = classScope + classNameAMD;

    string params;
    string paramsDecl;
    string args;

    ExceptionList throws = p->throws();
    throws.sort();
    throws.unique();

    //
    // Arrange exceptions into most-derived to least-derived order. If we don't
    // do this, a base exception handler can appear before a derived exception
    // handler, causing compiler warnings and resulting in the base exception
    // being marshaled instead of the derived exception.
    //
#if defined(__SUNPRO_CC)
    throws.sort(derivedToBaseCompare);
#else
    throws.sort(Slice::DerivedToBaseCompare());
#endif

    TypePtr ret = p->returnType();
    string retS = inputTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring);

    if(ret)
    {
        params += retS;
        paramsDecl += retS;
        paramsDecl += ' ';
        paramsDecl += "__ret";
        args += "__ret";
    }

    ParamDeclList outParams;
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        if((*q)->isOutParam())
        {
            string paramName = fixKwd((*q)->name());
            TypePtr type = (*q)->type();
            string typeString = inputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), _useWstring);

            if(ret || !outParams.empty())
            {
                params += ", ";
                paramsDecl += ", ";
                args += ", ";
            }

            params += typeString;
            paramsDecl += typeString;
            paramsDecl += ' ';
            paramsDecl += paramName;
            args += paramName;

            outParams.push_back(*q);
        }
    }

    H << sp << nl << "class " << _dllExport << classNameAMD << '_' << name
      << " : public " << classScopedAMD  << '_' << name << ", public ::IceInternal::IncomingAsync";
    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    H << sp;
    H << nl << classNameAMD << '_' << name << "(::IceInternal::Incoming&);";

    H << sp;
    H << nl << "virtual void ice_response(" << params << ");";
    if(!throws.empty())
    {
        H << nl << "// COMPILERFIX: The using directive avoid compiler warnings with -Woverloaded-virtual";
        H << nl << "using ::IceInternal::IncomingAsync::ice_exception;";
        H << nl << "virtual void ice_exception(const ::std::exception&);";
    }
    H << eb << ';';

    C << sp << nl << "IceAsync" << classScopedAMD << '_' << name << "::" << classNameAMD << '_' << name
      << "(::IceInternal::Incoming& in) :";
    C.inc();
    C << nl << "::IceInternal::IncomingAsync(in)";
    C.dec();
    C << sb;
    C << eb;

    C << sp << nl << "void" << nl << "IceAsync" << classScopedAMD << '_' << name << "::ice_response("
      << paramsDecl << ')';
    C << sb;
    C << nl << "if(__validateResponse(true))";
    C << sb;
    if(ret || !outParams.empty())
    {
        C << nl << "try";
        C << sb;
        C << nl << "::Ice::OutputStream* __os = __startWriteParams(" << opFormatTypeToString(p) << ");";
        writeMarshalCode(C, outParams, p, false, TypeContextInParam);
        if(p->returnsClasses(false))
        {
            C << nl << "__os->writePendingObjects();";
        }
        C << nl << "__endWriteParams(true);";
        C << eb;
        C << nl << "catch(const ::Ice::Exception& __ex)";
        C << sb;
        C << nl << "__exception(__ex);";
        C << nl << "return;";
        C << eb;
    }
    else
    {
        C << nl << "__writeEmptyParams();";
    }
    C << nl << "__response();";
    C << eb;
    C << eb;

    if(!throws.empty())
    {
        C << sp << nl << "void" << nl << "IceAsync" << classScopedAMD << '_' << name
            << "::ice_exception(const ::std::exception& ex)";
        C << sb;
        for(ExceptionList::const_iterator r = throws.begin(); r != throws.end(); ++r)
        {
            C << nl;
            if(r != throws.begin())
            {
                C << "else ";
            }
            C << "if(const " << fixKwd((*r)->scoped()) << "* __ex = dynamic_cast<const " << fixKwd((*r)->scoped())
            << "*>(&ex))";
            C << sb;
            C << nl <<"if(__validateResponse(false))";
            C << sb;
            C << nl << "__writeUserException(*__ex, " << opFormatTypeToString(p) << ");";
            C << nl << "__response();";
            C << eb;
            C << eb;
        }
        C << nl << "else";
        C << sb;
        C << nl << "::IceInternal::IncomingAsync::ice_exception(ex);";
        C << eb;
        C << eb;
    }
}

Slice::Gen::StreamVisitor::StreamVisitor(Output& h, Output& c, const string& dllExport) :
    H(h),
    C(c),
    _dllExport(dllExport)
{
}

bool
Slice::Gen::StreamVisitor::visitModuleStart(const ModulePtr& m)
{
    if(!m->hasNonLocalContained(Contained::ContainedTypeStruct) &&
       !m->hasNonLocalContained(Contained::ContainedTypeEnum) &&
       !m->hasNonLocalContained(Contained::ContainedTypeException))
    {
        return false;
    }

    if(UnitPtr::dynamicCast(m->container()))
    {
        //
        // Only emit this for the top-level module.
        //
        H << sp;
        H << nl << "namespace Ice" << nl << '{';

        C << sp;
        C << nl << "namespace Ice" << nl << '{';
    }

    return true;
}

void
Slice::Gen::StreamVisitor::visitModuleEnd(const ModulePtr& m)
{
    if(UnitPtr::dynamicCast(m->container()))
    {
        //
        // Only emit this for the top-level module.
        //
        H << nl << '}';
        C << nl << '}';
    }
}

bool
Slice::Gen::StreamVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    if(!p->isLocal())
    {
        string scoped = p->scoped();
        H << nl << "template<>";
        H << nl << "struct StreamableTraits< " << fixKwd(scoped) << ">";
        H << sb;
        H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryUserException;";
        H << eb << ";" << nl;
    }
    return false;
}

bool
Slice::Gen::StreamVisitor::visitStructStart(const StructPtr& p)
{
    if(!p->isLocal())
    {
        bool classMetaData = findMetaData(p->getMetaData(), false) == "%class";
        string scoped = p->scoped();

        string fullStructName = classMetaData ? fixKwd(scoped + "Ptr") : fixKwd(scoped);

        H << nl << "template<>";

        H << nl << "struct StreamableTraits< " << fullStructName << ">";

        H << sb;
        if(classMetaData)
        {
            H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryStructClass;";
        }
        else
        {
            H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryStruct;";
        }
        H << nl << "static const int minWireSize = " << p->minWireSize() << ";";
        if(p->isVariableLength())
        {
            H << nl << "static const bool fixedLength = false;";
        }
        else
        {
            H << nl << "static const bool fixedLength = true;";
        }
        H << eb << ";" << nl;

        DataMemberList dataMembers = p->dataMembers();

        string holder = classMetaData ? "v->" : "v.";

        H << nl << "template<class S>";
        H << nl << "struct StreamWriter< " << fullStructName << ", S>";
        H << sb;
        H << nl << "static void write(S* __os, const " <<  fullStructName << "& v)";
        H << sb;
        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            writeMarshalUnmarshalDataMemberInHolder(H, holder, *q, true);
        }
        H << eb;
        H << eb << ";" << nl;

        H << nl << "template<class S>";
        H << nl << "struct StreamReader< " << fullStructName << ", S>";
        H << sb;
        H << nl << "static void read(S* __is, " << fullStructName << "& v)";
        H << sb;
        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            writeMarshalUnmarshalDataMemberInHolder(H, holder, *q, false);
        }
        H << eb;
        H << eb << ";" << nl;

        if(!_dllExport.empty())
        {
            //
            // We tell "importers" that the implementation exports these instantiations
            //
            H << nl << "#if defined(ICE_HAS_DECLSPEC_IMPORT_EXPORT) && !defined(";
            H << _dllExport.substr(0, _dllExport.size() - 1) + "_EXPORTS) && !defined(ICE_STATIC_LIBS)";
            H << nl << "template struct " << _dllExport << "StreamWriter< " << fullStructName
              << ", ::Ice::OutputStream>;";
            H << nl << "template struct " << _dllExport << "StreamReader< " << fullStructName
              << ", ::Ice::InputStream>;";
            H << nl << "#endif" << nl;

            //
            // The instantations:
            //
            C << nl << "#if defined(ICE_HAS_DECLSPEC_IMPORT_EXPORT) && !defined(ICE_STATIC_LIBS)";
            C << nl << "template struct " << _dllExport << "StreamWriter< " << fullStructName
              << ", ::Ice::OutputStream>;";
            C << nl << "template struct " << _dllExport << "StreamReader< " << fullStructName
              << ", ::Ice::InputStream>;";
            C << nl << "#endif";
        }
    }
    return false;
}

void
Slice::Gen::StreamVisitor::visitEnum(const EnumPtr& p)
{
    if(!p->isLocal())
    {
        string scoped = fixKwd(p->scoped());
        H << nl << "template<>";
        H << nl << "struct StreamableTraits< " << scoped << ">";
        H << sb;
        H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryEnum;";
        H << nl << "static const int minValue = " << p->minValue() << ";";
        H << nl << "static const int maxValue = " << p->maxValue() << ";";
        H << nl << "static const int minWireSize = " << p->minWireSize() << ";";
        H << nl << "static const bool fixedLength = false;";
        H << eb << ";" << nl;
    }
}

void
Slice::Gen::validateMetaData(const UnitPtr& u)
{
    MetaDataVisitor visitor;
    u->visit(&visitor, false);
}

bool
Slice::Gen::MetaDataVisitor::visitUnitStart(const UnitPtr& p)
{
    static const string prefix = "cpp:";

    //
    // Validate global metadata in the top-level file and all included files.
    //
    StringList files = p->allFiles();

    for(StringList::iterator q = files.begin(); q != files.end(); ++q)
    {
        string file = *q;
        DefinitionContextPtr dc = p->findDefinitionContext(file);
        assert(dc);
        StringList globalMetaData = dc->getMetaData();
        int headerExtension = 0;
        for(StringList::const_iterator r = globalMetaData.begin(); r != globalMetaData.end(); ++r)
        {
            string s = *r;
            if(_history.count(s) == 0)
            {
                if(s.find(prefix) == 0)
                {
                    static const string cppIncludePrefix = "cpp:include:";
                    static const string cppHeaderExtPrefix = "cpp:header-ext:";
                    if(s.find(cppIncludePrefix) == 0 && s.size() > cppIncludePrefix.size())
                    {
                        continue;
                    }
                    else if(s.find(cppHeaderExtPrefix) == 0 && s.size() > cppHeaderExtPrefix.size())
                    {
                        headerExtension++;
                        if(headerExtension > 1)
                        {
                            ostringstream ostr;
                            ostr << "ignoring invalid global metadata `" << s
                                << "': directive can appear only once per file";
                            emitWarning(file, -1, ostr.str());
                            _history.insert(s);
                        }
                        continue;
                    }
                    ostringstream ostr;
                    ostr << "ignoring invalid global metadata `" << s << "'";
                    emitWarning(file, -1, ostr.str());
                }
                _history.insert(s);
            }
        }
    }

    return true;
}

bool
Slice::Gen::MetaDataVisitor::visitModuleStart(const ModulePtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitModuleEnd(const ModulePtr&)
{
}

void
Slice::Gen::MetaDataVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
}

bool
Slice::Gen::MetaDataVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitClassDefEnd(const ClassDefPtr&)
{
}

bool
Slice::Gen::MetaDataVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitExceptionEnd(const ExceptionPtr&)
{
}

bool
Slice::Gen::MetaDataVisitor::visitStructStart(const StructPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitStructEnd(const StructPtr&)
{
}

void
Slice::Gen::MetaDataVisitor::visitOperation(const OperationPtr& p)
{

    bool ami = false;
    ClassDefPtr cl = ClassDefPtr::dynamicCast(p->container());
    if(cl->hasMetaData("amd") || p->hasMetaData("amd"))
    {
        ami = true;
    }

    if(p->hasMetaData("UserException"))
    {
        if(!cl->isLocal())
        {
            ostringstream ostr;
            ostr << "ignoring invalid metadata `UserException': directive applies only to local operations "
                 << "but enclosing " << (cl->isInterface() ? "interface" : "class") << "`" << cl->name()
                 << "' is not local";
            emitWarning(p->file(), p->line(), ostr.str());
        }
    }

    StringList metaData = p->getMetaData();
    metaData.remove("cpp:const");

    TypePtr returnType = p->returnType();
    if(!metaData.empty())
    {
        if(!returnType)
        {
            for(StringList::const_iterator q = metaData.begin(); q != metaData.end(); ++q)
            {
                if(q->find("cpp:type:", 0) == 0 || q->find("cpp:view-type:", 0) == 0
                   || (*q) == "cpp:array" || q->find("cpp:range", 0) == 0)
                {
                    emitWarning(p->file(), p->line(), "ignoring invalid metadata `" + *q +
                                "' for operation with void return type");
                    break;
                }
            }
        }
        else
        {
            validate(returnType, metaData, p->file(), p->line(), ami);
        }
    }

    ParamDeclList params = p->parameters();
    for(ParamDeclList::iterator q = params.begin(); q != params.end(); ++q)
    {
        validate((*q)->type(), (*q)->getMetaData(), p->file(), (*q)->line(), ami || !(*q)->isOutParam());
    }
}

void
Slice::Gen::MetaDataVisitor::visitDataMember(const DataMemberPtr& p)
{
    validate(p->type(), p->getMetaData(), p->file(), p->line());
}

void
Slice::Gen::MetaDataVisitor::visitSequence(const SequencePtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
}

void
Slice::Gen::MetaDataVisitor::visitDictionary(const DictionaryPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
}

void
Slice::Gen::MetaDataVisitor::visitEnum(const EnumPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
}

void
Slice::Gen::MetaDataVisitor::visitConst(const ConstPtr& p)
{
    validate(p, p->getMetaData(), p->file(), p->line());
}

void
Slice::Gen::MetaDataVisitor::validate(const SyntaxTreeBasePtr& cont, const StringList& metaData,
                                      const string& file, const string& line, bool /*inParam*/)
{
    static const string cppPrefix = "cpp:";
    static const string cpp11Prefix = "cpp11:";

    for(StringList::const_iterator p = metaData.begin(); p != metaData.end(); ++p)
    {
        string s = *p;
        if(_history.count(s) == 0)
        {
            bool cpp = s.find(cppPrefix) == 0;
            bool cpp11 = s.find(cpp11Prefix) == 0;
            if(cpp || cpp11)
            {
                const string prefix = cpp ? cppPrefix : cpp11Prefix;

                string ss = s.substr(prefix.size());
                if(ss == "type:wstring" || ss == "type:string")
                {
                    BuiltinPtr builtin = BuiltinPtr::dynamicCast(cont);
                    ModulePtr module = ModulePtr::dynamicCast(cont);
                    ClassDefPtr clss = ClassDefPtr::dynamicCast(cont);
                    StructPtr strct = StructPtr::dynamicCast(cont);
                    ExceptionPtr exception = ExceptionPtr::dynamicCast(cont);
                    if((builtin && builtin->kind() == Builtin::KindString) || module || clss || strct || exception)
                    {
                        continue;
                    }
                }
                if(BuiltinPtr::dynamicCast(cont) && (ss.find("type:") == 0 || ss.find("view-type:") == 0))
                {
                    if(BuiltinPtr::dynamicCast(cont)->kind() == Builtin::KindString)
                    {
                        continue;
                    }
                }
                if(SequencePtr::dynamicCast(cont))
                {
                    if(ss.find("type:") == 0 || ss.find("view-type:") == 0 || ss == "array" || ss.find("range") == 0)
                    {
                        continue;
                    }
                }
                if(DictionaryPtr::dynamicCast(cont) && (ss.find("type:") == 0 || ss.find("view-type:") == 0))
                {
                    continue;
                }
                if(StructPtr::dynamicCast(cont) && (ss == "class" || ss == "comparable"))
                {
                    continue;
                }

                {
                    ClassDefPtr cl = ClassDefPtr::dynamicCast(cont);
                    if(cl && ((cpp && ss == "virtual") ||
                              (cpp11 && cl->isLocal() && ss.find("type:") == 0) ||
                              (cl->isLocal() && ss == "comparable")))
                    {
                        continue;
                    }
                }
                if(ExceptionPtr::dynamicCast(cont) && ss == "ice_print")
                {
                    continue;
                }
                if(EnumPtr::dynamicCast(cont) && ss == "unscoped")
                {
                    continue;
                }

                {
                    ClassDeclPtr cl = ClassDeclPtr::dynamicCast(cont);
                    if(cl && cpp11 && cl->isLocal() && ss.find("type:") == 0)
                    {
                        continue;
                    }
                }
                emitWarning(file, line, "ignoring invalid metadata `" + s + "'");
            }
            if(s.find("delegate") == 0)
            {
                ClassDefPtr cl = ClassDefPtr::dynamicCast(cont);
                if(cl && cl->isDelegate())
                {
                    continue;
                }
                emitWarning(file, line, "ignoring invalid metadata `" + s + "'");
            }
            _history.insert(s);
        }
    }
}

int
Slice::Gen::setUseWstring(ContainedPtr p, list<int>& hist, int use)
{
    hist.push_back(use);
    StringList metaData = p->getMetaData();
    if(find(metaData.begin(), metaData.end(), "cpp:type:wstring") != metaData.end())
    {
        use = TypeContextUseWstring;
    }
    else if(find(metaData.begin(), metaData.end(), "cpp:type:string") != metaData.end())
    {
        use = 0;
    }
    return use;
}

int
Slice::Gen::resetUseWstring(list<int>& hist)
{
    int use = hist.back();
    hist.pop_back();
    return use;
}

string
Slice::Gen::getHeaderExt(const string& file, const UnitPtr& unit)
{
    string ext;
    static const string headerExtPrefix = "cpp:header-ext:";
    DefinitionContextPtr dc = unit->findDefinitionContext(file);
    assert(dc);
    string meta = dc->findMetaData(headerExtPrefix);
    if(meta.size() > headerExtPrefix.size())
    {
        ext = meta.substr(headerExtPrefix.size());
    }
    return ext;
}

// C++11 visitors
Slice::Gen::Cpp11DeclVisitor::Cpp11DeclVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::Cpp11DeclVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasClassDecls() && !p->hasNonLocalExceptions())
    {
        return false;
    }
    C << sp << nl << "namespace" << nl << "{";
    return true;
}

void
Slice::Gen::Cpp11DeclVisitor::visitUnitEnd(const UnitPtr& p)
{
    C << sp << nl << "}";
}

bool
Slice::Gen::Cpp11DeclVisitor::visitModuleStart(const ModulePtr& p)
{
    if(p->hasClassDecls())
    {
        H << sp << nl << "namespace " << fixKwd(p->name()) << nl << '{' << sp;
    }
    return true;
}

void
Slice::Gen::Cpp11DeclVisitor::visitModuleEnd(const ModulePtr& p)
{
    if(p->hasClassDecls())
    {
        H << sp << nl << '}';
    }
}

void
Slice::Gen::Cpp11DeclVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    ClassDefPtr def = p->definition();
    if(def && def->isDelegate())
    {
        return;
    }

    H << nl << "class " << fixKwd(p->name()) << ';';
    if(p->isInterface() || (def && !def->allOperations().empty()))
    {
        H << nl << "class " << p->name() << "Prx;";
    }
}

bool
Slice::Gen::Cpp11DeclVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    if(!p->isInterface())
    {
        C << sp;

        C << nl << "const ::IceInternal::DefaultValueFactoryInit<" << fixKwd(p->scoped()) << "> ";
        C << p->flattenedScope() + p->name() + "_init" << "(\"" << p->scoped() << "\");";

        if(p->compactId() >= 0)
        {
            string n = p->flattenedScope() + p->name() + "_compactIdInit ";
            C << "const ::IceInternal::CompactIdInit " << n << "(\"" << p->scoped() << "\", " << p->compactId() << ");";
        }
    }

    OperationList allOps = p->allOperations();
    if(p->isInterface() || !allOps.empty())
    {
        C << sp;

        ClassList allBases = p->allBases();
        StringList ids;
        transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun(&Contained::scoped));
        StringList other;
        other.push_back(p->scoped());
        other.push_back("::Ice::Object");
        other.sort();
        ids.merge(other);
        ids.unique();

        C << nl << "const ::std::string " << p->flattenedScope() << p->name() << "_ids[" << ids.size() << "] =";
        C << sb;
        for(StringList::const_iterator r = ids.begin(); r != ids.end();)
        {
            C << nl << '"' << *r << '"';
            if(++r != ids.end())
            {
                C << ',';
            }
        }
        C << eb << ';';

        StringList allOpNames;
        transform(allOps.begin(), allOps.end(), back_inserter(allOpNames), ::IceUtil::constMemFun(&Contained::name));
        allOpNames.push_back("ice_id");
        allOpNames.push_back("ice_ids");
        allOpNames.push_back("ice_isA");
        allOpNames.push_back("ice_ping");
        allOpNames.sort();
        allOpNames.unique();

        C << nl << "const ::std::string " << p->flattenedScope() << p->name() << "_ops[] =";
        C << sb;
        for(StringList::const_iterator q = allOpNames.begin(); q != allOpNames.end();)
        {
            C << nl << '"' << *q << '"';
            if(++q != allOpNames.end())
            {
                C << ',';
            }
        }
        C << eb << ';';
    }

    return true;
}

bool
Slice::Gen::Cpp11DeclVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    C << sp;
    C << nl << "const ::IceInternal::DefaultUserExceptionFactoryInit<" << fixKwd(p->scoped()) << "> ";
    C << p->flattenedScope() + p->name() + "_init" << "(\"" << p->scoped() << "\");";
    return false;
}

void
Slice::Gen::Cpp11DeclVisitor::visitOperation(const OperationPtr& p)
{
    ClassDefPtr cl = ClassDefPtr::dynamicCast(p->container());
    if(cl && !cl->isLocal())
    {
        string flatName = p->flattenedScope() + p->name() + "_name";
        C << nl << "const ::std::string " << flatName << " = \"" << p->name() << "\";";
    }
}

Slice::Gen::Cpp11TypesVisitor::Cpp11TypesVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _doneStaticSymbol(false), _useWstring(false)
{
}

bool
Slice::Gen::Cpp11TypesVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasOtherConstructedOrExceptions())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    H << sp << nl << "namespace " << fixKwd(p->name()) << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11TypesVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';
    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11TypesVisitor::visitClassDefStart(const ClassDefPtr&)
{
    return false;
}

bool
Slice::Gen::Cpp11TypesVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    ExceptionPtr base = p->base();
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();
    DataMemberList baseDataMembers;

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;
    vector<string> baseParams;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
        allTypes.push_back(typeName);
        allParamDecls.push_back(typeName + " " + fixKwd("__ice_" + (*q)->name()));
    }

    if(base)
    {
        baseDataMembers = base->allDataMembers();
        for(DataMemberList::const_iterator q = baseDataMembers.begin(); q != baseDataMembers.end(); ++q)
        {
            baseParams.push_back(fixKwd("__ice_" + (*q)->name()));
        }
    }

    H << sp << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    H << "public ";
    if(!base)
    {
        H << (p->isLocal() ? "::Ice::LocalException" : "::Ice::UserException");
    }
    else
    {
        H << fixKwd(base->scoped());
    }
    H.restoreIndent();
    H << sb;

    H.dec();
    H << nl << "public:";
    H.inc();

    if(p->isLocal())
    {
        H << sp << nl << name << "(const char* __ice_file, int __ice_line) : ";
        H << (base ? fixKwd(base->scoped()) : "::Ice::LocalException") << "(__ice_file, __ice_line)";
        H << sb;
        H << eb;
    }
    else
    {
        H << sp << nl << name << "() = default;";
    }

    if(!allDataMembers.empty())
    {
        H << sp << nl << name << "(";
        if(p->isLocal())
        {
            H << "const char* __ice_file, int __ice_line";
            if(!allParamDecls.empty())
            {
                H << ", ";
            }
        }

        for(vector<string>::const_iterator q = allParamDecls.begin(); q != allParamDecls.end(); ++q)
        {
            if(q != allParamDecls.begin())
            {
                H << ", ";
            }
            H << (*q);
        }
        H << ") :";
        H.inc();
        if(base && (p->isLocal() || !baseDataMembers.empty()))
        {
            H << nl << fixKwd(base->scoped()) << "(";
            if(p->isLocal())
            {
                H << "__ice_file, __ice_line";
                if(!baseDataMembers.empty())
                {
                    H << ", ";
                }
            }

            for(DataMemberList::const_iterator q = baseDataMembers.begin(); q != baseDataMembers.end(); ++q)
            {
                if(q != baseDataMembers.begin())
                {
                    H << ", ";
                }
                if(isMovable((*q)->type()))
                {
                    H << "::std::move(" << fixKwd("__ice_" + (*q)->name()) << ")";
                }
                else
                {
                    H << fixKwd("__ice_" + (*q)->name());
                }
            }

            H << ")";
            if(!dataMembers.empty())
            {
                H << ",";
            }
        }
        else if(p->isLocal())
        {
            H << nl << "::Ice::LocalException(__ice_file, __ice_line)";
            if(!dataMembers.empty())
            {
                H << ",";
            }
        }

        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            if(q != dataMembers.begin())
            {
                H << ", ";
            }
            if(isMovable((*q)->type()))
            {
                H << nl << fixKwd((*q)->name()) << "(::std::move(" << fixKwd("__ice_" + (*q)->name()) << "))";
            }
            else
            {
                H << nl << fixKwd((*q)->name()) << "(" << fixKwd("__ice_" + (*q)->name()) << ")";
            }
        }

        H.dec();
        H << sb;
        H << eb;
    }
    H << sp;

    H << nl << "virtual ::std::string ice_id() const;";
    C << sp << nl << "::std::string" << nl << scoped.substr(2) << "::ice_id() const";
    C << sb;
    if(p->isLocal())
    {
        C << nl << "return \"" << p->scoped() << "\";" << ";";
    }
    else
    {
        C << nl << "return " << p->flattenedScope() << p->name() << "_init.typeId" << ";";
    }
    C << eb;

    StringList metaData = p->getMetaData();
    if(find(metaData.begin(), metaData.end(), "cpp:ice_print") != metaData.end())
    {
        H << nl << "virtual void ice_print(::std::ostream&) const;";
    }

    H << nl << "virtual void ice_throw() const;";
    C << sp << nl << "void" << nl << scoped.substr(2) << "::ice_throw() const";
    C << sb;
    C << nl << "throw *this;";
    C << eb;

    if(!p->isLocal() && p->usesClasses(false))
    {
        if(!base || (base && !base->usesClasses(false)))
        {
            H << sp << nl << "virtual bool __usesClasses() const;";

            C << sp << nl << "bool";
            C << nl << scoped.substr(2) << "::__usesClasses() const";
            C << sb;
            C << nl << "return true;";
            C << eb;
        }
    }

    if(!dataMembers.empty())
    {
        H << sp;
    }
    return true;
}

void
Slice::Gen::Cpp11TypesVisitor::visitExceptionEnd(const ExceptionPtr& p)
{
    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    string factoryName;

    if(!p->isLocal())
    {
        ExceptionPtr base = p->base();
        bool basePreserved = p->inheritsMetaData("preserve-slice");
        bool preserved = p->hasMetaData("preserve-slice");
        string typeId = p->flattenedScope() + p->name() + "_init.typeId";

        if(preserved && !basePreserved)
        {
            H << sp << nl << "virtual void __write(::Ice::OutputStream*) const;";
            H << nl << "virtual void __read(::Ice::InputStream*);";
        }

        H.dec();
        H << sp << nl << "protected:";
        H.inc();
        H << sp;
        H << nl << "virtual void __writeImpl(::Ice::OutputStream*) const;";
        H << nl << "virtual void __readImpl(::Ice::InputStream*);";

        if(preserved && !basePreserved)
        {
            H << sp << nl << "::std::shared_ptr<::Ice::SlicedData> __slicedData;";

            C << sp << nl << "void" << nl << scoped.substr(2) << "::__write(::Ice::OutputStream* __os) const";
            C << sb;
            C << nl << "__os->startException(__slicedData);";
            C << nl << "__writeImpl(__os);";
            C << nl << "__os->endException();";
            C << eb;

            C << sp << nl << "void" << nl << scoped.substr(2) << "::__read(::Ice::InputStream* __is)";
            C << sb;
            C << nl << "__is->startException();";
            C << nl << "__readImpl(__is);";
            C << nl << "__slicedData = __is->endException(true);";
            C << eb;
        }

        C << sp << nl << "void" << nl << scoped.substr(2) << "::__writeImpl(::Ice::OutputStream* __os) const";
        C << sb;
        C << nl << "__os->startSlice(" << typeId << ", -1, " << (!base ? "true" : "false") << ");";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), true);
        C << nl << "__os->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__writeImpl(__os);");
        }
        C << eb;

        C << sp << nl << "void" << nl << scoped.substr(2) << "::__readImpl(::Ice::InputStream* __is)";
        C << sb;
        C << nl << "__is->startSlice();";
        writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), false);
        C << nl << "__is->endSlice();";
        if(base)
        {
            emitUpcall(base, "::__readImpl(__is);");
        }
        C << eb;
    }
    H << eb << ';';

    if(!p->isLocal())
    {
        //
        // We need an instance here to trigger initialization if the implementation is in a shared libarry.
        // But we do this only once per source file, because a single instance is sufficient to initialize
        // all of the globals in a shared library.
        //
        if(!_doneStaticSymbol)
        {
            _doneStaticSymbol = true;
            H << sp << nl << "static " << name << " __" << p->name() << "_init;";
        }
    }

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11TypesVisitor::visitStructStart(const StructPtr& p)
{
    DataMemberList dataMembers = p->dataMembers();
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());


    H << sp << nl << "struct " << name;
    H << sb;

    return true;
}

void
Slice::Gen::Cpp11TypesVisitor::visitStructEnd(const StructPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    DataMemberList dataMembers = p->dataMembers();

    vector<string> params;
    vector<string>::const_iterator pi;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    bool containsSequence = false;
    if((Dictionary::legalKeyType(p, containsSequence) && !containsSequence))
    {
        H << sp << nl << "bool operator==(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "if(this == &__rhs)";
        H << sb;
        H << nl << "return true;";
        H << eb;
        for(vector<string>::const_iterator pi = params.begin(); pi != params.end(); ++pi)
        {
            H << nl << "if(" << *pi << " != __rhs." << *pi << ')';
            H << sb;
            H << nl << "return false;";
            H << eb;
        }
        H << nl << "return true;";
        H << eb;
        H << sp << nl << "bool operator<(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "if(this == &__rhs)";
        H << sb;
        H << nl << "return false;";
        H << eb;
        for(vector<string>::const_iterator pi = params.begin(); pi != params.end(); ++pi)
        {
            H << nl << "if(" << *pi << " < __rhs." << *pi << ')';
            H << sb;
            H << nl << "return true;";
            H << eb;
            H << nl << "else if(__rhs." << *pi << " < " << *pi << ')';
            H << sb;
            H << nl << "return false;";
            H << eb;
        }
        H << nl << "return false;";
        H << eb;

        H << sp << nl << "bool operator!=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator==(__rhs);";
        H << eb;
        H << nl << "bool operator<=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return operator<(__rhs) || operator==(__rhs);";
        H << eb;
        H << nl << "bool operator>(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator<(__rhs) && !operator==(__rhs);";
        H << eb;
        H << nl << "bool operator>=(const " << name << "& __rhs) const";
        H << sb;
        H << nl << "return !operator<(__rhs);";
        H << eb;
    }
    H << eb << ';';
    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::Cpp11TypesVisitor::visitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    H << nl << typeToString(p->type(), p->optional(), p->getMetaData(), _useWstring, true) << ' ' << name;

    string defaultValue = p->defaultValue();
    if(!defaultValue.empty())
    {
        H << " = ";
        writeConstantValue(H, p->type(), p->defaultValueType(), defaultValue, _useWstring, p->getMetaData(), true);
    }

    H << ';';
}

void
Slice::Gen::Cpp11TypesVisitor::visitSequence(const SequencePtr& p)
{
    string name = fixKwd(p->name());
    TypePtr type = p->type();
    int typeCtx = p->isLocal() ? (_useWstring | TypeContextLocal) : _useWstring;
    string s = typeToString(type, p->typeMetaData(), typeCtx, true);
    StringList metaData = p->getMetaData();

    string seqType = findMetaData(metaData, _useWstring);
    H << sp;

    if(!seqType.empty())
    {
        H << nl << "typedef " << seqType << ' ' << name << ';';
    }
    else
    {
        H << nl << "typedef ::std::vector<" << (s[0] == ':' ? " " : "") << s << "> " << name << ';';
    }
}

void
Slice::Gen::Cpp11TypesVisitor::visitDictionary(const DictionaryPtr& p)
{
    string name = fixKwd(p->name());
    string dictType = findMetaData(p->getMetaData());
    int typeCtx = p->isLocal() ? (_useWstring | TypeContextLocal) : _useWstring;
    if(dictType.empty())
    {
        //
        // A default std::map dictionary
        //

        TypePtr keyType = p->keyType();
        TypePtr valueType = p->valueType();
        string ks = typeToString(keyType, p->keyMetaData(), typeCtx, true);
        if(ks[0] == ':')
        {
            ks.insert(0, " ");
        }
        string vs = typeToString(valueType, p->valueMetaData(), typeCtx, true);

        H << sp << nl << "typedef ::std::map<" << ks << ", " << vs << "> " << name << ';';
    }
    else
    {
        //
        // A custom dictionary
        //
        H << sp << nl << "typedef " << dictType << ' ' << name << ';';
    }
}

Slice::Gen::Cpp11ProxyVisitor::Cpp11ProxyVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _useWstring(false)
{
}

bool
Slice::Gen::Cpp11ProxyVisitor::visitUnitStart(const UnitPtr& p)
{
    return true;
}

void
Slice::Gen::Cpp11ProxyVisitor::visitUnitEnd(const UnitPtr&)
{
}

bool
Slice::Gen::Cpp11ProxyVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    H << sp << nl << "namespace " << fixKwd(p->name()) << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11ProxyVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11ProxyVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal() || (!p->isInterface() && p->allOperations().empty()))
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }

    H << sp << nl << "class " << _dllExport << p->name() << "Prx : public virtual ::Ice::Proxy<"
      << fixKwd(p->name() + "Prx") << ", ";
    if(bases.empty() || (base && base->allOperations().empty()))
    {
        H << "::Ice::ObjectPrx";
    }
    else
    {
        ClassList::const_iterator q = bases.begin();
        while(q != bases.end())
        {
            H << fixKwd((*q)->scoped() + "Prx");
            if(++q != bases.end())
            {
                H << ", ";
            }
        }
    }
    H << ">";

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();
    return true;
}

void
Slice::Gen::Cpp11ProxyVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string prx = fixKwd(p->name() + "Prx");

    H << sp;
    H << nl << "static const ::std::string& ice_staticId();";

    H.dec();
    H << sp << nl << "protected: ";
    H.inc();
    H << sp << nl << prx << "() = default;";
    H << nl << "friend ::std::shared_ptr<" << prx << "> IceInternal::createProxy<" << prx << ">();";
    H << eb << ';';

    string suffix = p->isInterface() ? "" : "Disp";
    string scoped = fixKwd(p->scoped() + "Prx");

    C << sp;
    C << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_staticId()";
    C << sb;
    C << nl << "return "<< fixKwd(p->scope() + p->name() + suffix).substr(2) << "::ice_staticId();";
    C << eb;

    _useWstring = resetUseWstring(_useWstringHist);
}

void
Slice::Gen::Cpp11ProxyVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string flatName = p->flattenedScope() + p->name() + "_name";

    TypePtr ret = p->returnType();

    bool retIsOpt = p->returnIsOptional();
    string retS = returnTypeToString(ret, retIsOpt, p->getMetaData(), _useWstring | TypeContextAMIEnd, true);

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);

    vector<string> params;
    vector<string> paramsDecl;

    vector<string> lambdaParams;
    vector<string> lambdaParamsDecl;

    vector<string> futureParamsDecl;

    vector<string> lambdaOutParams;
    vector<string> lambdaOutParamsDecl;

    ParamDeclList paramList = p->parameters();
    ParamDeclList inParams;
    ParamDeclList outParams;

    string returnValueS = "returnValue";

    bool lambdaOutParamsHasOpt = false;
    if(ret)
    {
        lambdaOutParams.push_back(retS);
        lambdaOutParamsDecl.push_back(retS + " returnValue");
        lambdaOutParamsHasOpt |= p->returnIsOptional();
    }

    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd(paramPrefix + (*q)->name());

        StringList metaData = (*q)->getMetaData();
        string typeString;
        string outputTypeString;
        string typeStringEndAMI;
        if((*q)->isOutParam())
        {
            typeString = typeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true);
            outputTypeString = outputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true);
        }
        else
        {
            typeString = inputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true);
        }

        if(!(*q)->isOutParam())
        {
            params.push_back(typeString);
            paramsDecl.push_back(typeString + ' ' + paramName);

            lambdaParams.push_back(typeString);
            lambdaParamsDecl.push_back(typeString + ' ' + paramName);

            futureParamsDecl.push_back(typeString + ' ' + paramName);

            inParams.push_back(*q);
        }
        else
        {
            params.push_back(outputTypeString);
            paramsDecl.push_back(outputTypeString + ' ' + paramName);

            lambdaOutParams.push_back(typeString);
            lambdaOutParamsDecl.push_back(typeString + ' ' + paramName);
            lambdaOutParamsHasOpt |= (*q)->optional();

            outParams.push_back(*q);
            if((*q)->name() == "returnValue")
            {
                returnValueS = "_returnValue";
            }
        }
    }

    string scoped = fixKwd(cl->scope() + cl->name() + "Prx" + "::").substr(2);

    string futureT;
    if(lambdaOutParams.empty())
    {
        futureT = "void";
    }
    else if(lambdaOutParams.size() == 1)
    {
        futureT = lambdaOutParams[0];
    }
    else
    {
        futureT = "Result_" + name;
    }

    if(lambdaOutParams.size() > 1)
    {
        // We need to generate a Result_ struct.
        H << sp;
        H << nl << "struct Result_" << name;
        H << sb;
        for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
        {
            string typeString = typeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
            H << nl << typeString << " " << fixKwd((*q)->name()) << ";";
        }
        if(ret)
        {
            H << nl << retS << " " << returnValueS << ";";
        }
        H << eb << ";";
    }

    string deprecateSymbol = getDeprecateSymbol(p, cl);

    //
    // Synchronous operation
    //
    H << sp << nl << deprecateSymbol << retS << ' ' << fixKwd(name) << spar << paramsDecl;
    H << "const ::Ice::Context& __ctx = Ice::noExplicitContext" << epar;
    H << sb;
    H << nl;
    if(lambdaOutParams.size() == 1)
    {
        if(ret)
        {
            H << "return ";
        }
        else
        {
            H << paramPrefix << (*outParams.begin())->name() << " = ";
        }
    }
    else if(lambdaOutParams.size() > 1)
    {
        H << "auto __result = ";
    }
    if(futureT == "void")
    {
        H << nl << "makePromiseOutgoing";
    }
    else
    {
        H << nl << "makePromiseOutgoing<" << futureT << ">";
    }
    H << spar << "true, this" << string("&" + scoped + "__" + name);
    for(ParamDeclList::const_iterator q = inParams.begin(); q != inParams.end(); ++q)
    {
        H << fixKwd(paramPrefix + (*q)->name());
    }
    H << "__ctx" << epar << ".get();";
    if(lambdaOutParams.size() > 1)
    {
        for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
        {
            H << nl << paramPrefix << (*q)->name() << " = ";
            if(isMovable((*q)->type()))
            {
                H << "::std::move(__result." << fixKwd((*q)->name()) << ");";
            }
            else
            {
                H << "__result." << fixKwd((*q)->name()) << ";";
            }
        }
        if(ret)
        {
            if(isMovable(ret))
            {
                H << nl << "return ::std::move(__result." << returnValueS << ");";
            }
            else
            {
                H << nl << "return __result." << returnValueS << ";";
            }
        }
    }
    H << eb;

    //
    // Lambda based asynchronous operation
    //
    H << sp;
    H << nl << "::std::function<void ()>";
    H << nl << name << "_async(";
    H.useCurrentPosAsIndent();
    if(!lambdaParamsDecl.empty())
    {
        for(vector<string>::const_iterator q = lambdaParamsDecl.begin(); q != lambdaParamsDecl.end(); ++q)
        {
            H << *q << ", ";
        }
        H << nl;
    }
    H << "::std::function<void " << spar << lambdaOutParams << epar << "> __response,";
    H << nl << "::std::function<void (::std::exception_ptr)> __ex = nullptr,";
    H << nl << "::std::function<void (bool)> __sent = nullptr,";
    H << nl << "const ::Ice::Context& __ctx = Ice::noExplicitContext)";
    H.restoreIndent();
    H << sb;
    if(lambdaOutParams.size() > 1)
    {
        H << nl << "auto __responseCb = [__response](" << futureT << "&& result)";
        H << sb;
        H << nl << "__response" << spar;
        if(ret)
        {
            if(isMovable(ret))
            {
                H << "::std::move(result." + returnValueS + ")";
            }
            else
            {
                H << "result." + returnValueS;
            }
        }
        for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
        {
            if(isMovable((*q)->type()))
            {
                H << "::std::move(result." + fixKwd((*q)->name()) + ")";
            }
            else
            {
                H << "result." + fixKwd((*q)->name());
            }
        }
        H << epar << ";" << eb << ";";
    }
    if(futureT == "void")
    {
        H << nl << "return makeLambdaOutgoing" << spar;
    }
    else
    {
        H << nl << "return makeLambdaOutgoing<" << futureT << ">" << spar;
    }
    H << (lambdaOutParams.size() > 1 ? "__responseCb" : "__response") << "__ex" << "__sent" << "this";
    H << string("&" + scoped + "__" + name);
    for(ParamDeclList::const_iterator q = inParams.begin(); q != inParams.end(); ++q)
    {
        H << fixKwd(paramPrefix + (*q)->name());
    }
    H << "__ctx" << epar << ";";
    H << eb;

    //
    // Promise based asynchronous operation
    //
    H << sp;
    H << nl << "template<template<typename> class P = ::std::promise>";
    H << nl << deprecateSymbol << "auto " << name << "_async" << spar << futureParamsDecl;
    H << "const ::Ice::Context& __ctx = Ice::noExplicitContext" << epar;
    H.inc();
    H << nl << "-> decltype(::std::declval<P<" << futureT << ">>().get_future())";
    H.dec();
    H << sb;
    if(futureT == "void")
    {
        H << nl << "return makePromiseOutgoing<P>" << spar;
    }
    else
    {
        H << nl << "return makePromiseOutgoing<" << futureT << ", P>" << spar;
    }
    H << "false, this" << string("&" + scoped + "__" + name);
    for(ParamDeclList::const_iterator q = inParams.begin(); q != inParams.end(); ++q)
    {
        H << fixKwd(paramPrefix + (*q)->name());
    }
    H << "__ctx" << epar << ";";
    H << eb;

    //
    // Private implementation
    //
    H << sp;
    H << nl << "void __" << name << spar;
    H << "const ::std::shared_ptr<::IceInternal::OutgoingAsyncT<" + futureT + ">>&";
    H << lambdaParams;
    H << "const ::Ice::Context& = Ice::noExplicitContext";
    H << epar << ";";

    C << sp;
    C << nl << "void" << nl << scoped << "__" << name << spar;
    C << "const ::std::shared_ptr<::IceInternal::OutgoingAsyncT<" + futureT + ">>& __outAsync";
    C << lambdaParamsDecl << "const ::Ice::Context& __ctx";
    C << epar;
    C << sb;
    if(p->returnsData())
    {
        C << nl << "__checkAsyncTwowayOnly(" << flatName << ");";
    }
    C << nl << "__outAsync->invoke(" << flatName << ", ";
    C << operationModeToString(p->sendMode(), true) << ", " << opFormatTypeToString(p) << ", __ctx, ";
    C.inc();
    C << nl;
    if(inParams.empty())
    {
        C << "nullptr";
    }
    else
    {
        C << "[&](::Ice::OutputStream* __os)";
        C << sb;
        writeMarshalCode(C, inParams, 0, true, TypeContextInParam);
        if(p->sendsClasses(false))
        {
            C << nl << "__os->writePendingObjects();";
        }
        C << eb;
    }
    C << "," << nl;

    ExceptionList throws = p->throws();
    if(throws.empty())
    {
        C << "nullptr";
    }
    else
    {
        throws.sort();
        throws.unique();

        //
        // Arrange exceptions into most-derived to least-derived order. If we don't
        // do this, a base exception handler can appear before a derived exception
        // handler, causing compiler warnings and resulting in the base exception
        // being marshaled instead of the derived exception.
        //
        throws.sort(Slice::DerivedToBaseCompare());

        C << "[](const ::Ice::UserException& __ex)";
        C << sb;
        C << nl << "try";
        C << sb;
        C << nl << "__ex.ice_throw();";
        C << eb;
        //
        // Generate a catch block for each legal user exception.
        //
        for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
        {
            string scoped = (*i)->scoped();
            C << nl << "catch(const " << fixKwd((*i)->scoped()) << "&)";
            C << sb;
            C << nl << "throw;";
            C << eb;
        }
        C << nl << "catch(const ::Ice::UserException&)";
        C << sb;
        C << eb;
        C << eb;
    }

    if(lambdaOutParams.size() > 1)
    {
        //
        // Generate a read method if there are more than one ret/out parameter. If there's
        // only one, we rely on the default read method from LambdaOutgoing.
        //
        C << "," << nl << "[&](::Ice::InputStream* __is)";
        C << sb;
        C << nl << futureT << " v;";
        ParamDeclList optionals;
        for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
        {
            if((*q)->optional())
            {
                optionals.push_back(*q);
            }
            else
            {
                string name = "v." + fixKwd((*q)->name());
                writeMarshalUnmarshalCode(C, (*q)->type(), false, 0, name, false, (*q)->getMetaData());
            }
        }
        if(ret && !retIsOpt)
        {
            string name = "v." + returnValueS;
            writeMarshalUnmarshalCode(C, ret, false, 0, name, false, p->getMetaData());
        }

        //
        // Sort optional parameters by tag.
        //
        class SortFn
        {
        public:
            static bool compare(const ParamDeclPtr& lhs, const ParamDeclPtr& rhs)
            {
                return lhs->tag() < rhs->tag();
            }
        };
        optionals.sort(SortFn::compare);

        //
        // Marshal optional parameters.
        //
        bool checkReturnType = ret && retIsOpt;
        for(ParamDeclList::const_iterator q = optionals.begin(); q != optionals.end(); ++q)
        {
            if(checkReturnType && p->returnTag() < (*q)->tag())
            {
                string name = "v." + returnValueS;
                writeMarshalUnmarshalCode(C, ret, true, p->returnTag(), name, false, p->getMetaData());
                checkReturnType = false;
            }
            string name = "v." + fixKwd((*q)->name());
            writeMarshalUnmarshalCode(C, (*q)->type(), true, (*q)->tag(), name, false, (*q)->getMetaData());
        }
        if(checkReturnType)
        {
            string name = "v." + returnValueS;
            writeMarshalUnmarshalCode(C, ret, true, p->returnTag(), name, false, p->getMetaData());
        }
        if(p->returnsClasses(false))
        {
            C << nl << "__is->readPendingObjects();";
        }
        C << nl << "return v;";
        C << eb;
    }
    else if(lambdaOutParamsHasOpt)
    {
        //
        // If there's only one optional ret/out parameter, we still need to generate
        // a read method, we can't rely on the default read method which wouldn't
        // known which tag to use.
        //
        C << "," << nl << "[&](::Ice::InputStream* __is)";
        C << sb;
        C << nl << futureT << " v;";
        if(p->returnIsOptional())
        {
            writeMarshalUnmarshalCode(C, ret, true, p->returnTag(), "v", false, p->getMetaData());
        }
        else
        {
            assert(outParams.size() == 1);
            ParamDeclPtr q = (*outParams.begin());
            writeMarshalUnmarshalCode(C, q->type(), true, q->tag(), "v", false, q->getMetaData());
        }
        C << nl << "return v;";
        C << eb;
    }
    else if(p->returnsClasses(false))
    {
        C << "," << nl << "[&](::Ice::InputStream* __is)";
        C << sb;
        C << nl << futureT << " v;";
        if(ret)
        {
            writeMarshalUnmarshalCode(C, ret, false, 0, "v", false, p->getMetaData());
        }
        else
        {
            assert(outParams.size() == 1);
            ParamDeclPtr q = (*outParams.begin());
            writeMarshalUnmarshalCode(C, q->type(), false, 0, "v", false, q->getMetaData());
        }
        C << nl << "__is->readPendingObjects();";
        C << nl << "return v;";
        C << eb;
    }

    C.dec();

    C << ");" << eb;
}

void
Slice::Gen::Cpp11TypesVisitor::visitEnum(const EnumPtr& p)
{
    bool unscoped = findMetaData(p->getMetaData()) == "%unscoped";
    H << sp << nl << "enum ";
    if(!unscoped)
    {
        H << "class ";
    }
    H << fixKwd(p->name());
    if(!unscoped && p->maxValue() <= 0xFF)
    {
        H << " : unsigned char";
    }
    H << sb;

    EnumeratorList enumerators = p->getEnumerators();
    //
    // Check if any of the enumerators were assigned an explicit value.
    //
    const bool explicitValue = p->explicitValue();
    for(EnumeratorList::const_iterator en = enumerators.begin(); en != enumerators.end();)
    {
        H << nl << fixKwd((*en)->name());
        //
        // If any of the enumerators were assigned an explicit value, we emit
        // an explicit value for *all* enumerators.
        //
        if(explicitValue)
        {
            H << " = " << int64ToString((*en)->value());
        }
        if(++en != enumerators.end())
        {
            H << ',';
        }
    }
    H << eb << ';';
}

void
Slice::Gen::Cpp11TypesVisitor::visitConst(const ConstPtr& p)
{
    H << sp;
    H << nl << (isConstexprType(p->type()) ? "constexpr " : "const ") << typeToString(p->type(), p->typeMetaData(), _useWstring, true) << " " << fixKwd(p->name())
      << " = ";
    writeConstantValue(H, p->type(), p->valueType(), p->value(), _useWstring, p->typeMetaData(), true);
    H << ';';
}

void
Slice::Gen::Cpp11TypesVisitor::emitUpcall(const ExceptionPtr& base, const string& call, bool isLocal)
{
    C << nl << (base ? fixKwd(base->scoped()) : string(isLocal ? "::Ice::LocalException" : "::Ice::UserException"))
      << call;
}

Slice::Gen::Cpp11ObjectVisitor::Cpp11ObjectVisitor(::IceUtilInternal::Output& h,
                                                   ::IceUtilInternal::Output& c,
                                                   const std::string& dllExport) :
    H(h),
    C(c),
    _dllExport(dllExport),
    _doneStaticSymbol(false),
    _useWstring(false)
{
}

void
Slice::Gen::Cpp11ObjectVisitor::emitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    H << sp << nl << typeToString(p->type(), p->optional(), p->getMetaData(), _useWstring, true) << ' ' << name;

    string defaultValue = p->defaultValue();
    if(!defaultValue.empty())
    {
        H << " = ";
        writeConstantValue(H, p->type(), p->defaultValueType(), defaultValue, _useWstring, p->getMetaData(), true);
    }
    H << ";";
}

void
Slice::Gen::Cpp11InterfaceVisitor::emitUpcall(const ClassDefPtr& base, const string& call)
{
    C << nl << (base ? fixKwd(base->scoped()) : string("::Ice::Object")) << call;
}

void
Slice::Gen::Cpp11ValueVisitor::emitUpcall(const ClassDefPtr& base, const string& call)
{
    C << nl << (base ? fixKwd(base->scoped()) : string("::Ice::Value")) << call;
}

Slice::Gen::Cpp11LocalObjectVisitor::Cpp11LocalObjectVisitor(::IceUtilInternal::Output& h,
                                                             ::IceUtilInternal::Output& c,
                                                             const std::string& dllExport) :
    Cpp11ObjectVisitor(h, c, dllExport)
{
}

bool
Slice::Gen::Cpp11LocalObjectVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasLocalClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    string name = fixKwd(p->name());
    H << sp << nl << "namespace " << name << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11LocalObjectVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';
    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11LocalObjectVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isLocal())
    {
        return false;
    }
    if(p->isDelegate())
    {
        return false;
    }

    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();

    H << sp << nl << "class " << _dllExport << name;
    H.useCurrentPosAsIndent();
    if(!bases.empty())
    {
        H << " : ";
        ClassList::const_iterator q = bases.begin();
        bool virtualInheritance = p->isInterface();
        while(q != bases.end())
        {
            if(virtualInheritance || (*q)->isInterface())
            {
                H << "virtual ";
            }

            H << "public " << fixKwd((*q)->scoped());
            if(++q != bases.end())
            {
                H << ',' << nl;
            }
        }
    }

    H.restoreIndent();
    H << sb;
    H.dec();
    H << nl << "public:" << sp;
    H.inc();

    //
    // In C++, a nested type cannot have the same name as the enclosing type
    //
    if(p->name() != "PointerType")
    {
        H << nl << "typedef ::std::shared_ptr<" << name << "> PointerType;";
    }

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
        allTypes.push_back(typeName);
        allParamDecls.push_back(typeName + " __ice_" + (*q)->name());
    }

    if(!p->isInterface())
    {
        if(p->hasDefaultValues())
        {
            H << sp << nl << name << "() :";
            H.inc();
            writeDataMemberInitializers(H, dataMembers, _useWstring, true);
            H.dec();
            H << sb;
            H << eb;
        }
        else
        {
            H << sp << nl << name << "()";
            H << sb << eb;
        }

        emitOneShotConstructor(p);
        H << sp;
    }

    if(p->hasMetaData("cpp:comparable"))
    {
        H << sp;
        H << nl << "virtual bool operator==(const " << p->name() << "&) const = 0;";
        H << nl << "virtual bool operator<(const " << p->name() << "&) const = 0;";
    }
    return true;
}

void
Slice::Gen::Cpp11LocalObjectVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }

    //
    // Emit data members. Access visibility may be specified by metadata.
    //
    bool inProtected = true;
    DataMemberList dataMembers = p->dataMembers();
    bool prot = p->hasMetaData("protected");
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        if(prot || (*q)->hasMetaData("protected"))
        {
            if(!inProtected)
            {
                H.dec();
                H << sp << nl << "protected:";
                H.inc();
                inProtected = true;
            }
        }
        else
        {
            if(inProtected)
            {
                H.dec();
                H << sp << nl << "public:";
                H.inc();
                inProtected = false;
            }
        }

        emitDataMember(*q);
    }

    if(!p->isAbstract())
    {
        if(inProtected)
        {
            H.dec();
            H << sp << nl << "public:";
            H.inc();
            inProtected = false;
        }
        H << sp << nl << "virtual ~" << fixKwd(p->name()) << "() = default;";
    }

    H << eb << ';';
}

bool
Slice::Gen::Cpp11LocalObjectVisitor::visitExceptionStart(const ExceptionPtr&)
{
    return false;
}

bool
Slice::Gen::Cpp11LocalObjectVisitor::visitStructStart(const StructPtr&)
{
    return false;
}

void
Slice::Gen::Cpp11LocalObjectVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    int typeCtx = _useWstring | TypeContextLocal;
    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret, p->returnIsOptional(), p->getMetaData(), typeCtx, true);

    string params = "(";
    string paramsDecl = "(";
    string args = "(";

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    string classScope = fixKwd(cl->scope());

    ParamDeclList inParams;
    ParamDeclList outParams;
    ParamDeclList paramList = p->parameters();
    vector< string> outDecls;
    for(ParamDeclList::iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd(string(paramPrefix) + (*q)->name());
        TypePtr type = (*q)->type();
        bool isOutParam = (*q)->isOutParam();
        string typeString;
        if(isOutParam)
        {
            outParams.push_back(*q);
            typeString = outputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), typeCtx, true);
        }
        else
        {
            inParams.push_back(*q);
            typeString = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), typeCtx, true);
        }

        if(q != paramList.begin())
        {
            params += ", ";
            paramsDecl += ", ";
            args += ", ";
        }

        params += typeString;
        paramsDecl += typeString;
        paramsDecl += ' ';
        paramsDecl += paramName;
        args += paramName;

        if(isOutParam)
        {
            outDecls.push_back(typeString);
        }
    }

    params += ')';
    paramsDecl += ')';
    args += ')';

    string isConst = ((p->mode() == Operation::Nonmutating) || p->hasMetaData("cpp:const")) ? " const" : "";

    string deprecateSymbol = getDeprecateSymbol(p, cl);

    H << sp;
    H << nl << deprecateSymbol << "virtual " << retS << ' ' << fixKwd(name) << params << isConst << " = 0;";

    if(cl->hasMetaData("async-oneway") || p->hasMetaData("async-oneway"))
    {
        vector<string> paramsDeclAMI;
        vector<string> outParamsDeclAMI;

        ParamDeclList paramList = p->parameters();
        for(ParamDeclList::const_iterator r = paramList.begin(); r != paramList.end(); ++r)
        {
            string paramName = fixKwd((*r)->name());

            StringList metaData = (*r)->getMetaData();
            string typeString;
            if(!(*r)->isOutParam())
            {
                typeString = inputTypeToString((*r)->type(), (*r)->optional(), metaData, typeCtx, true);
                paramsDeclAMI.push_back(typeString + ' ' + paramName);
            }
        }

        H << sp;
        H << nl << "virtual ::std::function<void ()>";
        H << nl << name << "_async(";
        H.useCurrentPosAsIndent();
        for(vector<string>::const_iterator i = paramsDeclAMI.begin(); i != paramsDeclAMI.end(); ++i)
        {
            H << *i << ",";
        }
        if(!paramsDeclAMI.empty())
        {
            H << nl;
        }
        H << "::std::function<void (::std::exception_ptr)> exception,";
        H << nl << "::std::function<void (bool)> sent = nullptr) = 0;";
        H.restoreIndent();

        H << sp;
        H << nl << "template<template<typename> class P = ::std::promise>";
        H << nl << deprecateSymbol << "auto " << name << "_async" << spar << paramsDeclAMI << epar;
        H.inc();
        H << nl << "-> decltype(::std::declval<P<bool>>().get_future())";
        H.dec();
        H << sb;
        H << nl << "using Promise = P<bool>;";
        H << nl << "auto __promise = ::std::make_shared<Promise>();";

        H << nl << name << "_async(";
        H.useCurrentPosAsIndent();
        for(vector<string>::const_iterator i = paramsDeclAMI.begin(); i != paramsDeclAMI.end(); ++i)
        {
            H << *i << ",";
        }
        if(!paramsDeclAMI.empty())
        {
            H << nl;
        }
        H << "[__promise](::std::exception_ptr __ex)";
        H << sb;
        H << nl << "__promise->set_exception(::std::move(__ex));";
        H << eb << ",";
        H << nl << "[__promise](bool __b)";
        H << sb;
        H << nl << "__promise->set_value(__b);";
        H << eb << ");";
        H.restoreIndent();

        H << nl << "return __promise->get_future();";
        H << eb;
    }
}

Slice::Gen::Cpp11InterfaceVisitor::Cpp11InterfaceVisitor(::IceUtilInternal::Output& h,
                                                         ::IceUtilInternal::Output& c,
                                                         const std::string& dllExport) :
    Cpp11ObjectVisitor(h, c, dllExport)
{
}

bool
Slice::Gen::Cpp11InterfaceVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalInterfaceDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    string name = fixKwd(p->name());
    H << sp << nl << "namespace " << name << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11InterfaceVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11InterfaceVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal() || (!p->isInterface() && p->allOperations().empty()))
    {
        return false;
    }

    string suffix = p->isInterface() ? "" : "Disp";

    string name = fixKwd(p->name() + suffix);
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scope() + p->name() + suffix);
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();

    H << sp << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    if(bases.empty() || (base && base->allOperations().empty()))
    {
        H << "public virtual ::Ice::Object";
    }
    else
    {
        ClassList::const_iterator q = bases.begin();
        while(q != bases.end())
        {
            string baseSuffix = (*q)->isInterface() ? "" : "Disp";
            string baseScoped = fixKwd((*q)->scope() + (*q)->name() + baseSuffix);

            H << "public virtual " << baseScoped;
            if(++q != bases.end())
            {
                H << ',' << nl;
            }
        }
    }

    H.restoreIndent();
    H << sb;
    H.dec();
    H << nl << "public:" << sp;
    H.inc();

    //
    // In C++, a nested type cannot have the same name as the enclosing type
    //
    if(name != "ProxyType")
    {
        H << nl << "typedef " << p->name() << "Prx ProxyType;";
    }

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;

    ClassList allBases = p->allBases();
    StringList ids;
    transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun(&Contained::scoped));
    StringList other;
    other.push_back(p->scoped());
    other.push_back("::Ice::Object");
    other.sort();
    ids.merge(other);
    ids.unique();
    StringList::const_iterator firstIter = ids.begin();
    StringList::const_iterator scopedIter = find(ids.begin(), ids.end(), p->scoped());
    assert(scopedIter != ids.end());
    StringList::difference_type scopedPos = IceUtilInternal::distance(firstIter, scopedIter);

    H << sp;
    H << nl << "virtual bool ice_isA(::std::string, const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
    H << nl << "virtual ::std::vector< ::std::string> ice_ids(const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
    H << nl << "virtual const ::std::string& ice_id(const ::Ice::Current& = ::Ice::noExplicitCurrent) const;";
    H << nl << "static const ::std::string& ice_staticId();";

    string flatName = p->flattenedScope() + p->name() + "_ids";

    C << sp;
    C << nl << "bool" << nl << scoped.substr(2) << "::ice_isA(::std::string _s, const ::Ice::Current&) const";
    C << sb;
    C << nl << "return ::std::binary_search(" << flatName << ", " << flatName << " + " << ids.size() << ", _s);";
    C << eb;

    C << sp;
    C << nl << "::std::vector< ::std::string>" << nl << scoped.substr(2) << "::ice_ids(const ::Ice::Current&) const";
    C << sb;
    C << nl << "return ::std::vector< ::std::string>(&" << flatName << "[0], &" << flatName << '[' << ids.size() << "]);";
    C << eb;

    C << sp;
    C << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_id(const ::Ice::Current&) const";
    C << sb;
    C << nl << "return " << flatName << '[' << scopedPos << "];";
    C << eb;

    C << sp;
    C << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_staticId()";
    C << sb;
    C << nl << "return " << flatName << '[' << scopedPos << "];";
    C << eb;
    return true;
}

void
Slice::Gen::Cpp11InterfaceVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string suffix = p->isInterface() ? "" : "Disp";
    string scoped = fixKwd(p->scope() + p->name() + suffix);

    string scope = fixKwd(p->scope());
    string name = fixKwd(p->name() + suffix);
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }

    OperationList allOps = p->allOperations();
    if(!allOps.empty())
    {
        StringList allOpNames;
        transform(allOps.begin(), allOps.end(), back_inserter(allOpNames), ::IceUtil::constMemFun(&Contained::name));
        allOpNames.push_back("ice_id");
        allOpNames.push_back("ice_ids");
        allOpNames.push_back("ice_isA");
        allOpNames.push_back("ice_ping");
        allOpNames.sort();
        allOpNames.unique();

        string flatName = p->flattenedScope() + p->name() + "_ops";

        H << sp;
        H << nl << "virtual ::Ice::DispatchStatus __dispatch(::IceInternal::Incoming&, const ::Ice::Current&);";

        C << sp;
        C << nl << "::Ice::DispatchStatus" << nl << scoped.substr(2)
          << "::__dispatch(::IceInternal::Incoming& in, const ::Ice::Current& c)";
        C << sb;

        C << nl << "::std::pair< const ::std::string*, const ::std::string*> r = "
          << "::std::equal_range(" << flatName << ", " << flatName << " + " << allOpNames.size() << ", c.operation);";
        C << nl << "if(r.first == r.second)";
        C << sb;
        C << nl << "throw ::Ice::OperationNotExistException(__FILE__, __LINE__, c.id, c.facet, c.operation);";
        C << eb;
        C << sp;
        C << nl << "switch(r.first - " << flatName << ')';
        C << sb;
        int i = 0;
        for(StringList::const_iterator q = allOpNames.begin(); q != allOpNames.end(); ++q)
        {
            C << nl << "case " << i++ << ':';
            C << sb;
            C << nl << "return ___" << *q << "(in, c);";
            C << eb;
        }
        C << eb;
        C << sp;
        C << nl << "assert(false);";
        C << nl << "throw ::Ice::OperationNotExistException(__FILE__, __LINE__, c.id, c.facet, c.operation);";
        C << eb;
    }

    H << eb << ';';
}

bool
Slice::Gen::Cpp11InterfaceVisitor::visitExceptionStart(const ExceptionPtr&)
{
    return false;
}

bool
Slice::Gen::Cpp11InterfaceVisitor::visitStructStart(const StructPtr&)
{
    return false;
}

void
Slice::Gen::Cpp11InterfaceVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();

    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring, true);

    string params = "(";
    string paramsDecl = "(";
    string args = "(";

    string paramsAMD;
    string argsAMD;

    string responseParams;
    string responseParamsDecl;

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    string classScope = fixKwd(cl->scope());

    string suffix = cl->isInterface() ? "" : "Disp";
    string scope = fixKwd(cl->scope() + cl->name() + suffix + "::");
    string scoped = fixKwd(cl->scope() + cl->name() + suffix + "::" + p->name());

    ParamDeclList inParams;
    ParamDeclList outParams;
    ParamDeclList paramList = p->parameters();
    vector< string> outDecls;
    for(ParamDeclList::iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
        string paramName = fixKwd(string(paramPrefix) + (*q)->name());
        TypePtr type = (*q)->type();
        bool isOutParam = (*q)->isOutParam();
        string typeString;
        if(isOutParam)
        {
            outParams.push_back(*q);
            typeString = outputTypeToString(type, (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
            outDecls.push_back(inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true));
        }
        else
        {
            inParams.push_back(*q);
            typeString = typeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
        }

        if(q != paramList.begin())
        {
            params += ", ";
            paramsDecl += ", ";
            args += ", ";
        }

        params += typeString;
        paramsDecl += typeString;
        paramsDecl += ' ';
        paramsDecl += paramName;
        args += (isMovable(type) && !isOutParam) ? ("::std::move(" + paramName + ")") : paramName;
    }

    if(!paramList.empty())
    {
        params += ", ";
        paramsDecl += ", ";
        args += ", ";
    }

    params += "const ::Ice::Current& = ::Ice::noExplicitCurrent)";
    paramsDecl += "const ::Ice::Current& __current)";
    args += "__current)";

    for(ParamDeclList::iterator q = inParams.begin(); q != inParams.end(); ++q)
    {
        if(q != inParams.begin())
        {
            paramsAMD += ", ";
            argsAMD += ", ";
        }
        paramsAMD += typeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
        if(isMovable((*q)->type()))
        {
            argsAMD += "::std::move(" + fixKwd(string(paramPrefix) + (*q)->name()) + ")";
        }
        else
        {
            argsAMD += fixKwd(string(paramPrefix) + (*q)->name());
        }
    }

    if(ret)
    {
        string typeString = inputTypeToString(ret, p->returnIsOptional(), p->getMetaData(), _useWstring, true);
        responseParams = typeString;
        responseParamsDecl = typeString + " __ret";
        if(!outParams.empty())
        {
            responseParams += ", ";
            responseParamsDecl += ", ";
        }
    }

    for(ParamDeclList::iterator q = outParams.begin(); q != outParams.end(); ++q)
    {
        if(q != outParams.begin())
        {
            responseParams += ", ";
            responseParamsDecl += ", ";
        }
        string typeString = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
        responseParams += typeString;
        responseParamsDecl += typeString + " " + fixKwd(string(paramPrefix) + (*q)->name());
    }

    string isConst = ((p->mode() == Operation::Nonmutating) || p->hasMetaData("cpp:const")) ? " const" : "";
    bool amd = (cl->hasMetaData("amd") || p->hasMetaData("amd"));

    ExceptionList throws = p->throws();
    throws.sort();
    throws.unique();

    //
    // Arrange exceptions into most-derived to least-derived order. If we don't
    // do this, a base exception handler can appear before a derived exception
    // handler, causing compiler warnings and resulting in the base exception
    // being marshaled instead of the derived exception.
    //
#if defined(__SUNPRO_CC)
    throws.sort(derivedToBaseCompare);
#else
    throws.sort(Slice::DerivedToBaseCompare());
#endif

    string deprecateSymbol = getDeprecateSymbol(p, cl);
    H << sp;
    if(!amd)
    {
        H << nl << deprecateSymbol << "virtual " << retS << ' ' << fixKwd(name) << params << isConst << " = 0;";
    }
    else
    {
        H << nl << deprecateSymbol << "virtual void " << name << "_async(";
        H.useCurrentPosAsIndent();
        H << paramsAMD;
        if(!paramsAMD.empty())
        {
            H << "," << nl;
        }
        H << "::std::function<void (" << responseParams << ")>," << nl
          << "::std::function<void (::std::exception_ptr)>, const Ice::Current&)" << isConst << " = 0;";
        H.restoreIndent();
    }

    H << sp;
    H << nl << "::Ice::DispatchStatus ___" << name << "(::IceInternal::Incoming&, const ::Ice::Current&)" << isConst << ';';

    C << sp;
    C << nl << "::Ice::DispatchStatus" << nl << scope.substr(2) << "___" << name << "(::IceInternal::Incoming& __inS"
      << ", const ::Ice::Current& __current)" << isConst;
    C << sb;
    C << nl << "__checkMode(" << operationModeToString(p->mode(), true) << ", __current.mode);";

    if(!inParams.empty())
    {
        C << nl << "::Ice::InputStream* __is = __inS.startReadParams();";
        writeAllocateCode(C, inParams, 0, true, _useWstring | TypeContextInParam, true);
        writeUnmarshalCode(C, inParams, 0, true, TypeContextInParam);
        if(p->sendsClasses(false))
        {
            C << nl << "__is->readPendingObjects();";
        }
        C << nl << "__inS.endReadParams();";
    }
    else
    {
        C << nl << "__inS.readEmptyParams();";
    }

    if(!amd)
    {
        writeAllocateCode(C, outParams, 0, true, _useWstring, true);
        if(!throws.empty())
        {
            C << nl << "try";
            C << sb;
        }
        C << nl;
        if(ret)
        {
            C << retS << " __ret = ";
        }
        C << fixKwd(name) << args << ';';
        if(ret || !outParams.empty())
        {
            C << nl << "auto __os = __inS.__startWriteParams(" << opFormatTypeToString(p) << ");";
            writeMarshalCode(C, outParams, p, true);
            if(p->returnsClasses(false))
            {
                C << nl << "__os->writePendingObjects();";
            }
            C << nl << "__inS.__endWriteParams(true);";
        }
        else
        {
            C << nl << "__inS.__writeEmptyParams();";
        }
        C << nl << "return ::Ice::DispatchOK;";
        if(!throws.empty())
        {
            C << eb;
            ExceptionList::const_iterator r;
            for(r = throws.begin(); r != throws.end(); ++r)
            {
                C << nl << "catch(const " << fixKwd((*r)->scoped()) << "& __ex)";
                C << sb;
                C << nl << "__inS.__writeUserException(__ex, " << opFormatTypeToString(p) << ");";
                C << eb;
            }
            C << nl << "return ::Ice::DispatchUserException;";
        }
    }
    else
    {
        C << nl << "auto inS = ::IceInternal::IncomingAsync::create(__inS);";
        C << nl << "auto __exception = [inS](::std::exception_ptr e)";
        C << sb;
        C << nl << "try";
        C << sb;
        C << nl << "std::rethrow_exception(e);";
        C << eb;

        for(ExceptionList::const_iterator r = throws.begin(); r != throws.end(); ++r)
        {
            C << nl << "catch(const " << fixKwd((*r)->scoped()) << "& __ex)";
            C << sb;
            C << nl <<"if(inS->__validateResponse(false))";
            C << sb;
            C << nl << "inS->__writeUserException(__ex, " << opFormatTypeToString(p) << ");";
            C << nl << "inS->__response();";
            C << eb;
            C << eb;
        }

        C << nl << "catch(const ::std::exception& __ex)";
        C << sb;
        C << nl << "inS->ice_exception(__ex);";
        C << eb;
        C << nl << "catch(...)";
        C << sb;
        C << nl << "inS->ice_exception();";
        C << eb;
        C << eb << ";";

        C << nl << "try";
        C << sb;

        C << nl << name << "_async(";
        C.useCurrentPosAsIndent();
        if(!argsAMD.empty())
        {
            C << argsAMD << ",";
            C << nl;
        }
        C << "[inS](" << responseParamsDecl << ")";
        C << sb;
        C << nl << "if(inS->__validateResponse(true))";
        C << sb;
        if(ret || !outParams.empty())
        {
            C << nl << "auto __os = inS->__startWriteParams(" << opFormatTypeToString(p) << ");";
            writeMarshalCode(C, outParams, p, true);
            if(p->returnsClasses(false))
            {
                C << nl << "__os->writePendingObjects();";
            }
            C << nl << "inS->__endWriteParams(true);";
        }
        else
        {
            C << nl << "inS->__writeEmptyParams();";
        }
        C << nl << "inS->__response();";
        C << eb;
        C << eb << ",";
        C << nl << "__exception, __current);";
        C.restoreIndent();
        C << eb;
        C << nl << "catch(...)";
        C << sb;
        C << nl << "__exception(std::current_exception());";
        C << eb;
        C << nl << "return ::Ice::DispatchAsync;";
    }
    C << eb;
}

Slice::Gen::Cpp11ValueVisitor::Cpp11ValueVisitor(::IceUtilInternal::Output& h,
                                                 ::IceUtilInternal::Output& c,
                                                 const std::string& dllExport) :
    Cpp11ObjectVisitor(h, c, dllExport)
{
}

bool
Slice::Gen::Cpp11ValueVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasValueDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);
    string name = fixKwd(p->name());
    H << sp << nl << "namespace " << name << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11ValueVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11ValueVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal() || p->isInterface())
    {
        return false;
    }
    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty())
    {
        base = bases.front();
    }
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList allDataMembers = p->allDataMembers();

    H << sp << nl << "class " << _dllExport << name << " : public ::Ice::ValueHelper<" << name << ", ";

    if(!base || (base && base->isInterface()))
    {
        H << "Ice::Value";
    }
    else
    {
        H << fixKwd(base->scoped());
    }
    H << ">";
    H << sb;
    H.dec();
    H << nl << "public:" << sp;
    H.inc();

    //
    // In C++, a nested type cannot have the same name as the enclosing type
    //

    if(p->name() != "PointerType")
    {
        H << nl << "typedef ::std::shared_ptr<" << name << "> PointerType;";
    }

    vector<string> params;
    vector<string> allTypes;
    vector<string> allParamDecls;

    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        params.push_back(fixKwd((*q)->name()));
    }

    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring);
        allTypes.push_back(typeName);
        allParamDecls.push_back(typeName + " __ice_" + (*q)->name());
    }

    H << sp << nl << name << "() = default;";

    emitOneShotConstructor(p);
    H << sp;
    H << nl << "static const ::std::string& ice_staticId();";
    return true;
}

void
Slice::Gen::Cpp11ValueVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());
    string name = fixKwd(p->name());
    string typeId = p->flattenedScope() + p->name() + "_init.typeId";
    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }
    bool basePreserved = p->inheritsMetaData("preserve-slice");
    bool preserved = p->hasMetaData("preserve-slice");

    if(preserved && !basePreserved)
    {
        H << sp;
        H << nl << "virtual void __write(::Ice::OutputStream*) const;";
        H << nl << "virtual void __read(::Ice::InputStream*);";
    }

    H.dec();
    H << sp << nl << "protected:";
    H.inc();
    H << sp;
    H << nl << "virtual void __writeImpl(::Ice::OutputStream*) const;";
    H << nl << "virtual void __readImpl(::Ice::InputStream*);";

    if(preserved && !basePreserved)
    {
        C << sp;
        C << nl << "void" << nl << scoped.substr(2) << "::__write(::Ice::OutputStream* __os) const";
        C << sb;
        C << nl << "__os->startObject(__slicedData);";
        C << nl << "__writeImpl(__os);";
        C << nl << "__os->endObject();";
        C << eb;

        C << sp;
        C << nl << "void" << nl << scoped.substr(2) << "::__read(::Ice::InputStream* __is)";
        C << sb;
        C << nl << "__is->startObject();";
        C << nl << "__readImpl(__is);";
        C << nl << "__slicedData = __is->endObject(true);";
        C << eb;
    }

    C << sp;
    C << nl << "void" << nl << scoped.substr(2) << "::__writeImpl(::Ice::OutputStream* __os) const";
    C << sb;
    C << nl << "__os->startSlice(" << typeId << ", " << p->compactId() << (!base ? ", true" : ", false") << ");";
    writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), true);
    C << nl << "__os->endSlice();";
    if(base)
    {
        emitUpcall(base, "::__writeImpl(__os);");
    }
    C << eb;

    C << sp;
    C << nl << "void" << nl << scoped.substr(2) << "::__readImpl(::Ice::InputStream* __is)";
    C << sb;
    C << nl << "__is->startSlice();";
    writeMarshalUnmarshalDataMembers(C, p->dataMembers(), p->orderedOptionalDataMembers(), false);
    C << nl << "__is->endSlice();";
    if(base)
    {
        emitUpcall(base, "::__readImpl(__is);");
    }
    C << eb;

    C << sp;
    C << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_staticId()";
    C << sb;
    C << nl << "return " << typeId << ";";
    C << eb;

    //
    // Emit data members. Access visibility may be specified by metadata.
    //
    bool inProtected = true;
    DataMemberList dataMembers = p->dataMembers();
    bool prot = p->hasMetaData("protected");
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        if(prot || (*q)->hasMetaData("protected"))
        {
            if(!inProtected)
            {
                H.dec();
                H << sp << nl << "protected:";
                H.inc();
                inProtected = true;
            }
        }
        else
        {
            if(inProtected)
            {
                H.dec();
                H << sp << nl << "public:";
                H.inc();
                inProtected = false;
            }
        }

        emitDataMember(*q);
    }

    if(preserved && !basePreserved)
    {
        if(!inProtected)
        {
            H.dec();
            H << sp << nl << "protected:";
            H.inc();
            inProtected = true;
        }
        H << sp << nl << "::std::shared_ptr<::Ice::SlicedData> __slicedData;";
    }

    H << eb << ';';

    if(!_doneStaticSymbol)
    {
        //
        // We need an instance here to trigger initialization if the implementation is in a static library.
        // But we do this only once per source file, because a single instance is sufficient to initialize
        // all of the globals in a compilation unit.
        //
        _doneStaticSymbol = true;
        H << sp << nl << "static " << fixKwd(p->name()) << " _" << p->name() << "_init;";
    }

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11ValueVisitor::visitExceptionStart(const ExceptionPtr&)
{
    return false;
}

bool
Slice::Gen::Cpp11ValueVisitor::visitStructStart(const StructPtr&)
{
    return false;
}

void
Slice::Gen::Cpp11ValueVisitor::visitOperation(const OperationPtr&)
{
}


bool
Slice::Gen::Cpp11ObjectVisitor::emitVirtualBaseInitializers(const ClassDefPtr& derived, const ClassDefPtr& base)
{
    DataMemberList allDataMembers = base->allDataMembers();
    if(allDataMembers.empty())
    {
        return false;
    }

    string upcall = "(";
    for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
    {
        if(q != allDataMembers.begin())
        {
            upcall += ", ";
        }
        if(isMovable((*q)->type()))
        {
            upcall += "::std::move(__ice_" + (*q)->name() + ")";
        }
        else
        {
            upcall += "__ice_" + (*q)->name();
        }
    }
    upcall += ")";

    if(base->isLocal())
    {
        H << nl << fixKwd(base->scoped());
    }
    else
    {
        H << nl << "Ice::ValueHelper<" << fixKwd(derived->scoped()) << ", " << fixKwd(base->scoped()) << ">";
    }
    H << upcall;
    return true;
}

void
Slice::Gen::Cpp11ObjectVisitor::emitOneShotConstructor(const ClassDefPtr& p)
{
    DataMemberList allDataMembers = p->allDataMembers();

    if(!allDataMembers.empty())
    {
        vector<string> allParamDecls;
        DataMemberList dataMembers = p->dataMembers();

        for(DataMemberList::const_iterator q = allDataMembers.begin(); q != allDataMembers.end(); ++q)
        {
            string typeName = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
            allParamDecls.push_back(typeName + " __ice_" + (*q)->name());
        }

        H << sp << nl;
        if(allParamDecls.size() == 1)
        {
            H << "explicit ";
        }
        H << fixKwd(p->name()) << spar << allParamDecls << epar << " :";
        H.inc();

        ClassList bases = p->bases();

        if(!bases.empty() && !bases.front()->isInterface())
        {
            if(emitVirtualBaseInitializers(p, bases.front()))
            {
                if(!dataMembers.empty())
                {
                    H << ',';
                }
            }
        }

        if(!dataMembers.empty())
        {
            H << nl;
        }

        for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
        {
            if(q != dataMembers.begin())
            {
                H << ',' << nl;
            }
            string memberName = fixKwd((*q)->name());
            if(isMovable((*q)->type()))
            {
                H << memberName << "(::std::move(" << "__ice_" << (*q)->name() << "))";
            }
            else
            {
                H << memberName << '(' << "__ice_" << (*q)->name() << ')';
            }
        }

        H.dec();
        H << sb;
        H << eb;
    }
}

Slice::Gen::Cpp11StreamVisitor::Cpp11StreamVisitor(Output& h, Output& c, const string& dllExport) :
    H(h),
    C(c),
    _dllExport(dllExport)
{
}

bool
Slice::Gen::Cpp11StreamVisitor::visitModuleStart(const ModulePtr& m)
{
    if(!m->hasNonLocalContained(Contained::ContainedTypeStruct) &&
       !m->hasNonLocalContained(Contained::ContainedTypeEnum))
    {
        return false;
    }

    if(UnitPtr::dynamicCast(m->container()))
    {
        //
        // Only emit this for the top-level module.
        //
        H << sp;
        H << nl << "namespace Ice" << nl << '{';

        if(m->hasNonLocalContained(Contained::ContainedTypeStruct))
        {
            C << sp;
            C << nl << "namespace Ice" << nl << '{';
        }
    }
    return true;
}

void
Slice::Gen::Cpp11StreamVisitor::visitModuleEnd(const ModulePtr& m)
{
    if(UnitPtr::dynamicCast(m->container()))
    {
        //
        // Only emit this for the top-level module.
        //
        H << nl << '}';
        if(m->hasNonLocalContained(Contained::ContainedTypeStruct))
        {
            C << nl << '}';
        }
    }
}

bool
Slice::Gen::Cpp11StreamVisitor::visitStructStart(const StructPtr& p)
{
    if(p->isLocal())
    {
        return false;
    }

    string scoped = fixKwd(p->scoped());

    H << nl << "template<>";
    H << nl << "struct StreamableTraits< " << scoped << ">";
    H << sb;
    H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryStruct;";
    H << nl << "static const int minWireSize = " << p->minWireSize() << ";";
    H << nl << "static const bool fixedLength = " << (p->isVariableLength() ? "false" : "true") << ";";
    H << eb << ";" << nl;

    DataMemberList dataMembers = p->dataMembers();

    H << nl << "template<class S>";
    H << nl << "struct StreamWriter<" << scoped << ", S>";
    H << sb;
    H << nl << "static void write(S* __os, const " <<  scoped << "& v)";
    H << sb;
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        writeMarshalUnmarshalDataMemberInHolder(H, "v.", *q, true);
    }
    H << eb;
    H << eb << ";" << nl;

    H << nl << "template<class S>";
    H << nl << "struct StreamReader<" << scoped << ", S>";
    H << sb;
    H << nl << "static void read(S* __is, " << scoped << "& v)";
    H << sb;
    for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        writeMarshalUnmarshalDataMemberInHolder(H, "v.", *q, false);
    }
    H << eb;
    H << eb << ";" << nl;

    if(!_dllExport.empty())
    {
        //
        // We tell "importers" that the implementation exports these instantiations
        //
        H << nl << "#if defined(ICE_HAS_DECLSPEC_IMPORT_EXPORT) && !defined(";
        H << _dllExport.substr(0, _dllExport.size() - 1) + "_EXPORTS) && !defined(ICE_STATIC_LIBS)";
        H << nl << "template struct " << _dllExport << "StreamWriter< " << scoped << ", ::Ice::OutputStream>;";
        H << nl << "template struct " << _dllExport << "StreamReader< " << scoped << ", ::Ice::InputStream>;";
        H << nl << "#endif" << nl;

        //
        // The instantations:
        //
        C << nl << "#if defined(ICE_HAS_DECLSPEC_IMPORT_EXPORT) && !defined(ICE_STATIC_LIBS)";
        C << nl << "template struct " << _dllExport << "StreamWriter< " << scoped << ", ::Ice::OutputStream>;";
        C << nl << "template struct " << _dllExport << "StreamReader< " << scoped << ", ::Ice::InputStream>;";
        C << nl << "#endif";
    }
    return false;
}

void
Slice::Gen::Cpp11StreamVisitor::visitEnum(const EnumPtr& p)
{
    if(!p->isLocal())
    {
        string scoped = fixKwd(p->scoped());
        H << nl << "template<>";
        H << nl << "struct StreamableTraits< " << scoped << ">";
        H << sb;
        H << nl << "static const StreamHelperCategory helper = StreamHelperCategoryEnum;";
        H << nl << "static const int minValue = " << p->minValue() << ";";
        H << nl << "static const int maxValue = " << p->maxValue() << ";";
        H << nl << "static const int minWireSize = " << p->minWireSize() << ";";
        H << nl << "static const bool fixedLength = false;";
        H << eb << ";" << nl;
    }
}


Slice::Gen::Cpp11CompatibilityVisitor::Cpp11CompatibilityVisitor(Output& h, Output&, const string& dllExport) :
    H(h),
    _dllExport(dllExport)
{
}

bool
Slice::Gen::Cpp11CompatibilityVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDecls())
    {
        return false;
    }

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';
    return true;
}

void
Slice::Gen::Cpp11CompatibilityVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';
}

void
Slice::Gen::Cpp11CompatibilityVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    if(p->definition() && p->definition()->isDelegate())
    {
        return;
    }

    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());

    H << sp << nl << "typedef ::std::shared_ptr<" << name << "> " << p->name() << "Ptr;";

    if(!p->isLocal())
    {
        ClassDefPtr def = p->definition();
        if(p->isInterface() || (def && !def->allOperations().empty()))
        {
            H << nl << "class " << p->name() << "Prx;";
            H << nl << "typedef ::std::shared_ptr<" << p->name() << "Prx> " << p->name() << "PrxPtr;";
        }
    }
}

Slice::Gen::Cpp11ImplVisitor::Cpp11ImplVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport), _useWstring(false)
{
}

string
Slice::Gen::Cpp11ImplVisitor::defaultValue(const TypePtr& type, const StringList& metaData) const
{
    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(builtin)
    {
        switch(builtin->kind())
        {
            case Builtin::KindBool:
            {
                return "false";
            }
            case Builtin::KindByte:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            {
                return "0";
            }
            case Builtin::KindFloat:
            case Builtin::KindDouble:
            {
                return "0.0";
            }
            case Builtin::KindString:
            {
                return "::std::string()";
            }
            case Builtin::KindValue:
            case Builtin::KindObject:
            case Builtin::KindObjectProxy:
            case Builtin::KindLocalObject:
            {
                return "nullptr";
            }
        }
    }
    else
    {
        ProxyPtr prx = ProxyPtr::dynamicCast(type);

        if(ProxyPtr::dynamicCast(type) || ClassDeclPtr::dynamicCast(type))
        {
            return "nullptr";
        }

        StructPtr st = StructPtr::dynamicCast(type);
        if(st)
        {
           return fixKwd(st->scoped()) + "()";
        }

        EnumPtr en = EnumPtr::dynamicCast(type);
        if(en)
        {
            EnumeratorList enumerators = en->getEnumerators();
            return fixKwd(en->scoped() + "::" + enumerators.front()->name());
        }

        SequencePtr seq = SequencePtr::dynamicCast(type);
        if(seq)
        {
            return typeToString(seq, metaData, _useWstring, true) + "()";
        }

        DictionaryPtr dict = DictionaryPtr::dynamicCast(type);
        if(dict)
        {
            return fixKwd(dict->scoped()) + "()";
        }
    }

    assert(false);
    return "???";
}

void
Slice::Gen::Cpp11ImplVisitor::writeReturn(Output& out, const TypePtr& type, const StringList& metaData)
{
    out << nl << "return " << defaultValue(type, metaData) << ";";
}

bool
Slice::Gen::Cpp11ImplVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = fixKwd(p->name());

    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::Cpp11ImplVisitor::visitModuleEnd(const ModulePtr&)
{
    H << sp;
    H << nl << '}';

    _useWstring = resetUseWstring(_useWstringHist);
}

bool
Slice::Gen::Cpp11ImplVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }

    _useWstring = setUseWstring(p, _useWstringHist, _useWstring);

    string name = p->name();
    string scope = fixKwd(p->scope());
    string cls = scope.substr(2) + name + "I";
    ClassList bases = p->bases();

    H << sp;
    H << nl << "class " << name << "I : ";
    H.useCurrentPosAsIndent();
    H << "public virtual ";

    if(p->isInterface() || p->isLocal())
    {
        H << fixKwd(name);
    }
    else
    {
        H << fixKwd(name + "Disp");
    }
    H.restoreIndent();

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    OperationList ops = p->allOperations();

    for(OperationList::const_iterator r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = (*r);
        string opName = op->name();

        TypePtr ret = op->returnType();
        string retS = returnTypeToString(ret, op->returnIsOptional(), op->getMetaData(), _useWstring, true);

        ParamDeclList params = op->parameters();
        ParamDeclList outParams;
        ParamDeclList inParams;
        for(ParamDeclList::const_iterator q = params.begin(); q != params.end(); ++q)
        {
            if((*q)->isOutParam())
            {
                outParams.push_back(*q);
            }
            else
            {
                inParams.push_back(*q);
            }
        }

        if(!p->isLocal() && (p->hasMetaData("amd") || op->hasMetaData("amd")))
        {
            string responseParams;
            string responseParamsDecl;

            H << sp << nl << "virtual void " << opName << "_async(";
            H.useCurrentPosAsIndent();
            for(ParamDeclList::const_iterator q = inParams.begin(); q != inParams.end(); ++q)
            {
                H << typeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true) << "," << nl;
            }

            if(ret)
            {
                string typeS = inputTypeToString(ret, op->returnIsOptional(), op->getMetaData(), _useWstring, true);
                responseParams = typeS;
                responseParamsDecl = typeS + " __ret";
                if(!outParams.empty())
                {
                    responseParams += ", ";
                    responseParamsDecl += ", ";
                }
            }

            for(ParamDeclList::iterator q = outParams.begin(); q != outParams.end(); ++q)
            {
                if(q != outParams.begin())
                {
                    responseParams += ", ";
                    responseParamsDecl += ", ";
                }
                string typeS = inputTypeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
                responseParams += typeS;
                responseParamsDecl += typeS + " " + fixKwd(string(paramPrefix) + (*q)->name());
            }

            string isConst = ((op->mode() == Operation::Nonmutating) || op->hasMetaData("cpp:const")) ? " const" : "";

            H << "std::function<void (" << responseParams << ")>,";
            H << nl << "std::function<void (std::exception_ptr)>,";
            H << nl << "const Ice::Current&)" << isConst << ';';
            H.restoreIndent();

            C << sp << nl << "void" << nl << scope << name << "I::" << opName << "_async(";
            C.useCurrentPosAsIndent();
            for(ParamDeclList::const_iterator q = inParams.begin(); q != inParams.end(); ++q)
            {
                C << typeToString((*q)->type(), (*q)->optional(), (*q)->getMetaData(), _useWstring, true);
                C << ' ' << fixKwd((*q)->name()) << "," << nl;
            }

            C << "std::function<void (" << responseParams << ")> " << opName << "_response,";
            C << nl << "std::function<void (std::exception_ptr)>,";
            C << nl << "const Ice::Current& current)" << isConst;
            C.restoreIndent();
            C << sb;

            string result = "r";
            for(ParamDeclList::const_iterator q = params.begin(); q != params.end(); ++q)
            {
                if((*q)->name() == result)
                {
                    result = "_" + result;
                    break;
                }
            }

            C << nl << opName << "_response(";
            if(ret)
            {
                C << defaultValue(ret, op->getMetaData());
            }
            for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
            {
                if(ret || q != outParams.begin())
                {
                    C << ", ";
                }
                C << defaultValue((*q)->type(), op->getMetaData());
            }
            C << ");";

            C << eb;
        }
        else
        {
            H << sp << nl << "virtual " << retS << ' ' << fixKwd(opName) << '(';
            H.useCurrentPosAsIndent();
            ParamDeclList paramList = op->parameters();
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(q != paramList.begin())
                {
                    H << ',' << nl;
                }
                StringList metaData = (*q)->getMetaData();
                string typeString;
                if((*q)->isOutParam())
                {
                    typeString = outputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true);
                }
                else
                {
                    typeString = typeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true);
                }
                H << typeString;
            }
            if(!p->isLocal())
            {
                if(!paramList.empty())
                {
                    H << ',' << nl;
                }
                H << "const Ice::Current&";
            }
            H.restoreIndent();

            string isConst = ((op->mode() == Operation::Nonmutating) || op->hasMetaData("cpp:const")) ? " const" : "";

            H << ")" << isConst << ';';

            C << sp << nl << retS << nl;
            C << scope.substr(2) << name << "I::" << fixKwd(opName) << '(';
            C.useCurrentPosAsIndent();
            for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
            {
                if(q != paramList.begin())
                {
                    C << ',' << nl;
                }
                StringList metaData = (*q)->getMetaData();
                string typeString;
                if((*q)->isOutParam())
                {
                    C << outputTypeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true) << " "
                      << fixKwd((*q)->name());
                }
                else
                {
                    C << typeToString((*q)->type(), (*q)->optional(), metaData, _useWstring, true)<< " /*"
                      << fixKwd((*q)->name()) << "*/";
                }
            }
            if(!p->isLocal())
            {
                if(!paramList.empty())
                {
                    C << ',' << nl;
                }
                C << "const Ice::Current& current";
            }
            C.restoreIndent();
            C << ')';
            C << isConst;
            C << sb;

            for(ParamDeclList::const_iterator q = outParams.begin(); q != outParams.end(); ++q)
            {
                C << nl << fixKwd((*q)->name()) << " = " << defaultValue((*q)->type(), op->getMetaData()) << ";";
            }

            if(ret)
            {
                writeReturn(C, ret, op->getMetaData());
            }

            C << eb;
        }
    }

    H << eb << ';';

    _useWstring = resetUseWstring(_useWstringHist);

    return true;
}
