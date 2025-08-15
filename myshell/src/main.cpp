#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include "shell.hpp"

int main(int argc, char** argv) {
    Shell shell;
    return shell.run(argc, argv);
}
