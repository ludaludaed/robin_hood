#include <iostream>
#include <string>
#include "robin_hood.h"

int main() {
    static_assert(std::bidirectional_iterator<ludaed::unordered_map<int, int>::iterator>);
    {
        ludaed::unordered_map<std::string, int> map;
        for (int i = 0; i < 100; ++i) {
            map.insert({std::to_string(i), i});
        }

        for (const auto &item: map) {

            bool res = const_cast<const ludaed::unordered_map<std::string, int> &>(map).find(item.first) ==
                       const_cast<const ludaed::unordered_map<std::string, int> &>(map).end();

            if (res == 1) {
                const_cast<const ludaed::unordered_map<std::string, int> &>(map).find(item.first);
            }
            std::cout << item.first << " " << item.second << " "
                      << res << std::endl;
        }

        for (int i = 0; i < 100; ++i) {
            if (!map.contains(std::to_string(i))) {
                std::cout << i << std::endl;
            }
            map.erase(std::to_string(i));
        }
        std::cout << "==================================" << std::endl;
        for (const auto &item: map) {

            bool res = const_cast<const ludaed::unordered_map<std::string, int> &>(map).find(item.first) ==
                       const_cast<const ludaed::unordered_map<std::string, int> &>(map).end();

            if (res == 1) {
                const_cast<const ludaed::unordered_map<std::string, int> &>(map).find(item.first);
            }
            std::cout << item.first << " " << item.second << " "
                      << res << std::endl;
        }

        for (int i = 0; i < 100; ++i) {
            if (!map.contains(std::to_string(i))) {
                std::cout << i << std::endl;
            }
            map.erase(std::to_string(i));
        }
    }
    return 0;
}