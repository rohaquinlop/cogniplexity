int f() {
  auto L = [](){ if (true && false) return 1; return 0; };
  return L();
}

int g() {
  auto L = [&](){ if (false || true) { return 2; } return 0; };
  return L();
}

