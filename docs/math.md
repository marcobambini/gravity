## Math
The Math class is a class in Gravity that offers various methods for calculating more complex mathematics than the standard +,-,/, and *.

### Mathematical Constants
```swift
	Math.PI;      // pi (~3.141593)
	Math.E;       // e (~2.718282)
	Math.LN2;     // natural log of 2 (ie. Math.log(2) = ~0.693147)
	Math.LN10;    // natural log of 10 (ie. Math.log(10) = ~2.302585)
	Math.LOG2E;   // log base 2 of e (~1.442695)
	Math.LOG10E;  // log base 10 of e (~0.434294)
	Math.SQRT2;   // sqrt of 2  (ie. Math.sqrt(2) = ~1.414214)
	Math.SQRT1_2; // sqrt of 0.5  (ie. Math.sqrt(0.5) = ~0.707107)
```

### Absolute Values
**Math.abs()** is a method that returns the absolute value of an integer of float.
```swift
	Math.abs(-10); // returns 10
	Math.abs(10);  // also returns 10
```

### Trig Functions
The Math class also contains several Trigonometric Functions. All values that represent angles are in radians for these methods, and they all expect radians for inputs. You can use the ".radians" and ".degrees" properties of the Int and Float class to do conversions.
```swift
	Math.acos(-1);     // returns pi
	Math.asin(0.5);    // returns 0.523599 ( = pi/6)
	Math.atan(1);      // returns 0.785398 ( = pi/4 )
	Math.atan2(-1,-1); // returns -2.356194 ( = -3pi/4 )

	Math.cos(Math.PI);     // returns -1
	Math.cos(180.radians); // returns -1 (same as above)
	Math.sin(Math.PI);     // returns 0
	Math.tan(Math.PI/4);   // returns 1
```

### Ceiling, Floor
```swift
	Math.ceil(4.1)  // returns 5
	Math.floor(4.1) // returns 4
```
### Rounding
Return number rounded to ndigits precision after the decimal point. If ndigits is omitted, it returns the nearest integer to its input. For Float values are rounded to the closest multiple of 10 to the power minus ndigits.
```swift
	Math.round(4.1) // returns 4
	Math.round(4.5) // returns 5
	Math.round(65.34634) // returns 65.0
	Math.round(65.34634,1) // returns 65.3
	Math.round(65.34634,2) // returns 65.35
```

### Exponents and Radicals
```swift
	// e to the power of x
	Math.exp(1) // returns 2.718282 (e)
	Math.exp(2) // returns 7.389056 (e^2)

	// x to the power of y
	Math.pow(2,3); // returns 8

	Math.sqrt(9); // 3
	Math.cbrt(8); // 2
	Math.xrt(4,16); // 2 (4th root of 16)
```

### Logarithms
```swift
	// log base e
	Math.log(Math.E) // returns 1
	Math.log10(10)   // returns 1
	Math.logx(2,2)   // returns 1
	Math.logx(2,4)   // returns 2
```

### Max and Min
```swift
	Math.max(-1,10,2); // Returns 10
	Math.min(-1,10,2); // Returns -1
```

### Random Number
```swift
	Math.random()      // Returns a random number between 0.0 and 1.0
	Math.random(N)     // Returns a random number between 0 and N (or between 0.0 and N.0 if N is Float)
	Math.random(N1,N2) // Returns a random number between N1 and N2 (they must be both Int or Float)
```

### GCF and LCM
```swift
	Math.gcf(12,15,21); // 3
	Math.lcm(6,15,2);   // 30
```
