# BYTRA

[![Build Status](https://travis-ci.com/ArneWouters/BYTRA.svg?token=whAYzQpaYXnwwohSyHG7&branch=master)](https://travis-ci.com/ArneWouters/BYTRA)
[![GitHub](https://img.shields.io/github/license/ArneWouters/BYTRA?color=blue)](https://github.com/ArneWouters/BYTRA/blob/master/LICENSE)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/ArneWouters/BYTRA)](https://github.com/ArneWouters/BYTRA/releases/latest)

* An automated algorithmic cryptocurrency trader for Bybit.
* All BYTRA code is released under MIT license
* Other code in this repo released under its respective license

## Getting Started

### Prerequisites

These have to be installed on the current system.
 * zlib1g-dev
 * libssl-dev
 * TA-Lib
 * Boost
 
Installing zlib1g-dev and libssl-dev:

```bash
sudo apt-get install zlib1g-dev libssl-dev
```
 
Installing TA-Lib:

```bash
wget https://sourceforge.net/projects/ta-lib/files/ta-lib/0.4.0/ta-lib-0.4.0-src.tar.gz && tar -xvzf ta-lib-0.4.0-src.tar.gz > /dev/null
cd ta-lib
./configure --prefix=/usr
make && sudo make install
```
 
Installing Boost:

```bash
wget https://sourceforge.net/projects/boost/files/boost/1.73.0/boost_1_73_0.tar.gz && tar -xvzf boost_1_73_0.tar.gz > /dev/null
cd boost_1_73_0
./bootstrap.sh --prefix=/usr
./b2 && sudo ./b2 install
```

## Usage

### Clone the repository

```bash
git clone https://github.com/ArneWouters/BYTRA.git
cd Bytra
```

### Build and run the target

Use the following command to build and run the executable target.

```bash
cmake -Hbytra -Bbuild/bytra
cmake --build build/bytra
./build/bytra/Bytra --help
```

### Build and run test suite

Use the following commands from the project's root directory to run the test suite.

```bash
cmake -Htest -Bbuild/test
cmake --build build/test
CTEST_OUTPUT_ON_FAILURE=1 cmake --build build/test --target test

# or simply call the executable: 
./build/test/BytraTests
```

### Run clang-format

Use the following commands from the project's root directory to check and fix C++ and CMake source style.
This requires _clang-format_, _cmake-format_ and _pyyaml_ to be installed on the current system.

```bash
cmake -Htest -Bbuild/test

# view changes
cmake --build build/test --target format

# apply changes
cmake --build build/test --target fix-format
```

See [Format.cmake](https://github.com/TheLartians/Format.cmake) for details.

## Built with
 * [CLI11](https://github.com/CLIUtils/CLI11) - Header only single or multi-file C++11 library for simple and advanced CLI parsing.
 * [spdlog](https://github.com/gabime/spdlog) - Super fast, header only, C++ logging library.
 * [toml++](https://github.com/marzer/tomlplusplus) - Header-only TOML parser and serializer for C++17 and later.
 * [Boost.Beast](https://github.com/boostorg/beast) - HTTP and WebSocket built on Boost.Asio in C++11.
 * [simdjson](https://github.com/lemire/simdjson) - Accelerate the parsing of JSON per se using commonly available SIMD instructions
 * [cpr](https://github.com/whoshuu/cpr) - A modern C++ HTTP requests library with a simple but powerful interface. Modeled after the Python Requests module.
 * [TA-Lib](https://sourceforge.net/projects/ta-lib/) - Technical analysis library with indicators like ADX, MACD, RSI, Stochastic, TRIX...
