namespace ns {
class C { public: int m(); };
}

int ns::C::m() {
  if (true && false) {
    return 1;
  }
  return 0;
}

