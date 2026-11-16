#include <iostream>
#include <string>
#include <vector>

#include "transport/cli.h"

int main(int argc, char** argv) {
  return RunCli(std::vector<std::string>(argv + 1, argv + argc), std::cin,
                std::cout);
}
