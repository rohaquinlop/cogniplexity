function foo(x) {
  if (x > 0) {
    return 1;
  } else if (x < 0) {
    return -1;
  } else {
    return 0;
  }
}

class C {
  bar(n) {
    for (let i = 0; i < n; i++) {
      if (i % 2 === 0) {
        continue;
      }
    }
  }
}

