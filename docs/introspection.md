### Introspection

Type introspection is a core feature of Gravity. In Gravity, the Object class (ancestor of every class) provides methods for checking the instance's class.

All Objects now responds to the following methods:
```swift
	Object.introspection();
	Object.methods();
	Object.properties();
```

Each method support two optional parameters:
* param1 a Bool value (default false), if set to true returns extended information
* param2 a Bool value (default false), if set to true it scan super classes hierarchy
Result can be a List or a Map (in case of param1 set to true).

Examples:
```swift
  List.introspection();
  // returns: [sort,reduce,loop,sorted,contains,filter,count,reverse,iterate,push,remove,pop,storeat,loadat,reversed,indexOf,next,map,join]
  
  List.introspection(true);
  // returns: [sorted:[isvar:false,name:sorted],next:[isvar:false,name:next],sort:[isvar:false,name:sort],filter:[isvar:false,name:filter],storeat:[isvar:false,name:storeat],map:[isvar:false,name:map],indexOf:[isvar:false,name:indexOf],reversed:[isvar:false,name:reversed],contains:[isvar:false,name:contains],loop:[isvar:false,name:loop],reduce:[isvar:false,name:reduce],count:[isvar:true,name:count,readonly:true],loadat:[isvar:false,name:loadat],iterate:[isvar:false,name:iterate],remove:[isvar:false,name:remove]...
```
