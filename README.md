# Plasma Weather China Sources

A collection of ions for Chinese users.

## What is Ion

Ion is KDE's weather source engine, which can provide weather data to its weather widget (org.kde.plasma.weather).
KDE itself has some Ions, like `wetter.com`, `noaa`, etc. If you have used the weather widget, you will not miss them.

## Why create this

Although KDE itself has some Ions, they are not suitable for Chinese users. Only a few cities can be found and their reports are too brief.
That's why this project is created. We collect some weather apis focus on Chinese cities so we can have a better experience when using KDE's weather widget.

## Usage

You should be able to search Chinese cities and obtain weather report in KDE's weather widget after installed this.

1. Install [dependencies](#dependencies)
2. Clone this repository using git or downloading archive
3. Build and install the project:

   ```bash
   cmake -B build -S "/path/to/plasma-ions-china"
   cmake --build  build
   cmake --install build
   ```
   There is no need to set `CMAKE_INSTALL_PREFIX`, because it will follow KDE's and not be configurable. That means most of the time it is `/usr`.
   So it is strongly recommended that you should create a package for your Linux Distribution.
   Because you do not want to mix files managed by package manager and yourself.

4. Open KDE's weather widget (org.kde.plasma.weather) and search Chinese cities like Beijing, etc.

   For just experiencing, you can run `plasmoidviewer -a org.kde.plasma.weather` after you installed `plasma-sdk`.

### Dependencies

#### Runtime dependencies

- qt6-base
- plasma-workspace < 6.5 OR kdeplasma-addons >= 6.5

#### Buildtime extra dependencies

- cmake
- extra-cmake-modules
- git (If you want to automatically set project version from git tag)

## Ions

|Name|Reverse Engineered|Comment|
|----|------------------|-------|
|[nmccn](./src/www.nmc.cn)|Y|The passed daytime report is not available when is night.|

## Adding new ion

Here are some steps and tips about adding new ion yourself.
But do not forget you can always see source code of `plasma-workspace` or `kdeplasma-addons` or other ion in this project for more info.

1. Create a directory under [./src](./src)
2. Modify [./src/CMakeLists.txt](./src/CMakeLists.txt) to add a new instruction `add_subdirectory("name-of-the-directory")`
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

   For legacy ion, you need to implement those methods:

   - `void IonInterface::reset()`
   - `bool IonInterface::updateIonSource(const QString &source)`

   See https://techbase.kde.org/Projects/Plasma/Weather/Ions for more info about source string and how to provide data to consumer.
   See [/usr/include/plasma5support/weather/ion.h](/usr/include/plasma5support/weather/ion.h) for more about `IonInterface`.

   For new ion, you need to implement those methods:

   - `void Ion::findPlaces(std::shared_ptr<QPromise<std::shared_ptr<Location>>> promise, const QString &searchString)`
   - `void Ion::fetchForecast(std::shared_ptr<QPromise<std::shared_ptr<Forecast>>> promise, const QString &placeInfo)`

   When you need translate some strings, you can use macro `KDE_WEATHER_TRANSLATION_DOMAIN` with methods like `i18nd` to reuse existing translation from KDE.
   Our default translation domain is `plasma_ions_china`.

7. Add unit test (Optional)

   It is always better if you have unit tests to ensure your ion works well.

   Add a directory with the name same to you created in step 1.
   Then create `CMakeLists.txt` to define tests.
   After that, modify [./tests/CMakeLists.txt](./tests/CMakeLists.txt) by adding a new instruction `add_subdirectory("name-of-the-directory")`.

8. Update readme and translation template

   You need to update [ions](#ions) table to add information about your ion.
   Translation template can get updated by using commands like `xgettext -o ./po/plasma_ions_china.pot -d plasma_ions_china ...`. 

### Debugging tips

   For legacy ion, if you just want to test if your ion is working, you can use `plasmaengineexplorer` in `plasma-sdk` to test it.

   You can also run `plasmoidviewer -a org.kde.plasma.weather` to see if it works in actual situation.

   Setting `QT_LOGGING_RULES=${LOGGING_CATEGORY}.debug=true` will make those programs print debug output of your ion.
   `${LOGGING_CATEGORY}` is the `CATEGORY_NAME` when you call `ecm_qt_declare_logging_category`.
