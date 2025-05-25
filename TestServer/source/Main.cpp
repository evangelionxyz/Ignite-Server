#include <iostream>
#include <format>

int main() {

    std::string msg = std::format("Hello {}", "Evangelion");

    std::cout << msg;
}