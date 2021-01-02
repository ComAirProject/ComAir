int main() {
  int sum = 0;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 5; ++j) {
      sum += i * j;
    }
  }
  return 0;
}
