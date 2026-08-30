#include "types.hpp"

namespace wsl
{
namespace serialize
{

// ------------------------------------------------------------------
// json_writer
// ------------------------------------------------------------------

json_writer::json_writer ()
{
  m_doc = yyjson_mut_doc_new (nullptr);
  yyjson_mut_val *root = yyjson_mut_obj (m_doc);
  yyjson_mut_doc_set_root (m_doc, root);
  m_stack.push_back (root);
}

json_writer::~json_writer () { yyjson_mut_doc_free (m_doc); }

yyjson_mut_val *
json_writer::make_key (std::string_view key)
{
  // Copies the key bytes (yyjson's add_str* variants do not copy).
  return yyjson_mut_strn (m_doc, key.data (), key.size ());
}

yyjson_mut_val *
json_writer::current ()
{
  return m_stack.back ();
}

void
json_writer::begin_object (std::string_view key)
{
  yyjson_mut_val *obj = yyjson_mut_obj (m_doc);
  yyjson_mut_obj_put (current (), make_key (key), obj);
  m_stack.push_back (obj);
}

void
json_writer::begin_element_object ()
{
  yyjson_mut_val *obj = yyjson_mut_obj (m_doc);
  yyjson_mut_arr_append (current (), obj);
  m_stack.push_back (obj);
}

void
json_writer::end_object ()
{
  m_stack.pop_back ();
}

void
json_writer::begin_array (std::string_view key)
{
  yyjson_mut_val *arr = yyjson_mut_arr (m_doc);
  yyjson_mut_obj_put (current (), make_key (key), arr);
  m_stack.push_back (arr);
}

void
json_writer::end_array ()
{
  m_stack.pop_back ();
}

void
json_writer::write_u64 (std::string_view key, std::uint64_t value)
{
  yyjson_mut_obj_put (current (), make_key (key),
                      yyjson_mut_uint (m_doc, value));
}

void
json_writer::write_double (std::string_view key, double value)
{
  yyjson_mut_obj_put (current (), make_key (key),
                      yyjson_mut_real (m_doc, value));
}

void
json_writer::write_bool (std::string_view key, bool value)
{
  yyjson_mut_obj_put (current (), make_key (key),
                      yyjson_mut_bool (m_doc, value));
}

void
json_writer::write_string (std::string_view key, std::string_view value)
{
  yyjson_mut_val *str = yyjson_mut_strn (m_doc, value.data (), value.size ());
  yyjson_mut_obj_put (current (), make_key (key), str);
}

yyjson_mut_val *
json_writer::graft_string (const std::string &json)
{
  yyjson_doc *sub = yyjson_read (json.c_str (), json.size (),
                                 YYJSON_READ_ALLOW_INVALID_UNICODE);
  if (sub == nullptr) {
    return nullptr;
  }

  yyjson_mut_val *copy = yyjson_val_mut_copy (m_doc, yyjson_doc_get_root (sub));
  yyjson_doc_free (sub);
  return copy;
}

void
json_writer::attach_json (std::string_view key, const std::string &json)
{
  yyjson_mut_val *copy = graft_string (json);
  if (copy != nullptr) {
    yyjson_mut_obj_put (current (), make_key (key), copy);
  }
}

void
json_writer::append_element_json (const std::string &json)
{
  yyjson_mut_val *copy = graft_string (json);
  if (copy != nullptr) {
    yyjson_mut_arr_append (current (), copy);
  }
}

std::string
json_writer::to_string () const
{
  std::size_t len = 0;
  char *json = yyjson_mut_write (m_doc, 0, &len);
  if (json == nullptr) {
    return "{}";
  }
  std::string out (json, len);
  free (json); // NOLINT(cppcoreguidelines-own-memory) — yyjson allocation
  return out;
}

// ------------------------------------------------------------------
// json_reader
// ------------------------------------------------------------------

json_reader::json_reader (const std::string &json)
{
  m_doc = yyjson_read (json.data (), json.size (), 0);
  if (m_doc != nullptr) {
    m_stack.push_back (yyjson_doc_get_root (m_doc));
  }
}

json_reader::~json_reader () { yyjson_doc_free (m_doc); }

bool
json_reader::valid () const
{
  return m_doc != nullptr;
}

yyjson_val *
json_reader::current () const
{
  return m_stack.back ();
}

yyjson_val *
json_reader::find (std::string_view key) const
{
  if (m_stack.empty () || !yyjson_is_obj (current ())) {
    return nullptr;
  }
  return yyjson_obj_getn (current (), key.data (), key.size ());
}

bool
json_reader::enter_object (std::string_view key)
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_obj (val)) {
    return false;
  }
  m_stack.push_back (val);
  return true;
}

bool
json_reader::enter_array (std::string_view key)
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_arr (val)) {
    return false;
  }
  m_stack.push_back (val);
  return true;
}

bool
json_reader::enter_element (std::size_t index)
{
  if (m_stack.empty () || !yyjson_is_arr (current ())) {
    return false;
  }
  yyjson_val *val = yyjson_arr_get (current (), index);
  if (val == nullptr) {
    return false;
  }
  m_stack.push_back (val);
  return true;
}

std::size_t
json_reader::array_size (std::string_view key) const
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_arr (val)) {
    return 0;
  }
  return yyjson_arr_size (val);
}

void
json_reader::leave ()
{
  if (m_stack.size () > 1) {
    m_stack.pop_back ();
  }
}

bool
json_reader::read_u64 (std::string_view key, std::uint64_t &out) const
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_uint (val)) {
    // yyjson_is_uint is false for negative ints; accept any int as u64.
    if (val == nullptr || !yyjson_is_int (val)) {
      return false;
    }
  }
  out = yyjson_get_uint (val);
  return true;
}

bool
json_reader::read_double (std::string_view key, double &out) const
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_num (val)) {
    return false;
  }
  out = yyjson_get_real (val);
  return true;
}

bool
json_reader::read_bool (std::string_view key, bool &out) const
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_bool (val)) {
    return false;
  }
  out = yyjson_get_bool (val);
  return true;
}

bool
json_reader::read_string (std::string_view key, std::string &out) const
{
  yyjson_val *val = find (key);
  if (val == nullptr || !yyjson_is_str (val)) {
    return false;
  }
  out.assign (yyjson_get_str (val), yyjson_get_len (val));
  return true;
}

bool
json_reader::extract_json (std::string_view key, std::string &out) const
{
  yyjson_val *val = find (key);
  if (val == nullptr) {
    return false;
  }
  std::size_t len = 0;
  char *json = yyjson_val_write (val, 0, &len);
  if (json == nullptr) {
    return false;
  }
  out.assign (json, len);
  free (json); // NOLINT(cppcoreguidelines-own-memory) — yyjson allocation
  return true;
}

} // namespace serialize

} // namespace wsl
