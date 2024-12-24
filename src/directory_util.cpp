#include "directory_util.h"

#if (_WIN64) | (_WIN32)
#include "Windows.h"
#endif



const char* _path_seperator_filter = "/\\"; 
const char* _dir_current_symbol = ".";
const char* _dir_up_symbol = "..";


void DirectoryUtil::split_directory_string(const std::string& path, std::vector<std::string>& target_split_data){
  target_split_data.clear();

  size_t _idx = 0;
  while(_idx < path.size()){
    _idx = path.find_first_not_of(_path_seperator_filter, _idx);
    size_t _target_idx = path.find_first_of(_path_seperator_filter, _idx);
    if(_target_idx != _idx)
      target_split_data.insert(target_split_data.end(), path.substr(_idx, _target_idx-_idx));

    _idx = _target_idx;
  }
}

std::string DirectoryUtil::get_absolute_path(const std::string& path){
  std::string _current_dir;

#if (_WIN64) | (_WIN32)
  DWORD _str_len = GetCurrentDirectory(NULL, 0);
  char* _cstr = (char*)malloc(_str_len+1);
  _str_len = GetCurrentDirectory(_str_len, _cstr);
  if(_str_len > 0)
    _current_dir = _cstr;

  free(_cstr);
#endif

  std::vector<std::string> _absolute_dir_data;
  split_directory_string(_current_dir, _absolute_dir_data);

  std::vector<std::string> _split_data;
  split_directory_string(path, _split_data);

  std::vector<std::string> _result_data;

  for(int i = 0; i < _split_data.size(); i++){
    std::string _current_str = _split_data[i];
    if(i == 0){
      if(_current_str == _dir_current_symbol || _current_str == _dir_up_symbol)
        _result_data.insert(_result_data.end(), _absolute_dir_data.begin(), _absolute_dir_data.end());
    }
    
    if(_current_str == _dir_current_symbol)
      continue;

    // size > 1 for drive path
    if(_current_str == _dir_up_symbol && _result_data.size() > 1){
      _result_data.erase(_result_data.end()-1);
      continue;
    }

    _result_data.insert(_result_data.end(), _current_str);
  }

  std::string _result;
  for(auto _iter: _result_data)
    _result += _iter;

  return _result;
}


std::string DirectoryUtil::strip_path(const std::string& path){
  return path.substr(path.find_last_of(_path_seperator_filter, path.size()) + 1);
}

std::string DirectoryUtil::strip_filename(const std::string& path){
  return path.substr(0, path.find_last_of(_path_seperator_filter));
}