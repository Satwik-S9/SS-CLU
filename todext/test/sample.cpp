// sample.cpp — fixture for todext testing

#include <iostream>

// TODO: implement the fast path here
//   it should handle the edge case where n == 0
//   and also the negative input case

void foo(int n) {
    // BUG: off-by-one error when n wraps around at INT_MAX
    //   this was noticed during stress testing
    for (int i = 0; i <= n; ++i) {
        std::cout << i << "\n";
    }
}

/* TODO: add unit tests for foo()
   covering boundary values and negative inputs */

int main() {
    foo(5);
    // TODO: single-line todo with no continuation
	// NOTE: This is something that should be highlighted too
    return 0;
}

