#include "directory_util.h"



const char* _path_seperator_filter = "/\\"; 


void DirectoryUtil::split_directory_string(const std::string& path, std::vector<std::string>& target_split_data){
  target_split_data.clear();

  size_t _idx = 0;
  while(_idx < path.size()){
    _idx = path.find_first_not_of(_path_seperator_filter, _idx);
    size_t _target_idx = path.find_first_of(_path_seperator_filter, _idx);
    if(_target_idx != _idx)
      target_split_data.insert(target_split_data.begin(), path.substr(_idx, _target_idx));

    _idx = _target_idx;
  }
}


std::string DirectoryUtil::strip_path(const std::string& path){
  return path.substr(path.find_last_of(_path_seperator_filter, path.size()) + 1);
}

std::string DirectoryUtil::strip_filename(const std::string& path){
  return path.substr(0, path.find_last_of(_path_seperator_filter));
}