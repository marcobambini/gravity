//
//  GravityVirtualMachineDelegate.swift
//  
//
//  Created by v.prusakov on 6/4/22.
//

import Foundation
import CGravity

public protocol GravityVirtualMachineDelegate: AnyObject {

    /// Virtual Machine want to load file from import.
    /// If gravity file contains `#include` macro, than Gravity compiler try to load that file using that method.
    ///
    /// - Parameter virtualMachine: The virtual machine instance.
    /// - Parameter file: File name of imported file.
    /// - Parameter fileId: File Descriptor.
    /// - Returns: Returns source code of loaded file.
    func virtualMachineLoadFile(
        _ virtualMachine: GravityVirtualMachine,
        file: String,
        fileId: inout UInt32,
        isStatic: inout Bool
    ) -> String?

    /// Virtual Machine notify about critical error.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didErrorWith message: String,
        errorType: error_type_t,
        errorDescription: error_desc_t
    )

    // MARK: Bridge

    /// Virtual Machine did recive log.
    func virtualMachineDidReciveLog(_ virtualMachine: GravityVirtualMachine, message: String)

    /// Virtual Machine required to clear log.
    func virtualMachineDidClearLog(_ virtualMachine: GravityVirtualMachine)

    /// Virtual Machine try to equals to values.
    func virtualMachineBridgeEquals(_ virtualMachine: GravityVirtualMachine, lhsValue: GSValue, rhsValue: GSValue) -> Bool

    /// Virtual Machine try to execute callback or function.
    /// - Parameter virtualMachine: Current virtual machine
    /// - Parameter ctx: Context instance.
    /// - Parameter arguments: All passed arguments. First value is callee.
    /// - Parameter argumentsCount: Arguments count.
    /// - Parameter vIndex: index in register
    /// - Returns: true if you executed a value, or false if not.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didExecuteIn ctx: GSValue,
        arguments: [GSValue],
        argumentsCount: Int16,
        vIndex: UInt32
    ) -> Bool

    /// Virtual Machine try to set value from target.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didSetValue value: GSValue,
        in target: GSValue,
        forKey key: String
    ) -> Bool

    /// Virtual Machine try to get value from target.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didGetValueFrom target: GSValue,
        forKey key: String
    ) throws -> GSValue?

    /// Virtual Machine try to set undef value from target.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didSetUndefValue value: GSValue,
        in target: GSValue,
        forKey key: String
    ) -> Bool

    /// Virtual Machine try to get undef value from target.
    /// - Parameter virtualMachine: Current virtual machine
    /// - Parameter xdata: Instance object if exists
    /// - Parameter didGetUndefValueFrom: target
    /// - Parameter forKey: identifier key
    /// - Parameter vIndex: index in register
    /// - Returns: true if you find a value, or false if not.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didGetUndefValueFrom target: GSValue,
        forKey key: String
    ) throws -> GSValue?
    
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestStringWith length: UInt32) -> String
}

// MARK: Memory managment

public protocol GravityMemoryControlVMDelegate: GravityVirtualMachineDelegate {

    /// Virtual Machine request to free memory of object.
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestFree object: GSValue)

    /// Virtual Machine request to memory size of object.
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestSizeFor object: GSValue) -> UInt32

    /// Virtual Machine did init native object.
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didInitObjectIn ctx: GSValue,
        instance:  UnsafeMutablePointer<gravity_instance_t>?,
        arguments: [GSValue],
        argumentsCount: Int16
    ) -> Bool

    /// Virtual Machine did request clone object.
    func virtalMachine(_ virtualMachine: GravityVirtualMachine, didRequestCloneFor object: GSValue) -> GSValue
}

extension GravityVirtualMachineDelegate {
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didErrorWith message: String,
        errorType: error_type_t,
        errorDescription: error_desc_t
    ) {
        print("Error!", message)
    }
}
