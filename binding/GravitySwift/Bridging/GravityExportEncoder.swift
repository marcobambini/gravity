//
//  GravityExportEncoder.swift
//  
//
//  Created by v.prusakov on 6/4/22.
//

import Foundation
import CGravity

public final class GravityExportEncoder {
    
    unowned let vm: GravityVirtualMachine
    
    private(set) var classDescriptors: [GravityBridgeClassDescriptor] = []
    
    init(vm: GravityVirtualMachine) {
        self.vm = vm
    }
    
    public func makeContainer<T: GSExportable>(for type: T.Type) throws -> GravityExportClassEncoderContainer {
        let clazz = self.vm.getOrRegisterClass(type)
        
        let classDescriptor = GravityBridgeClassDescriptor(
            vm: self.vm,
            registredName: T.runtimeName,
            gClass: clazz,
            type: type
        )
        
        // FIXME: xdata required to identify value as class. instead we get `closure` and value never call bridge_init
        let xdata = Unmanaged<GravityBridgeClassDescriptor>.passRetained(classDescriptor).toOpaque()
        gravity_class_setxdata(clazz, xdata)
        
        self.classDescriptors.append(classDescriptor)
        return _GravityExportClassEncoderContainer(descriptor: classDescriptor, type: T.self)
    }
}
