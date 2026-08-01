#include "das_interop.hpp"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_interop.h"
#include "daScript/daScript.h"

using namespace ::das;

namespace wsl::das
{

namespace
{

// ── describe_type(value) ──
// Accepts any daslang type and returns a description including type name,
// size, and struct field information when applicable.

vec4f
describe_type (::das::Context &context, SimNode_CallBase *call, vec4f *args)
{
  TypeInfo *ti = call->types[0];
  if (!ti) {
    auto result = context.allocateString ("unknown", nullptr);
    return cast<char *>::from (result);
  }

  TextWriter tw;

  switch (ti->type) {
  case Type::tInt:
    tw << "type = int, size = " << getTypeSize (ti);
    break;
  case Type::tInt8:
    tw << "type = int8, size = " << getTypeSize (ti);
    break;
  case Type::tInt16:
    tw << "type = int16, size = " << getTypeSize (ti);
    break;
  case Type::tInt64:
    tw << "type = int64, size = " << getTypeSize (ti);
    break;
  case Type::tUInt:
    tw << "type = uint, size = " << getTypeSize (ti);
    break;
  case Type::tUInt8:
    tw << "type = uint8, size = " << getTypeSize (ti);
    break;
  case Type::tUInt16:
    tw << "type = uint16, size = " << getTypeSize (ti);
    break;
  case Type::tUInt64:
    tw << "type = uint64, size = " << getTypeSize (ti);
    break;
  case Type::tFloat:
    tw << "type = float, size = " << getTypeSize (ti);
    break;
  case Type::tDouble:
    tw << "type = double, size = " << getTypeSize (ti);
    break;
  case Type::tBool:
    tw << "type = bool, size = " << getTypeSize (ti);
    break;
  case Type::tString:
    tw << "type = string, size = " << getTypeSize (ti);
    break;
  case Type::tStructure:
    tw << "type = struct";
    if (ti->structType) {
      tw << ", name = " << ti->structType->name;
      tw << ", fields = " << ti->structType->count;
    }
    tw << ", size = " << getTypeSize (ti);
    break;
  case Type::tEnumeration:
    tw << "type = enum";
    if (ti->enumType) {
      tw << ", name = " << ti->enumType->name;
    }
    tw << ", size = " << getTypeSize (ti);
    break;
  case Type::tHandle:
    tw << "type = handle";
    {
      auto ann = ti->getAnnotation ();
      if (ann) {
        tw << ", name = " << ann->name;
      }
    }
    tw << ", size = " << getTypeSize (ti);
    break;
  case Type::tPointer:
    tw << "type = pointer, size = " << getTypeSize (ti);
    break;
  case Type::tArray:
    tw << "type = array, size = " << getTypeSize (ti);
    break;
  case Type::tTable:
    tw << "type = table, size = " << getTypeSize (ti);
    break;
  default:
    tw << "type = " << das_to_string (ti->type);
    tw << ", size = " << getTypeSize (ti);
    break;
  }

  auto result = context.allocateString (tw.str (), &call->debugInfo);
  return cast<char *>::from (result);
}

// ── type_name(value) ──
// Returns just the type name string for any value.

vec4f
type_name (::das::Context &context, SimNode_CallBase *call, vec4f *args)
{
  TypeInfo *ti = call->types[0];
  if (!ti) {
    auto result = context.allocateString ("unknown", nullptr);
    return cast<char *>::from (result);
  }

  ::das::string resolved_name;

  switch (ti->type) {
  case Type::tStructure:
    if (ti->structType) {
      resolved_name = ti->structType->name;
    }
    break;
  case Type::tEnumeration:
    if (ti->enumType) {
      resolved_name = ti->enumType->name;
    }
    break;
  case Type::tHandle: {
    auto ann = ti->getAnnotation ();
    if (ann) {
      resolved_name = ann->name;
    }
    break;
  }
  default:
    break;
  }

  if (resolved_name.empty ()) {
    resolved_name = das_to_string (ti->type);
  }

  auto result
      = context.allocateString (resolved_name.c_str (), &call->debugInfo);
  return cast<char *>::from (result);
}

// ── type_size(value) ──
// Returns sizeof for the value's type.

vec4f
type_size (::das::Context &context, SimNode_CallBase *call, vec4f *args)
{
  TypeInfo *ti = call->types[0];
  int size = ti ? getTypeSize (ti) : 0;
  return cast<int32_t>::from (size);
}

// ── struct_field_count(value) ──
// Returns the number of fields for struct-typed values, 0 otherwise.

vec4f
struct_field_count (::das::Context &context, SimNode_CallBase *call,
                    vec4f *args)
{
  TypeInfo *ti = call->types[0];
  int count = 0;
  if (ti && ti->type == Type::tStructure && ti->structType) {
    count = ti->structType->count;
  }
  return cast<int32_t>::from (count);
}

// ── struct_field_names(value) ──
// Returns a comma-separated list of field names for struct-typed values.

vec4f
struct_field_names (::das::Context &context, SimNode_CallBase *call,
                    vec4f *args)
{
  TypeInfo *ti = call->types[0];
  if (!ti || ti->type != Type::tStructure || !ti->structType) {
    auto result = context.allocateString ("", nullptr);
    return cast<char *>::from (result);
  }

  TextWriter tw;
  for (int i = 0; i < ti->structType->count; ++i) {
    if (i > 0) {
      tw << ", ";
    }
    tw << ti->structType->fields[i]->name;
  }

  auto result = context.allocateString (tw.str (), &call->debugInfo);
  return cast<char *>::from (result);
}

// ── call_site_info() ──
// Returns the source file and line number of the call site.

vec4f
call_site_info (::das::Context &context, SimNode_CallBase *call, vec4f *)
{
  TextWriter tw;
  if (call->debugInfo.fileInfo) {
    tw << call->debugInfo.fileInfo->name << ":" << call->debugInfo.line;
  } else {
    tw << "<unknown>";
  }
  auto result = context.allocateString (tw.str (), &call->debugInfo);
  return cast<char *>::from (result);
}

} // anonymous namespace

void
register_interop_functions (::das::Module &mod)
{
  ::das::ModuleLibrary lib (&mod);
  lib.addBuiltInModule ();

  // describe_type(value : auto) => string
  // Accepts any daslang type. Returns a description with type name, size,
  // and struct field count when applicable.
  addInterop<describe_type, char *, vec4f> (
      mod, lib, "describe_type", SideEffects::none, "wsl::das::describe_type")
      ->arg ("value");

  // type_name(value : auto) => string
  // Accepts any daslang type. Returns the type name.
  addInterop<type_name, char *, vec4f> (
      mod, lib, "type_name", SideEffects::none, "wsl::das::type_name")
      ->arg ("value");

  // type_size(value : auto) => int
  // Accepts any daslang type. Returns sizeof the type.
  addInterop<type_size, int32_t, vec4f> (
      mod, lib, "type_size", SideEffects::none, "wsl::das::type_size")
      ->arg ("value");

  // struct_field_count(value : auto) => int
  // Returns the number of fields for struct values, 0 otherwise.
  addInterop<struct_field_count, int32_t, vec4f> (
      mod, lib, "struct_field_count", SideEffects::none,
      "wsl::das::struct_field_count")
      ->arg ("value");

  // struct_field_names(value : auto) => string
  // Returns comma-separated field names for struct values.
  addInterop<struct_field_names, char *, vec4f> (mod, lib, "struct_field_names",
                                                 SideEffects::none,
                                                 "wsl::das::struct_field_names")
      ->arg ("value");

  // call_site_info() => string
  // Returns the source file and line of the caller.
  addInterop<call_site_info, char *> (mod, lib, "call_site_info",
                                      SideEffects::none,
                                      "wsl::das::call_site_info");
}

} // namespace wsl::das
