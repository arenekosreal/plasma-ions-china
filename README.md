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

## Adding new ion

You can always see source code of `plasma-workspace` or `kdeplasma-addons` or other ion in this project for more info.

1. Create a directory under [./src](./src)
2. Modify [./src/CMakeLists.txt] to add a new `add_subdirectory("name-of-the-directory")`
3. Create a `CMakeLists.txt` in the directory
4. Add instructions to let cmake create a new library

   You can use boolean `PlasmaWeatherIon_LEGACY` to detect if we are going to use ion library from `plasma-workspace`.

   If you are using legacy ion, your library must be `plasma_engine_${ION_NAME}`, `ION_NAME` is the first token of source request and response.

   If you are using newer ion from `kdeplasma-addons`, you must use `kcoreaddons_add_plugin` to create your library.
   Its `INSTALL_NAMESPACE` must be `${ION_INSTALL_NAMESPACE}`.

   Do not forget to call `ecm_qt_declare_logging_category` to generate header for using logging functions like `qDebug`, etc.

5. Create a metadata for your library in the directory

   It is a json document, which defines your library's metadata like name and description, etc.
   You need to use `K_PLUGIN_CLASS_WITH_JSON` macro with the name of json file in your source code.

6. Implement ion interface.

   For legacy ion, you need to implement `void IonInterface::reset()` and `bool IonInterfce::updateIonSource(const QString &source)`.
   See https://techbase.kde.org/Projects/Plasma/Weather/Ions for more info about source string and how to provide data to consumer.

   For new ion, you need to implement `void IonInterface::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Location>>> promise, const QString &searchString)`
   and `void IonInterface::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)`.

7. Add unit test (Optional)

   It is always better if you have unit tests to ensure your ion works well.

8. Debugging tips

   For legacy ion, if you just want to test if your ion is working, you can use `plasmaengineexplorer` in `plasma-sdk` to test it.

   You can also run `plasmoidviewer -a org.kde.plasma.weather` to see if it works in actual situation.

   Setting `QT_LOGGING_RULES=${LOGGING_CATEGORY}.debug=true` will make those programs print debug output of your ion.
   `${LOGGING_CATEGORY}` is the `CATEGORY_NAME` when you call `ecm_qt_declare_logging_category`.
