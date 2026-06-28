# ![Ext icon](res/EXTICON.BMP "Icon") OpenGL-Surface
Replacement extension for Min's OpenGL Base object for MMF2/CF2.5. Instead of creating a Win32 child window, this extension draws the OpenGL window using Fusion's native `cSurface` API, ensuring any other objects can be placed above the OpenGL scene. Transparency is also supported.

Usage of this extension is pretty much the same as the original OpenGL Base object.

# Build Prerequisites
If you wish to build the source code yourself you will need the following:
- [Fusion 2.5 Windows SDK](https://www.clickteam.com/extensions-sdks)
