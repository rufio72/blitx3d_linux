## BLitz3d

This compiler is a fork of the Blitz3D NG project, which itself was a fork of the original Blitz3D by the late Mark Sibly.
My goal is to modernize Blitz3D and make it compile on Linux, Windows, Mac and Android. Android compilation is really important to me. The other platforms I see more as a dress rehearsal.
Blitz3D will have object-oriented syntax and use GCC as its compiler. You'll be able to easily download ready-to-use versions for Linux and Windows.
Over time I'd like to introduce an alternative C-like syntax (for those who, like me, prefer it).
Yes, I know — reviving a compiler from 2000 with goto and gosub might seem a bit old-fashioned. But I think now that AI is learning to write code, this compiler could actually become a better solution than all those engines where you have to learn complex environments, study strategies and whatnot.
Anyway, even if nobody else ends up caring about this, I can tell you that working on this project just makes me feel good.

## Current status

23-12-2026
All the examples work. There might be something I haven't noticed, but overall they work.
There will probably be some bugs in texture handling, or something I haven't noticed, that slipped past me, but overall it seems like the first part of the work is done.
Then I'll try to get the tank game working. I consider that a test for the compiler's reliability.
When I manage to get that part working, I'll start considering the compiler ready.
Then I'll move on to creating the distributable version for Linux.

## Documentation
      
The original Blitz3D help is available in the [\_release/help](_release/help)
 directory in HTML form.
Documentation is generated from Markdown files using the built-in `docgen`
tool.


## New Features
### Object-Oriented Programming (OOP)

This version of Blitz3D introduces full OOP support with the following features:

**Classes**: Define custom types with `Class...End Class`
**Methods**: Member functions with `Method...End Method`
**Constructors**: Automatic initialization with `Method New()`
**Inheritance**: Extend classes with the `Extends` keyword
**Self/Super**: Access current instance and parent class methods
**Static Methods**: Class-level functions with `Static Method`
**Access Modifiers**: `Private`, `Protected`, and `Public` visibility control

See the full [OOP Documentation](docs/OOP.md) for details and examples.
## Roadmap





## License

```
The zlib/libpng License

Copyright (c) 2013 Blitz Research Ltd

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```
