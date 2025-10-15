# Carbon Core

[![license](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE.txt)

## Overview

Provides generic low-level functionality and cross-platform abstractions for system calls

## 🛠️ Building

Build using provided `CMakeLists` in the repository root.

### Current requirements for documentation generation

1. Documentation can be built on either Windows or macOS
2. Doxygen 1.12.0 (or higher) installed on build machine

### Building the documentation

Documentation is built using the following defaults:
- TeamCity build agents: ON
- Local dev build: OFF

To override the default documentation build settings, turn the CMake option `BUILD_DOCUMENTATION` to `ON/OFF`.

Building the `INSTALL` target will build all documentation and place it in the path specified by `CMAKE_INSTALL_PREFIX`.

Entry point for the documentation is `documentation/index.html`.

Documentation can be built using either .rst (restructuredText) or .md (markdown) files as sources.

## 🤝 Contributing

Contribution follows the standard GIT PR model.

By submitting a pull request or otherwise contributing to this project, you agree to license your contribution under the MIT Licence, and you confirm that you have the right to do so.

## 📄 License and Legal Notices

This project is licensed under the [MIT Licence](LICENSE.txt). Nothing in the MIT License grants any rights to CCP Games' trademarks or game content.

Copyright notice: © 2025 CCP Games.