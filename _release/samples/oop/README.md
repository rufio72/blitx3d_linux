# Blitz3D-NG OOP Samples

This directory contains sample programs demonstrating the Object-Oriented Programming features of Blitz3D-NG.

## Features Demonstrated

### test_super.bb
Demonstrates the `Super` keyword for calling parent class methods:
- Child class calling parent's method
- Using Super in method return values

### test_inheritance.bb
Basic inheritance example:
- Calling inherited methods on child class instances
- Field inheritance

### test_multi_inheritance.bb
Multi-level inheritance:
- Three-level class hierarchy (Animal -> Dog -> GermanShepherd)
- Method inheritance across multiple levels
- Method overriding

## Running the Samples

```bash
export blitzpath=/path/to/_release
blitzcc test_super.bb
blitzcc test_inheritance.bb
blitzcc test_multi_inheritance.bb
```

## OOP Quick Reference

### Defining a Class
```blitz
Class ClassName
    Field fieldName%
    Field anotherField$

    Method MethodName(param%)
        Self\fieldName% = param%
    End Method
End Class
```

### Inheritance
```blitz
Class ChildClass Extends ParentClass
    ; Inherits all fields and methods from ParentClass
End Class
```

### Creating Objects
```blitz
Local obj.ClassName = New ClassName()
```

### Calling Methods
```blitz
obj\MethodName(42)
result = obj\GetValue%()
```

### Using Super
```blitz
Class Child Extends Parent
    Method DoSomething()
        Super\DoSomething()  ; Call parent's method
    End Method
End Class
```

### Static Methods
```blitz
Class Utils
    Static Method Add%(a%, b%)
        Return a% + b%
    End Method
End Class

result = Utils_Add(5, 3)  ; Called as global function
```
