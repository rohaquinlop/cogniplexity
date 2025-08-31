#include "../include/file_operations.h"

std::string load_file_content(std::string& path) {
  std::ifstream file;
  std::stringstream buffer;

  file.open(path);
  if (file.fail()) {
    throw std::runtime_error("Failed to open file: " + path);
  }

  buffer << file.rdbuf();
  file.close();

  return buffer.str();
}
