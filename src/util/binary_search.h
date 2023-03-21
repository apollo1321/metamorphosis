template <class Integral, class Predicate>
Integral BinarySearch(Integral begin, Integral end, Predicate predicate) {
  while (begin < end) {
    auto middle = begin + (end - begin) / 2;
    if (predicate(middle)) {
      end = middle;
    } else {
      begin = middle + 1;
    }
  }
  return begin;
}
