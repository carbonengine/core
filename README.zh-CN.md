# Carbon Core

[English](./README.md) | [简体中文](./README.zh-CN.md)

[![license](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE.txt)

## 概述

提供通用的底层功能,以及对系统调用的跨平台抽象。

## 🛠️ 构建

使用仓库根目录下提供的 `CMakeLists` 进行构建。

### 当前文档生成的环境要求

1. 文档可以在 Windows 或 macOS 上构建
2. 构建机器上需安装 Doxygen 1.12.0(或更高版本)

### 构建文档

文档构建的默认设置如下:
- TeamCity 构建代理:开启(ON)
- 本地开发构建:关闭(OFF)

如需覆盖默认的文档构建设置,请将 CMake 选项 `BUILD_DOCUMENTATION` 设为 `ON/OFF`。

构建 `INSTALL` 目标会构建全部文档,并将其放置在 `CMAKE_INSTALL_PREFIX` 指定的路径下。

文档的入口文件为 `documentation/index.html`。

文档源文件可以使用 .rst(reStructuredText)或 .md(Markdown)格式编写。

## 🤝 参与贡献

贡献遵循标准的 Git PR(Pull Request)流程。

提交 Pull Request 或以其他方式为本项目做出贡献,即表示您同意以 MIT 许可证授权您的贡献内容,并确认您拥有这样做的权利。

## 📄 许可证与法律声明

本项目基于 [MIT 许可证](LICENSE.txt)授权。MIT 许可证中的任何条款均不授予任何关于 CCP Games 商标或游戏内容的权利。

版权声明:© 2025 CCP Games。
