//
//  GravityCompiler.swift
//  
//
//  Created by v.prusakov on 6/2/22.
//

import CGravity

/// Collection on data required for Gravity virtual machine.
public struct GravityBinary {
    let binary: UnsafeMutablePointer<gravity_closure_t>!
    let compiler: GravityCompiler
}

/// Gravity Compiler
public final class GravityCompiler {

    internal private(set) var compiler: OpaquePointer

    init(delegate: UnsafeMutablePointer<gravity_delegate_t>) {
        self.compiler = gravity_compiler_create(delegate)
    }
    
    deinit {
        gravity_compiler_free(self.compiler)
    }
    
    public func compile(source: String, debug: Bool = true) -> GravityBinary {
        let closure = source.withCString { sourcePtr in
            return gravity_compiler_run(
                self.compiler,
                sourcePtr,
                source.count,
                0, // fileId
                true, // is_static
                debug // add_debug
            )
        }

        return GravityBinary(binary: closure, compiler: self)
    }
    
    func transferMem(to vm: GravityVirtualMachine) {
        gravity_compiler_transfer(self.compiler, vm.vmPtr)
    }
}
