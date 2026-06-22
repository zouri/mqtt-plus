# Third-Party Notices

This file is a release-preparation draft. Verify every license before publishing binaries or source archives.

## Qt

The application is built with Qt 6 modules including Core, Gui, Network, Qml, Quick, QuickControls2, Sql, Mqtt, Svg, Widgets, and LinguistTools.

Before publishing, document which Qt license path the project expects contributors and packagers to use. If distributing binaries, verify the deployment obligations for the selected Qt license and distribution channel.

## Lua

The build downloads Lua source with CMake `FetchContent` and builds it as a static library. The configured Lua version and source URL are declared in `CMakeLists.txt`.

Before publishing, include the Lua license text or a link to the exact Lua release license used by the build.

## Icons and Image Assets

The repository includes SVG resources and application icon assets under `resources/` and `assets/`.

Before publishing, confirm whether each asset is original project work or copied/adapted from a third-party icon set. Add attribution and license text for any third-party assets.

## Packaging Tools

The project uses CMake, CPack, and Qt deployment helpers for packaging. Platform packages may include Qt plugins and runtime libraries collected during deployment.

Before publishing binary packages, inspect the generated package contents and include any notices required by bundled runtime components.
