# Change Log

This document lays out what we've changed from the original Blitz3D.

## Language Features

### Object-Oriented Programming (OOP)

Blitz3D-NG introduces full OOP support with the following features:

#### Classes and Methods

```blitz
Class Player
    Field name$
    Field health%

    Method Init(n$, h%)
        Self\name$ = n$
        Self\health% = h%
    End Method

    Method TakeDamage(amount%)
        Self\health% = Self\health% - amount%
    End Method

    Method GetInfo$()
        Return Self\name$ + " has " + Self\health% + " HP"
    End Method
End Class

Local p.Player = New Player()
p\Init("Hero", 100)
p\TakeDamage(25)
Print p\GetInfo$()  ; "Hero has 75 HP"
```

#### Inheritance with Extends

```blitz
Class Animal
    Field name$

    Method Speak$()
        Return "..."
    End Method
End Class

Class Dog Extends Animal
    Field breed$

    Method Speak$()
        Return "Woof!"
    End Method
End Class

Local d.Dog = New Dog()
d\name$ = "Rex"
Print d\Speak$()  ; "Woof!"
```

#### Super Keyword

Call parent class methods using `Super\Method()`:

```blitz
Class Animal
    Method Describe$()
        Return "I am an animal"
    End Method
End Class

Class Dog Extends Animal
    Field breed$

    Method Describe$()
        Return Super\Describe$() + " - specifically a " + Self\breed$
    End Method
End Class
```

#### Automatic Method Inheritance

Methods from parent classes are automatically inherited:

```blitz
Class Vehicle
    Method Start()
        Print "Engine started"
    End Method
End Class

Class Car Extends Vehicle
    ; Car inherits Start() from Vehicle
End Class

Local c.Car = New Car()
c\Start()  ; Works! Calls Vehicle_Start
```

#### Constructor Syntax

Create objects with constructor arguments:

```blitz
Class Player
    Field name$

    Method New(n$)
        Self\name$ = n$
    End Method
End Class

Local p.Player = New Player("Hero")
```

#### Static Methods

```blitz
Class MathUtils
    Static Method Square%(n%)
        Return n% * n%
    End Method
End Class

Print MathUtils_Square(5)  ; 25
```

## Runtime

### Graphics

- Apps are now DPI "aware." Scale your windows & UIs based on user DPI using DPIScaleX()/DPIScaleY().
- FreeImage upgraded to version 3.17 with support for more file formats.
- OpenGL is now the primary graphics backend (cross-platform).
- DirectX backend has been removed for a cleaner, cross-platform codebase.

### Input

- SDL2-based input system (cross-platform).

### Networking

- DirectPlay has been removed.

### System

- Added ScreenWidth & ScreenHeight for getting the desktop dimensions.
- Cross-platform support: Linux, macOS, Windows.

## IDE

- DPI "aware" with automatic scaling.
- `$BLITZPATH/tmp` is automatically created if it does not exist.
- Improved indentation handling.
- Added BB icon to executable & window.
- Fixed pasting junk data.
- 64-bit build available.

## Debugger

- DPI "aware" with automatic scaling.
- Added BB rocket icon to executable & window.

## Compiler

- LLVM-based backend for optimized code generation.
- Cross-platform compilation support.
- Removed legacy x86 assembly dependencies.
