//
//  PropertyDescriptor.swift
//  
//
//  Created by v.prusakov on 6/7/22.
//

import CGravity

/// Contains setter and getter logic for Swift properties.
public final class PropertyDescriptor {
    let name: String
    private let getter: (GSValue) -> GSValue // target -> value
    private let setter: ((GSValue, GSValue) -> Void)? // (target, newValue) -> Void
    
    var isReadonly: Bool {
        return self.setter == nil
    }
    
    internal init(name: String, getter: @escaping (GSValue) -> GSValue, setter: ((GSValue, GSValue) -> Void)?) {
        self.name = name
        self.getter = getter
        self.setter = setter
    }
    
    public static func property<T, A>(_ keyPath: ReferenceWritableKeyPath<T, A>, named name: String) -> PropertyDescriptor {
        PropertyDescriptor(
            name: name,
            getter: { target in
                let value = (target.toObjectOf(T.self))?[keyPath: keyPath]
                return GSValue(object: value, in: target.vm)
            },
            setter: { target, value in
                // we use var because we know that target always contains ref type.
                target.toObjectOf(T.self)?[keyPath: keyPath] = value.toObjectOf(A.self)!
            }
        )
    }
    
    public static func property<T, A>(_ keyPath: KeyPath<T, A>, named name: String) -> PropertyDescriptor {
        PropertyDescriptor(
            name: name,
            getter: { target in
                let value = (target.toObjectOf(T.self))?[keyPath: keyPath]
                return GSValue(object: value, in: target.vm)
            },
            setter: nil
        )
    }

    public static func property<T, A>(_ keyPath: WritableKeyPath<T, A>, named name: String) -> PropertyDescriptor {
        PropertyDescriptor(
            name: name,
            getter: { target in
                let value = (target.toObjectOf(T.self))?[keyPath: keyPath]
                return GSValue(object: value, in: target.vm)
            },
            setter: { target, value in
                // we use var because we know that target always contains ref type.
                target.mutableScope(of: T.self) {
                    $0[keyPath: keyPath] = value.toObjectOf(A.self)!
                }
            }
        )
    }

    // MARK: - Internals
    
    func setValue(_ newValue: GSValue, in target: GSValue) {
        assert(!self.isReadonly, "We cannot set value to read only property.")
        
        self.setter?(target, newValue)
    }
    
    func getValue(in target: GSValue) -> GSValue {
        return self.getter(target)
    }
}
