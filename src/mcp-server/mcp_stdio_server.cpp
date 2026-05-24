#include "mcp_stdio_server.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace wsl::mcp_server {

mcp_stdio_server::mcp_stdio_server () = default;

void
mcp_stdio_server::set_server_info (const std::string &name, const std::string &version)
{
  name_ = name;
  version_ = version;
}

void
mcp_stdio_server::set_capabilities (const mcp::json &capabilities)
{
  capabilities_ = capabilities;
}

void
mcp_stdio_server::set_instructions (const std::string &instructions)
{
  instructions_ = instructions;
}

void
mcp_stdio_server::register_tool (const mcp::tool &tool, tool_handler handler)
{
  tools_[tool.name] = std::make_pair (tool, handler);

  // Lazily register tools/list and tools/call handlers on first tool registration
  if (method_handlers_.find ("tools/list") == method_handlers_.end ()) {
    method_handlers_["tools/list"] = [this](const mcp::json & /*params*/,
                                             const std::string & /*session_id*/) -> mcp::json {
      mcp::json tools_json = mcp::json::array ();
      for (const auto &[name, tool_pair] : tools_) {
        tools_json.push_back (tool_pair.first.to_json ());
      }
      return mcp::json{{"tools", tools_json}};
    };
  }

  if (method_handlers_.find ("tools/call") == method_handlers_.end ()) {
    method_handlers_["tools/call"] = [this](const mcp::json &params,
                                             const std::string & /*session_id*/) -> mcp::json {
      if (!params.contains ("name")) {
        throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                    "Missing 'name' parameter");
      }

      std::string tool_name = params["name"];
      auto it = tools_.find (tool_name);
      if (it == tools_.end ()) {
        throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                    "Tool not found: " + tool_name);
      }

      mcp::json tool_args = params.contains ("arguments") ? params["arguments"] : mcp::json::object ();

      if (tool_args.is_string ()) {
        try {
          tool_args = mcp::json::parse (tool_args.get<std::string> ());
        } catch (const mcp::json::exception &e) {
          throw mcp::mcp_exception (mcp::error_code::invalid_params,
                                    "Invalid JSON arguments: " + std::string (e.what ()));
        }
      }

      mcp::json tool_result = {
        {"isError", false}
      };

      try {
        tool_result["content"] = it->second.second (tool_args, "");
      } catch (const std::exception &e) {
        tool_result["isError"] = true;
        tool_result["content"] = mcp::json::array ({
          {
            {"type", "text"},
            {"text", e.what ()}
          }
        });
      }

      return tool_result;
    };
  }
}

void
mcp_stdio_server::run ()
{
  std::string line;
  while (std::getline (std::cin, line)) {
    if (line.empty ())
      continue;

    mcp::json req_json;
    try {
      req_json = mcp::json::parse (line);
    } catch (const mcp::json::exception &e) {
      send_response (mcp::response::create_error (nullptr, mcp::error_code::parse_error,
                                                     "Invalid JSON: " + std::string (e.what ()))
                        .to_json ());
      continue;
    }

    mcp::request req = mcp::request::from_json (req_json);

    if (req.is_notification ()) {
      if (req.method == "notifications/initialized") {
        initialized_ = true;
      }
      // Notifications don't get a response
      continue;
    }

    mcp::json response = process_request (req);
    send_response (response);
  }
}

mcp::json
mcp_stdio_server::handle_initialize (const mcp::request &req)
{
  const mcp::json &params = req.params;

  if (!params.contains ("protocolVersion") || !params["protocolVersion"].is_string ()) {
    return mcp::response::create_error (req.id, mcp::error_code::invalid_params,
                                         "Expected string for 'protocolVersion' parameter")
      .to_json ();
  }

  std::string requested_version = params["protocolVersion"];
  std::string negotiated_version = mcp::MCP_VERSION;

  mcp::json server_info = {
    {"name", name_},
    {"version", version_}
  };

  mcp::json result = {
    {"protocolVersion", negotiated_version},
    {"capabilities", capabilities_},
    {"serverInfo", server_info}
  };

  if (!instructions_.empty ()) {
    result["instructions"] = instructions_;
  }

  return mcp::response::create_success (req.id, result).to_json ();
}

mcp::json
mcp_stdio_server::process_request (const mcp::request &req)
{
  try {
    if (req.method == "initialize") {
      return handle_initialize (req);
    } else if (req.method == "ping") {
      return mcp::response::create_success (req.id, mcp::json::object ()).to_json ();
    }

    if (!initialized_) {
      return mcp::response::create_error (req.id, mcp::error_code::invalid_request,
                                           "Session not initialized")
        .to_json ();
    }

    auto it = method_handlers_.find (req.method);
    if (it != method_handlers_.end ()) {
      mcp::json result = it->second (req.params, "");
      return mcp::response::create_success (req.id, result).to_json ();
    }

    return mcp::response::create_error (req.id, mcp::error_code::method_not_found,
                                         "Method not found: " + req.method)
      .to_json ();
  } catch (const mcp::mcp_exception &e) {
    return mcp::response::create_error (req.id, e.code (), e.what ()).to_json ();
  } catch (const std::exception &e) {
    return mcp::response::create_error (req.id, mcp::error_code::internal_error,
                                         "Internal error: " + std::string (e.what ()))
      .to_json ();
  } catch (...) {
    return mcp::response::create_error (req.id, mcp::error_code::internal_error,
                                         "Unknown internal error")
      .to_json ();
  }
}

void
mcp_stdio_server::send_response (const mcp::json &response)
{
  std::cout << response.dump () << std::endl;
}

} // namespace wsl::mcp_server
