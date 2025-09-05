template <class T>
struct C { int m(); };

template <class T>
int C<T>::m() {
  if (true || false && true) return 1;
  return 0;
}

