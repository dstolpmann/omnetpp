//==========================================================================
//  MSGCOMPILER.CC - part of
//
//                     OMNeT++/OMNEST
//            Discrete System Simulation in C++
//
//==========================================================================

/*--------------------------------------------------------------*
  Copyright (C) 2002-2017 Andras Varga
  Copyright (C) 2006-2017 OpenSim Ltd.

  This file is distributed WITHOUT ANY WARRANTY. See the file
  `license' for details on this and other legal matters.
*--------------------------------------------------------------*/

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "common/stringutil.h"
#include "common/stlutil.h"
#include "common/fileutil.h"
#include "msgcompiler.h"
#include "msgparser.h"
#include "msgdtdvalidator.h"
#include "exception.h"

using namespace omnetpp::common;

namespace omnetpp {
namespace nedxml {

const char *MsgCompiler::BUILTIN_DEFINITIONS =
        R"ENDMARK(
        @property[property](type=any; usage=file; desc="Property for declaring properties.");
        @property[customize](type=bool; usage=class; desc="If true: Customize the class via inheritance. Generates base class <name>_Base.");
        @property[str](type=string; usage=class; desc="Expression to be returned from the generated str() method.");
        @property[primitive](type=bool; usage=field,class; desc="Shortcut for @opaque @byValue @editable @subclassable(false) @supportsPtr(false).");
        @property[opaque](type=bool; usage=field,class; desc="If true: Treat the field as atomic (non-compound) type, i.e. having no descriptor class. When specified on a class, it determines the default for fields of that type.");
        @property[byValue](type=bool; usage=field,class; desc="If true: Causes the value to be passed by value (instead of by reference) in setters/getters. When specified on a class, it determines the default for fields of that type.");
        @property[supportsPtr](type=bool; usage=field,class; desc="Specifies whether this type supports creating a pointer (or pointer array) from it.");
        @property[subclassable](type=bool; usage=class; desc="Specifies whether this type can be subclassed (e.g. C++ primitive types and final classes cannot).");
        @property[defaultValue](type=string; usage=class; desc="Default value for fields of this type.");
        @property[cppType](type=string; usage=field,class; desc="Member C++ datatype. When specified on a class, it determines the default for fields of that type.");
        @property[argType](type=string; usage=field,class; desc="Field setter C++ argument type. When specified on a class, it determines the default for fields of that type.");
        @property[returnType](type=string; usage=field,class; desc="Field getter C++ return type. When specified on a class, it determines the default for fields of that type.");
        @property[fromString](type=string; usage=field,class; desc="Affects descriptor class: Code to convert string to field value. When specified on a class, it determines the default for fields of that type.");
        @property[toString](type=string; usage=field,class; desc="Affects descriptor class: Code to convert field value to string. When specified on a class, it determines the default for fields of that type.");
        @property[getterConversion](type=string; usage=field,class; desc="Code to convert field data type to return type in getters. When specified on a class, it determines the default for fields of that type.");
        @property[clone](type=string; usage=field,class; desc="For owned pointer fields: Code to duplicate (one array element of) the field value. When specified on a class, it determines the default for fields of that type.");
        @property[existingClass](type=bool; usage=class; desc="If true: This is a type is already defined in C++, i.e. it does not need to be generated.");
        @property[descriptor](type=string; usage=class; desc="A 'true'/'false' value specifies whether to generate descriptor class; special value 'readonly' requests generating a read-only descriptor.");
        @property[omitGetVerb](type=bool; usage=class; desc="If true: Drop the 'get' verb from the names of getter methods.");
        @property[fieldNameSuffix](type=string; usage=class; desc="Suffix to append to the names of data members.");
        @property[beforeChange](type=string; usage=class; desc="Method to be called before mutator code (in setters, non-const getters, operator=, etc.).");
        @property[implements](type=stringlist; usage=class; desc="Names of additional base classes.");
        @property[nopack](type=bool; usage=field; desc="If true: Ignore this field in parsimPack/parsimUnpack methods.");
        @property[editable](type=bool; usage=field,class; desc="Specifies whether field value (or value of fields that are instances of this type) can be set via the class descriptor's setFieldValueFromString() method.");
        @property[replaceable](type=bool; usage=field; desc="If true: Field is a pointer whose value can be set via the class descriptor's setFieldStructValuePointer() method.");
        @property[resizable](type=bool; usage=field; desc="If true: Field is a variable-size array whose size can be set via the class descriptor's setFieldArraySize() method.");
        @property[overrideGetter](type=bool; usage=field; desc="If true: Add the 'override' keyword to the declaration of the getter method.");
        @property[overrideSetter](type=bool; usage=field; desc="If true: Add the 'override' keyword to the declaration of the setter method.");
        @property[enum](type=string; usage=field; desc="For integer fields: Values are from the given enum.");
        @property[sizeType](type=string; usage=field; desc="C++ type to use for array sizes and indices.");
        @property[setter](type=string; usage=field; desc="Name of the setter method.");
        @property[getter](type=string; usage=field; desc="Name of the (const) getter method.");
        @property[getterForUpdate](type=string; usage=field; desc="Name of the non-const getter method.");
        @property[sizeSetter](type=string; usage=field; desc="Name of the method that sets size of dynamic array.");
        @property[sizeGetter](type=string; usage=field; desc="Name of the method that returns the array size.");
        @property[inserter](type=string; usage=field; desc="Name of the inserter method. (This method inserts element into a dynamic array.)");
        @property[eraser](type=string; usage=field; desc="Name of the eraser method. (This method erases element from a dynamic array.)");
        @property[allowReplace](type=bool; usage=field; desc="Specifies whether the setter method of an owned pointer field is allowed to delete the previously set object.");
        @property[actually](type=string; usage=class; internal=true; desc="Internal use.");
        @property[overwritePreviousDefinition](type=bool; usage=class; internal=true; desc="Internal use.");
        @property[owned](type=bool; usage=field; desc="For pointers and pointer arrays: Whether allocated memory is owned by the object (needs to be duplicated in dup(), and deleted in destructor). If field type is also a cOwnedObject, take()/drop() calls are also generated.");
        @property[custom](type=bool; usage=field; desc="If true: Do not generate any data or code for the field, only add it to the descriptor. Indicates that the field's implementation will be added to the class via targeted cplusplus blocks.");

        class __bool { @actually(bool); @primitive; @fromString(string2bool($)); @toString(bool2string($)); @defaultValue(false); }
        class __float { @actually(float); @primitive; @fromString(string2double($)); @toString(double2string($)); @defaultValue(0); }
        class __double { @actually(double); @primitive; @fromString(string2double($)); @toString(double2string($)); @defaultValue(0); }
        class __string { @actually(string); @primitive; @cppType(omnetpp::opp_string); @argType(const char *); @returnType(const char *); @getterConversion(.c_str()); @fromString(($)); @toString(oppstring2string($)); }
        class __char { @actually(char); @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class __short { @actually(short); @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class __int { @actually(int); @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class __long { @actually(long); @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class __uchar { @actually(unsigned char); @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class __ushort { @actually(unsigned short); @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class __uint { @actually(unsigned int); @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class __ulong { @actually(unsigned long); @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class int8_t { @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int16_t { @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int32_t { @primitive; @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int64_t { @primitive; @fromString(string2int64($)); @toString(int642string($)); @defaultValue(0); }
        class uint8_t { @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint16_t { @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint32_t { @primitive; @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint64_t { @primitive; @fromString(string2uint64($)); @toString(uint642string($)); @defaultValue(0); }
        class int8 { @primitive; @cppType(int8_t); @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int16 { @primitive; @cppType(int16_t); @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int32 { @primitive; @cppType(int32_t); @fromString(string2long($)); @toString(long2string($)); @defaultValue(0); }
        class int64 { @primitive; @cppType(int64_t); @fromString(string2int64($)); @toString(int642string($)); @defaultValue(0); }
        class uint8 { @primitive; @cppType(uint8_t); @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint16 { @primitive; @cppType(uint16_t); @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint32 { @primitive; @cppType(uint32_t); @fromString(string2ulong($)); @toString(ulong2string($)); @defaultValue(0); }
        class uint64 { @primitive; @cppType(uint64_t); @fromString(string2uint64($)); @toString(uint642string($)); @defaultValue(0); }
        )ENDMARK";

extern const char *SIM_STD_DEFINITIONS;  // contents of sim/sim_std.msg, stringified into sim_std_msg.cc

inline std::string str(const char *s)
{
    return s ? s : "";
}

static bool isQualified(const std::string& qname)
{
    return qname.find("::") != qname.npos;
}

template<class P, class T>
P check_and_cast(T *p)
{
    Assert(p);
    P ret = dynamic_cast<P>(p);
    Assert(ret);
    return ret;
}

MsgCompiler::MsgCompiler(const MsgCompilerOptions& opts, ErrorStore *errors) :
    opts(opts), analyzer(opts, &typeTable, errors), codegen(errors), errors(errors)
{
}

void MsgCompiler::generate(MsgFileElement *fileElement, const char *hFile, const char *ccFile, StringSet& outImportedFiles)
{
    if (used)
        throw opp_runtime_error("MsgCompiler is a one-shot object, make a new instance to compile another file");
    used = true;

    try {
        importBuiltinDefinitions();

        collectTypes(fileElement, false);

        codegen.openFiles(hFile, ccFile);
        generateCode(fileElement);
        codegen.closeFiles();
    }
    catch (std::exception& e) {
        errors->addError(fileElement, "Fatal: %s", e.what());
    }

    if (errors->containsError())
        codegen.deleteFiles();

    outImportedFiles = importedFiles;
}


void MsgCompiler::importBuiltinDefinitions()
{
    processBuiltinImport(BUILTIN_DEFINITIONS, "builtin-definitions");
    processBuiltinImport(SIM_STD_DEFINITIONS, "sim_std.msg");
}

void MsgCompiler::processBuiltinImport(const char *txt, const char *fname)
{
    MsgParser parser(errors);
    parser.setMsgNewSyntaxFlag(true);
    ASTNode *tree = parser.parseMsgText(txt, fname);
    if (errors->containsError()) {
        delete tree;
        return;
    }

    MsgDtdValidator dtdvalidator(errors);
    dtdvalidator.validate(tree);
    if (errors->containsError()) {
        delete tree;
        return;
    }

    typeTable.storeMsgFile(tree); // keep AST until we're done because ClassInfo/FieldInfo refer to it...

    // extract declarations
    MsgFileElement *fileElement = check_and_cast<MsgFileElement*>(tree);
    collectTypes(fileElement, true);
}

void MsgCompiler::collectTypes(MsgFileElement *fileElement, bool isImported)
{
    std::string currentDir = directoryOf(fileElement->getFilename());
    std::string currentNamespace = "";
    for (ASTNode *child = fileElement->getFirstChild(); child; child = child->getNextSibling()) {
        switch (child->getTagCode()) {
            case MSG_NAMESPACE: {
                NamespaceElement *nsElem = check_and_cast<NamespaceElement *>(child);
                currentNamespace = nsElem->getName();
                validateNamespaceName(currentNamespace, child); //TODO into syntax validator class!!!
                break;
            }

            case MSG_CPLUSPLUS:
                break;

            case MSG_IMPORT: {
                ImportElement *importElem = check_and_cast<ImportElement *>(child);
                std::string importName = importElem->getImportSpec();
                if (!currentNamespace.empty())
                    errors->addError(child, "misplaced import '%s': imports are not allowed within a namespace", importName.c_str()); //TODO into syntax validator class!!!
                if (!common::contains(importsSeen, importName)) {
                    importsSeen.insert(importName);
                    processImport(importElem, currentDir);
                }
                break;
            }

            case MSG_PROPERTY: {
                PropertyElement *propertyElem = check_and_cast<PropertyElement *>(child);
                Property property = analyzer.extractProperty(propertyElem);
                typeTable.addGlobalProperty(property);
                analyzer.validateFileProperty(property); // do not swap with previous
                break;
            }

            case MSG_STRUCT_DECL:
            case MSG_CLASS_DECL:
            case MSG_MESSAGE_DECL:
            case MSG_PACKET_DECL:
                errors->addError(child, "Type declarations are not needed with imports, try invoking the message compiler in legacy (4.x) mode using the --msg4 option");
                break;

            case MSG_ENUM_DECL: { // for enums already defined and registered in C++
                EnumDeclElement *enumDeclElem = check_and_cast<EnumDeclElement *>(child);
                EnumInfo enumInfo = analyzer.extractEnumDecl(enumDeclElem, currentNamespace);
                if (!typeTable.isEnumDefined(enumInfo.enumQName))
                    typeTable.addEnum(enumInfo);
                break;
            }

            case MSG_ENUM: {
                EnumElement *enumElem = check_and_cast<EnumElement *>(child);
                EnumInfo enumInfo = analyzer.extractEnumInfo(enumElem, currentNamespace);
                if (isQualified(enumInfo.enumName))
                    errors->addError(enumInfo.astNode, "type name may not be qualified: '%s'", enumInfo.enumName.c_str()); //TODO into some validator class
                if (typeTable.isEnumDefined(enumInfo.enumQName))
                    errors->addError(enumInfo.astNode, "attempt to redefine '%s'", enumInfo.enumName.c_str());
                typeTable.addEnum(enumInfo);
                ClassInfo classInfo = analyzer.extractClassInfoFromEnum(check_and_cast<EnumElement *>(child), currentNamespace, isImported);
                if (typeTable.isClassDefined(classInfo.qname))
                    errors->addError(classInfo.astNode, "attempt to redefine '%s'", classInfo.name.c_str());
                typeTable.addClass(classInfo);
                break;
            }

            case MSG_STRUCT:
            case MSG_CLASS:
            case MSG_MESSAGE:
            case MSG_PACKET: {
                ClassInfo classInfo = analyzer.extractClassInfo(child, currentNamespace, isImported);
                if (typeTable.isClassDefined(classInfo.qname) && !classInfo.props.contains(MsgAnalyzer::PROP_OVERWRITEPREVIOUSDEFINITION))
                    errors->addError(classInfo.astNode, "attempt to redefine '%s'", classInfo.name.c_str());
                typeTable.addClass(classInfo);
                break;
            }
        }
    }
}

void MsgCompiler::processImport(ImportElement *importElem, const std::string& currentDir)
{
    std::string importName = importElem->getImportSpec();
    std::string fileName = resolveImport(importName, currentDir);
    if (fileName == "") {
        errors->addError(importElem, "cannot resolve import '%s'", importName.c_str());
        return;
    }

    importedFiles.insert(fileName);

    MsgParser parser(errors);
    parser.setMsgNewSyntaxFlag(true);
    ASTNode *tree = parser.parseMsgFile(fileName.c_str());
    if (errors->containsError()) {
        delete tree;
        return;
    }

    typeTable.storeMsgFile(tree); // keep AST until we're done because ClassInfo/FieldInfo refer to it...

    // extract declarations
    MsgFileElement *fileElement = check_and_cast<MsgFileElement*>(tree);
    collectTypes(fileElement, true);
}

std::string MsgCompiler::resolveImport(const std::string& importName, const std::string& currentDir)
{
    std::string msgFile = opp_replacesubstring(importName, ".", "/", true) + ".msg";
    std::string candidate = concatDirAndFile(currentDir.c_str(), msgFile.c_str());
    if (fileExists(candidate.c_str()))
        return candidate;
    for (auto dir : opts.importPath) {
        std::string candidate = concatDirAndFile(dir.c_str(), msgFile.c_str());
        if (fileExists(candidate.c_str()))
            return candidate;
    }
    return "";
}


std::string MsgCompiler::prefixWithNamespace(const std::string& name, const std::string& namespaceName)
{
    return !namespaceName.empty() ? namespaceName + "::" + name : name;  // prefer name from local namespace
}

void MsgCompiler::generateCode(MsgFileElement *fileElement)
{
    std::string currentNamespace = "";
    NamespaceElement *firstNSElem = fileElement->getFirstNamespaceChild();
    std::string firstNSName = firstNSElem ? firstNSElem->getName() : "";
    codegen.generateProlog(fileElement->getFilename(), firstNSName, opts.exportDef);
    std::map<std::string, std::map<std::string, std::string>> classExtraCode; //per-class, per-method

    // generate forward declarations so that cyclic references compile; also collect cplusplus blocks
    for (ASTNode *child = fileElement->getFirstChild(); child; child = child->getNextSibling()) {
        switch (child->getTagCode()) {
            case MSG_NAMESPACE: {
                // open namespace(s)
                if (!currentNamespace.empty())
                    codegen.generateNamespaceEnd(currentNamespace, false);
                NamespaceElement *nsElem = check_and_cast<NamespaceElement *>(child);
                currentNamespace = nsElem->getName();
                codegen.generateNamespaceBegin(currentNamespace, false);
                break;
            }
            case MSG_STRUCT:
            case MSG_CLASS:
            case MSG_MESSAGE:
            case MSG_PACKET: {
                std::string qname = prefixWithNamespace(child->getAttribute(ATT_NAME), currentNamespace);
                ClassInfo& classInfo = typeTable.getClassInfo(qname);
                analyzer.ensureAnalyzed(classInfo);
                if (classInfo.generateClass)
                    codegen.generateTypeAnnouncement(classInfo);
                break;
            }
            case MSG_CPLUSPLUS: {
                CplusplusElement *cppElem = check_and_cast<CplusplusElement *>(child);
                analyzer.analyzeCplusplusBlockTarget(cppElem, currentNamespace);
                break;
            }
        }
    }
    if (!currentNamespace.empty()) {
        codegen.generateNamespaceEnd(currentNamespace, false);
        currentNamespace = "";
    }

    // generate classes, C++ blocks, etc.
    for (ASTNode *child = fileElement->getFirstChild(); child; child = child->getNextSibling()) {
        switch (child->getTagCode()) {
            case MSG_NAMESPACE: {
                // open namespace(s)
                if (!currentNamespace.empty())
                    codegen.generateNamespaceEnd(currentNamespace);
                NamespaceElement *nsElem = check_and_cast<NamespaceElement *>(child);
                currentNamespace = nsElem->getName();
                codegen.generateNamespaceBegin(currentNamespace);
                break;
            }

            case MSG_CPLUSPLUS: {
                // print C++ block
                CplusplusElement *cppElem = check_and_cast<CplusplusElement *>(child);
                std::string body = cppElem->getBody();
                std::string target = cppElem->getTarget();
                if (target == "" || target == "h")
                    codegen.generateHeaderCplusplusBlock(body);
                else if (target == "cc")
                    codegen.generateImplCplusplusBlock(body);
                break;
            }

            case MSG_IMPORT: {
                ImportElement *importElem = check_and_cast<ImportElement *>(child);
                std::string importName = importElem->getImportSpec();
                codegen.generateImport(importName);
                break;
            }

            case MSG_ENUM: {
                EnumElement *enumElem = check_and_cast<EnumElement *>(child);
                std::string qname = prefixWithNamespace(enumElem->getName(), currentNamespace);
                const EnumInfo& enumInfo = typeTable.getEnumInfo(qname);
                codegen.generateEnum(enumInfo);
                break;
            }

            case MSG_STRUCT:
            case MSG_CLASS:
            case MSG_MESSAGE:
            case MSG_PACKET: {
                std::string qname = prefixWithNamespace(child->getAttribute(ATT_NAME), currentNamespace);
                ClassInfo& classInfo = typeTable.getClassInfo(qname);
                analyzer.ensureAnalyzed(classInfo);
                analyzer.ensureFieldsAnalyzed(classInfo);
                if (errors->containsError())
                    break; // prevent asserts/errors from firing during code generation
                if (classInfo.generateClass) {
                    if (isQualified(classInfo.className))
                        errors->addError(classInfo.astNode, "type name may only be qualified when generating descriptor for an existing class: '%s'", classInfo.className.c_str());
                    if (child->getTagCode() == MSG_STRUCT)
                        codegen.generateStruct(classInfo, opts.exportDef);
                    else
                        codegen.generateClass(classInfo, opts.exportDef);
                }
                if (classInfo.generateDescriptor)
                    codegen.generateDescriptorClass(classInfo);
                break;
            }
        }
    }

    if (!currentNamespace.empty())
        codegen.generateNamespaceEnd(currentNamespace);
    codegen.generateEpilog();
}

void MsgCompiler::validateNamespaceName(const std::string& namespaceName, ASTNode *element)
{
    if (namespaceName.empty())
        errors->addError(element, "namespace name is empty");
    for (auto token : opp_split(namespaceName, "::"))
        if (contains(MsgAnalyzer::RESERVED_WORDS, token))
            errors->addError(element, "namespace name '%s' is a reserved word", token.c_str());
}

void MsgCompiler::printPropertiesLatexDocu(std::ostream& out)
{
    if (used)
        throw opp_runtime_error("MsgCompiler is a one-shot object, make a new instance to compile another file");
    used = true;

    importBuiltinDefinitions();

    std::map<std::string,const Property*> sortedPropertyDecls;
    auto& properties = typeTable.getGlobalProperties().getAll();
    for (const Property& p : properties)
        if (p.getName() == "property" && opp_join(p.getValue("internal"),"") != "true")
            sortedPropertyDecls[p.getIndex()] = &p;

    out << "\\begin{description}\n";
    for (auto pair : sortedPropertyDecls) {
        const Property *p = pair.second;
        out << "\\item[" << p->getIndex() << "] ";
        out << "\\textit{(type: " << opp_join(p->getValue("type"), "") << ", ";
        out << "use: " << opp_join(p->getValue("usage"), ", ") << ")} \\\\\n";
        out << opp_indentlines(opp_breaklines(opp_latexquote(opp_join(p->getValue("desc"), "")), 76), "  ") << "\n\n";
    }
    out << "\\end{description}\n";
}

}  // namespace nedxml
}  // namespace omnetpp
