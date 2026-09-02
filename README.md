# Smart Talk
*By Seb263*

Smart Talk enhances dialogue by making conversations clearer, smoother, and more dynamic. It highlights quest-related lines, lets you reorder dialogue options, adds natural pauses, and offers flexible skipping and interruption features, and full support for controllers and other mods.

The mod can be downloaded here: [nexusmods.com](https://www.nexusmods.com/skyrimspecialedition/mods/161500)

## Requirements

* [XMake](https://xmake.io) [3.0.0+]
* C++23 Compiler (MSVC, Clang-CL)

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Register Visual Studio as a Generator

If you use Visual Studio, run the following command:
```bat
xmake project -k vsxmake
```

> ***Note:*** *This will generate a `vsxmakeXXXX/` directory in the **project's root directory** using the latest version of Visual Studio installed on the system.*

## Building

To build the project, run the following command:
```bat
xmake build
```

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. The terms and conditions of the GPL-3.0 apply to this project.

> [!Note]
> If I'm still around and actively maintaining the project, please consider contributing improvements upstream rather than maintaining a separate fork whenever possible. It makes everyone's life easier, including mine.
>
> If I stop responding to messages and my [Nexus profile](https://www.nexusmods.com/profile/Seb263) shows no sign of activity for at least **three months**, please consider the project abandoned and feel free to take over its development.
