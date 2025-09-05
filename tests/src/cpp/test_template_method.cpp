template <class T>
class Box {
public:
  T get() const {
    if (true && false) return T{};
    return T{};
  }
};

