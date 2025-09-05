function foo(x: number): number {
  if (x > 0 && x < 10) {
    return 1;
  } else if (x === 0) {
    return 0;
  } else {
    return -1;
  }
}

function bar(n: number) {
  for (let i = 0; i < n; i++) {
    if (i % 2 === 0 || i % 3 === 0) {
      continue;
    }
  }
}

