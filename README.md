# Plasma Weather China Sources

A collection of ions for Chinese users.

## Usage

You should be able to search Chinese cities and obtain weather report in KDE's weather widget after installed this.

## Installation

1. Install [dependencies](#dependencies)
2. Build and install the project:

   ```bash
   git clone https://github.com/arenekosreal/plasma-ions-china
   cmake -B build -S "/path/to/plasma-ions-china"
   cmake --build  build
   cmake --install build
   ```
   It is strongly recommended that you should create a package for your Linux Distribution.
   Because you do not want to mix files managed by package manager and yourself.

3. Open KDE's weather widget (org.kde.plasma.weather) and search Chinese cities like Beijing, etc.

   For just experiencing, you can run `plasmoidviewer -a org.kde.plasma.weather` after you installed `plasma-sdk`.

### Dependencies

- qt6-base
- plasma-workspace < 6.5 OR kdeplasma-addons >= 6.5
- cmake
- extra-cmake-modules
- git (If you want to automatically set project version from git tag)
- gettext

## Ions

|Name|Source|Reverse Engineered|Comment|
|----|------|------------------|-------|
|nmccn|[./src/www.nmc.cn](./src/www.nmc.cn)|Y|APIs are scratched from Dev Tools of Chromium.|
