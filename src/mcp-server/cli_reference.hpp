#pragma once

#include <string>
#include <vector>

struct cli_command_info {
  std::string name;
  std::string category;
  std::string description;
  std::string syntax;
  std::vector<std::string> parameters;
  std::vector<std::string> examples;
};

const std::vector<cli_command_info> &get_cli_reference ();
const cli_command_info *find_command (const std::string &name);
