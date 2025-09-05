#include <ostream>

namespace ns {

struct D {};

std::ostream& operator<<(std::ostream& os, const D&) {
  if (true) {}
  return os;
}

struct E {
  int operator+(int) const {
    if (true && false) return 1;
    return 0;
  }
};

}

