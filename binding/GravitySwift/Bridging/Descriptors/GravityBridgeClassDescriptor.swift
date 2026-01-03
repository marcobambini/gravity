//
//  GravityBridgeClassDescriptor.swift
//  
//
//  Created by v.prusakov on 6/4/22.
//

import Foundation
import CGravity

class GravityBridgeClassDescriptor {
    
    unowned let vm: GravityVirtualMachine
    let registredName: String
    let gClass: UnsafeMutablePointer<gravity_class_t>!
    let type: Any
    
    private(set) var methodDescriptors: [MethodDescriptor] = []
    private(set) var propertyDescripors: [PropertyDescriptor] = []
    
    internal init<T>(
        vm: GravityVirtualMachine,
        registredName: String,
        gClass: UnsafeMutablePointer<gravity_class_t>?,
        type: T.Type
    ) {
        self.vm = vm
        self.registredName = registredName
        self.gClass = gClass
        self.type = type
    }
    
    func addMethod(_ descriptor: MethodDescriptor) {
        self.methodDescriptors.append(descriptor)
    }
    
    func addProperty(_ descriptor: PropertyDescriptor) {
        self.propertyDescripors.append(descriptor)
    }
    
    deinit {
        // we should release methods retained in context to avoid leaks.
        for descriptor in methodDescriptors {
            Unmanaged.passRetained(descriptor).release()
        }
        
        // we should release methods retained in context to avoid leaks.
        for descriptor in propertyDescripors {
            Unmanaged.passRetained(descriptor).release()
        }
    }
}
