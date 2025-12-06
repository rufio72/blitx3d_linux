# Object-Oriented Programming (OOP) in Blitz3D-NG

Blitz3D-NG extends the original Blitz3D language with full object-oriented programming support. This document describes all OOP features available in the language.

## Table of Contents

- [Classes](#classes)
- [Fields](#fields)
- [Methods](#methods)
- [Constructors](#constructors)
- [Self Keyword](#self-keyword)
- [Inheritance](#inheritance)
- [Super Keyword](#super-keyword)
- [Static Methods](#static-methods)
- [Access Modifiers](#access-modifiers)

---

## Classes

Classes are defined using the `Class...End Class` block. A class encapsulates data (fields) and behavior (methods).

```blitz
Class Player
    Field name$
    Field health%
    Field x#, y#
End Class
```

### Creating Instances

Use `New` to create an instance of a class:

```blitz
Local p.Player = New Player
p\name$ = "Hero"
p\health% = 100
```

---

## Fields

Fields are class member variables. They store the state of an object.

```blitz
Class Vector3
    Field x#
    Field y#
    Field z#
End Class

Local v.Vector3 = New Vector3
v\x# = 1.0
v\y# = 2.0
v\z# = 3.0
```

Multiple fields can be declared on one line:

```blitz
Class Point
    Field x#, y#, z#
End Class
```

---

## Methods

Methods are functions that belong to a class. They operate on the instance data.

```blitz
Class Counter
    Field value%

    Method Increment()
        Self\value% = Self\value% + 1
    End Method

    Method GetValue%()
        Return Self\value%
    End Method
End Class

Local c.Counter = New Counter
c\Increment()
c\Increment()
Print c\GetValue()  ; Prints: 2
```

### Method Return Types

Methods can return values using type tags:

```blitz
Class Calculator
    Method Add%(a%, b%)
        Return a% + b%
    End Method

    Method Multiply#(a#, b#)
        Return a# * b#
    End Method

    Method GetName$()
        Return "Calculator"
    End Method
End Class
```

---

## Constructors

The constructor is a special method named `New`. It is called automatically when an object is created.

```blitz
Class Enemy
    Field name$
    Field health%

    Method New()
        name$ = "Unknown"
        health% = 100
        Print "Enemy created!"
    End Method
End Class

Local e.Enemy = New Enemy  ; Prints: "Enemy created!"
Print e\name$              ; Prints: "Unknown"
```

---

## Self Keyword

The `Self` keyword refers to the current object instance. Use it to access the object's fields and methods from within a method.

```blitz
Class Player
    Field score%

    Method AddScore(points%)
        Self\score% = Self\score% + points%
    End Method

    Method DoubleScore()
        Self\AddScore(Self\score%)  ; Call another method on Self
    End Method
End Class
```

---

## Inheritance

Classes can inherit from other classes using the `Extends` keyword. The child class inherits all fields and methods from the parent class.

```blitz
Class Animal
    Field name$

    Method Speak$()
        Return "..."
    End Method
End Class

Class Dog Extends Animal
    Method Speak$()
        Return "Woof!"
    End Method
End Class

Class Cat Extends Animal
    Method Speak$()
        Return "Meow!"
    End Method
End Class

Local d.Dog = New Dog
d\name$ = "Rex"
Print d\name$ + " says: " + d\Speak()  ; Prints: "Rex says: Woof!"

Local c.Cat = New Cat
c\name$ = "Whiskers"
Print c\name$ + " says: " + c\Speak()  ; Prints: "Whiskers says: Meow!"
```

### Inherited Fields

Child classes have access to all parent fields:

```blitz
Class Vehicle
    Field speed%
    Field fuel#
End Class

Class Car Extends Vehicle
    Field wheels%

    Method New()
        wheels% = 4
        speed% = 0      ; Inherited from Vehicle
        fuel# = 100.0   ; Inherited from Vehicle
    End Method
End Class
```

---

## Super Keyword

The `Super` keyword allows calling parent class methods from within a child class. This is useful for extending parent behavior rather than completely replacing it.

```blitz
Class Animal
    Field name$

    Method Describe$()
        Return "Animal: " + name$
    End Method
End Class

Class Dog Extends Animal
    Field breed$

    Method Describe$()
        ; Call parent's Describe first, then add more info
        Return Super\Describe() + ", Breed: " + breed$
    End Method
End Class

Local d.Dog = New Dog
d\name$ = "Rex"
d\breed$ = "German Shepherd"
Print d\Describe()  ; Prints: "Animal: Rex, Breed: German Shepherd"
```

---

## Static Methods

Static methods belong to the class itself, not to any particular instance. They cannot access instance fields (no `Self`).

```blitz
Class MathUtils
    Static Method Max%(a%, b%)
        If a% > b% Then Return a%
        Return b%
    End Method

    Static Method Min%(a%, b%)
        If a% < b% Then Return a%
        Return b%
    End Method
End Class

; Call static methods without creating an instance
Print MathUtils_Max(10, 20)  ; Prints: 20
Print MathUtils_Min(10, 20)  ; Prints: 10
```

**Note:** Static methods are called using the syntax `ClassName_MethodName()`, not with the `\` operator.

---

## Access Modifiers

Access modifiers control the visibility of fields and methods. Blitz3D-NG supports three access levels:

| Modifier | Description |
|----------|-------------|
| **Public** | Accessible from anywhere (default) |
| **Private** | Accessible only within the same class |
| **Protected** | Accessible within the same class and subclasses |

### Public (Default)

Fields and methods are public by default. They can be accessed from anywhere.

```blitz
Class Player
    Field name$          ; Public by default
    Public Field score%  ; Explicitly public

    Method GetName$()    ; Public by default
        Return name$
    End Method
End Class

Local p.Player = New Player
p\name$ = "Hero"         ; OK - public field
p\score% = 100           ; OK - public field
Print p\GetName()        ; OK - public method
```

### Private

Private members can only be accessed from within the same class.

```blitz
Class BankAccount
    Private Field balance#
    Private Field pin%

    Method New()
        balance# = 0.0
        pin% = 1234
    End Method

    Private Method ValidatePin%(inputPin%)
        Return inputPin% = pin%
    End Method

    Method Deposit(amount#)
        balance# = balance# + amount#
    End Method

    Method Withdraw#(amount#, inputPin%)
        If Self\ValidatePin(inputPin%) Then
            If amount# <= balance# Then
                balance# = balance# - amount#
                Return amount#
            EndIf
        EndIf
        Return 0.0
    End Method

    Method GetBalance#()
        Return balance#
    End Method
End Class

Local account.BankAccount = New BankAccount
account\Deposit(1000.0)
Print account\GetBalance()              ; OK - public method

; These would cause compile errors:
; account\balance# = 999999            ; ERROR: Cannot access private field
; account\ValidatePin(1234)            ; ERROR: Cannot access private method
```

### Protected

Protected members can be accessed from the same class or any subclass, but not from outside.

```blitz
Class Animal
    Protected Field name$
    Private Field id%

    Protected Method MakeSound$()
        Return "..."
    End Method

    Method Describe$()
        Return "Animal: " + name$
    End Method
End Class

Class Dog Extends Animal
    Method SetName(n$)
        Self\name$ = n$         ; OK - protected accessible from subclass
        ; Self\id% = 123        ; ERROR - private not accessible
    End Method

    Method Bark$()
        Return Self\MakeSound() ; OK - protected method accessible from subclass
    End Method
End Class

Local d.Dog = New Dog
d\SetName("Rex")
Print d\Describe()

; These would cause compile errors:
; d\name$ = "Buddy"              ; ERROR: Cannot access protected field
; d\MakeSound()                  ; ERROR: Cannot access protected method
```

### Access Modifier Summary

```blitz
Class Parent
    Private Field privateField%      ; Only in Parent
    Protected Field protectedField%  ; Parent and children
    Public Field publicField%        ; Everywhere (or just "Field")

    Private Method PrivateMethod()   ; Only in Parent
    End Method

    Protected Method ProtectedMethod() ; Parent and children
    End Method

    Public Method PublicMethod()     ; Everywhere (or just "Method")
    End Method
End Class

Class Child Extends Parent
    Method Test()
        ; Self\privateField% = 1     ; ERROR - private
        Self\protectedField% = 2     ; OK - protected
        Self\publicField% = 3        ; OK - public

        ; Self\PrivateMethod()       ; ERROR - private
        Self\ProtectedMethod()       ; OK - protected
        Self\PublicMethod()          ; OK - public
    End Method
End Class

; From outside:
Local p.Parent = New Parent
; p\privateField% = 1               ; ERROR
; p\protectedField% = 2             ; ERROR
p\publicField% = 3                  ; OK
```

---

## Complete Example

Here's a complete example demonstrating all OOP features:

```blitz
; Base class for all game entities
Class Entity
    Protected Field x#, y#
    Protected Field active%

    Method New()
        x# = 0.0
        y# = 0.0
        active% = True
    End Method

    Method MoveTo(newX#, newY#)
        Self\x# = newX#
        Self\y# = newY#
    End Method

    Method GetPosition$()
        Return "(" + x# + ", " + y# + ")"
    End Method

    Protected Method Update()
        ; Base update logic
    End Method
End Class

; Player class extends Entity
Class Player Extends Entity
    Private Field health%
    Private Field maxHealth%
    Field name$

    Method New()
        health% = 100
        maxHealth% = 100
        name$ = "Player"
    End Method

    Method TakeDamage(amount%)
        health% = health% - amount%
        If health% < 0 Then health% = 0
    End Method

    Method Heal(amount%)
        health% = health% + amount%
        If health% > maxHealth% Then health% = maxHealth%
    End Method

    Method GetHealth%()
        Return health%
    End Method

    Method IsAlive%()
        Return health% > 0
    End Method

    Method Describe$()
        Return name$ + " at " + Super\GetPosition() + " HP: " + health%
    End Method
End Class

; Enemy class extends Entity
Class Enemy Extends Entity
    Protected Field damage%

    Method New()
        damage% = 10
    End Method

    Method Attack.Player(target.Player)
        If Self\active% And target\IsAlive() Then
            target\TakeDamage(Self\damage%)
        EndIf
    End Method
End Class

; Boss is a special enemy
Class Boss Extends Enemy
    Method New()
        damage% = 25  ; Bosses deal more damage
    End Method

    Method Attack.Player(target.Player)
        ; Boss attacks twice!
        Super\Attack(target)
        Super\Attack(target)
    End Method
End Class

; Main game code
Local player.Player = New Player
player\name$ = "Hero"
player\MoveTo(100.0, 50.0)

Local enemy.Enemy = New Enemy
Local boss.Boss = New Boss

Print player\Describe()  ; Hero at (100.0, 50.0) HP: 100

enemy\Attack(player)
Print "After enemy attack: HP = " + player\GetHealth()  ; 90

boss\Attack(player)
Print "After boss attack: HP = " + player\GetHealth()   ; 40

player\Heal(30)
Print "After healing: HP = " + player\GetHealth()       ; 70

End
```

---

## Summary of OOP Keywords

| Keyword | Description |
|---------|-------------|
| `Class` | Defines a new class |
| `End Class` | Ends a class definition |
| `Field` | Declares a class field (member variable) |
| `Method` | Declares a class method (member function) |
| `End Method` | Ends a method definition |
| `Self` | Reference to the current object instance |
| `Super` | Reference to the parent class (for calling parent methods) |
| `Extends` | Specifies class inheritance |
| `Static` | Declares a static method (class-level, no instance needed) |
| `Private` | Access modifier: same class only |
| `Protected` | Access modifier: same class and subclasses |
| `Public` | Access modifier: accessible from anywhere (default) |
| `New` | Constructor method name / creates new instance |
