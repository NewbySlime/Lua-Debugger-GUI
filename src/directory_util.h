#ifndef DIRECTORY_UTIL_HEADER
#define DIRECTORY_UTIL_HEADER

#include "string"
#include "vector"


namespace DirectoryUtil{
  void split_directory_string(const std::string& path, std::vector<std::string>& target_split_data);

  std::string strip_path(const std::string& path);
  std::string strip_filename(const std::string& path);
}

#endif