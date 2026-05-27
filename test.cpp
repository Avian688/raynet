#include <variant>
#include <iostream>

int main() {
    std::variant<int, double> v = 3;
    std::visit([](auto x) { std::cout << x << "\n"; }, v);
}
