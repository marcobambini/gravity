//
//  MethodDescription.swift
//  
//
//  Created by v.prusakov on 6/4/22.
//

import CGravity

/// Class describing method exported to Gravity Script.
public class MethodDescriptor {
    public let name: String
    public let argsCount: Int
    public let isStatic: Bool
    let callWrapper: (GSValue?, [GSValue]) -> Any?
    
    static let constructorName = "$init"
    
    public var isConstrucor: Bool {
        return self.name == Self.constructorName && self.isStatic
    }
    
    init(name: String, argsCount: Int, isStatic: Bool, callWrapper: @escaping (GSValue?, [GSValue]) -> Any?) {
        self.name = name
        self.argsCount = argsCount
        self.isStatic = isStatic
        self.callWrapper = callWrapper
    }
    
    // MARK: Methods
    
    public static func method<T: _GravityCompitable, R>(_ method: @escaping MethodNoArg<T, R>, named: String) -> MethodDescriptor {
        return MethodDescriptor(
            name: named,
            argsCount: 0,
            isStatic: false,
            callWrapper: { target, args -> Any? in
                return method(target!.toObjectOf(T.self)!)()
            }
        )
    }
    
    public static func method<T: _GravityCompitable, A: _GravityCompitable, R>(_ method: @escaping Method1Arg<T, A, R>, named: String) -> MethodDescriptor {
        MethodDescriptor(
            name: named,
            argsCount: 1,
            isStatic: false,
            callWrapper: { target, args -> Any? in
                return method(target!.toObjectOf(T.self)!)(
                    args[0].toObjectOf(A.self)!
                )
            }
        )
    }
    
    public static func method<T: _GravityCompitable, A: _GravityCompitable, B: _GravityCompitable, R>(_ method: @escaping Method2Arg<T, A, B, R>, named: String) -> MethodDescriptor {
        MethodDescriptor(
            name: named,
            argsCount: 2,
            isStatic: false,
            callWrapper: { target, args -> Any? in
                return method(target!.toObjectOf(T.self)!)(
                    args[0].toObjectOf(A.self)!,
                    args[1].toObjectOf(B.self)!
                )
            }
        )
    }
    
    public static func method<T: _GravityCompitable, A: _GravityCompitable, B: _GravityCompitable, C: _GravityCompitable, R>(_ method: @escaping Method3Arg<T, A, B, C, R>, named: String) -> MethodDescriptor {
        MethodDescriptor(
            name: named,
            argsCount: 3,
            isStatic: false,
            callWrapper: { target, args -> Any? in
                return method(target!.toObjectOf(T.self)!)(
                    args[0].toObjectOf(A.self)!,
                    args[1].toObjectOf(B.self)!,
                    args[2].toObjectOf(C.self)!
                )
            }
        )
    }
    
    public static func method<T: _GravityCompitable, A: _GravityCompitable, B: _GravityCompitable, C: _GravityCompitable, D: _GravityCompitable, R>(_ method: @escaping Method4Arg<T, A, B, C, D, R>, named: String) -> MethodDescriptor {
        MethodDescriptor(
            name: named,
            argsCount: 4,
            isStatic: false,
            callWrapper: { target, args -> Any? in
                return method(target!.toObjectOf(T.self)!)(
                    args[0].toObjectOf(A.self)!,
                    args[1].toObjectOf(B.self)!,
                    args[2].toObjectOf(C.self)!,
                    args[3].toObjectOf(D.self)!
                )
            }
        )
    }
    
    // MARK: Constructors
    
    public static func constructor<T>(_ constructor: @escaping StaticMethodNoArgs<T>) -> MethodDescriptor {
        MethodDescriptor(
            name: Self.constructorName,
            argsCount: 0,
            isStatic: true,
            callWrapper: { _, _ -> Any? in
                // TODO: Looks like leak
                return constructor()
            }
        )
    }
    
    public static func constructor<T, A: _GravityCompitable>(_ constructor: @escaping StaticMethod1Arg<T, A>) -> MethodDescriptor {
        MethodDescriptor(
            name: Self.constructorName,
            argsCount: 1,
            isStatic: true,
            callWrapper: { _, args -> Any? in
                return constructor(args[0].toObjectOf(A.self)!)
            }
        )
    }
    
    // MARK: Static Methods
    
    public static func staticMethod<T>(_ method: @escaping StaticMethodNoArgs<T>, named name: String) -> MethodDescriptor {
        MethodDescriptor(
            name: name,
            argsCount: 1,
            isStatic: true,
            callWrapper: { _, args -> Any? in
                return method()
            }
        )
    }
    
    public static func staticMethod<T, A: _GravityCompitable>(_ method: @escaping StaticMethod1Arg<T, A>, named name: String) -> MethodDescriptor {
        MethodDescriptor(
            name: name,
            argsCount: 1,
            isStatic: true,
            callWrapper: { _, args -> Any? in
                return method(args[0].toObjectOf(A.self)!)
            }
        )
    }
    
    public func call(in target: GSValue?, with args: [GSValue]) -> Any? {
        return callWrapper(target, args)
    }
    
    public func callStatic(with args: [GSValue]) -> Any? {
        return callWrapper(nil, args)
    }
}

public typealias StaticMethodNoArgs<T> = () -> T

public typealias StaticMethod1Arg<T, A> = (A) -> T

public typealias StaticMethod2Arg<T, A, B> = (A, B) -> T

public typealias StaticMethod3Arg<T, A, C, B> = (A, B, C) -> T

public typealias StaticMethod4Arg<T, A, C, D, B> = (A, B, C, D) -> T

public typealias StaticMethod5Arg<T, A, C, D, E, B> = (A, B, C, D, E) -> T

public typealias StaticMethod6Arg<T, A, C, D, E, F, B> = (A, B, C, D, E, F) -> T

public typealias MethodWithArgs<T, each A, R> = (T) -> (repeat each A) -> R

public typealias MethodNoArg<T, R> = (T) -> () -> R

public typealias Method1Arg<T, A, R> = (T) -> (A) -> R

public typealias Method2Arg<T, A, B, R> = (T) -> (A, B) -> R

public typealias Method3Arg<T, A, B, C, R> = (T) -> (A, B, C) -> R

public typealias Method4Arg<T, A, B, C, D, R> = (T) -> (A, B, C, D) -> R

public typealias Method5Arg<T, A, B, C, D, E, R> = (T) -> (A, B, C, D, E) -> R

public typealias Method6Arg<T, A, B, C, D, E, F, R> = (T) -> (A, B, C, D, E, F) -> R
