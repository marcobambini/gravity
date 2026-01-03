//
//  main.swift
//  
//
//  Created by v.prusakov on 6/1/22.
//

import Gravity
import Foundation

class GVMDelegateImpl: GravityVirtualMachineDelegate {
    func virtualMachine(
        _ virtualMachine: Gravity.GravityVirtualMachine,
        didErrorWith message: String,
        errorType: error_type_t,
        errorDescription: error_desc_t
    ) {

    }
    
    func virtualMachine(_ virtualMachine: Gravity.GravityVirtualMachine, didGetValueFrom target: Gravity.GSValue, forKey key: String) throws -> Gravity.GSValue? {
        return nil
    }

    func virtualMachine(_ virtualMachine: Gravity.GravityVirtualMachine, didGetUndefValueFrom target: Gravity.GSValue, forKey key: String) throws -> Gravity.GSValue? {
        return nil
    }

    func virtualMachineLoadFile(
        _ virtualMachine: GravityVirtualMachine,
        file: String,
        fileId: inout UInt32,
        isStatic: inout Bool
    ) -> String? {
        guard let path = Bundle.module.path(forResource: file, ofType: "gravity") else {
            return nil
        }
        let content = try? String(contentsOfFile: path)
        return content
    }
    
    func virtualMachine(_ virtualMachine: Gravity.GravityVirtualMachine, didSetUndefValue value: Gravity.GSValue, in target: Gravity.GSValue, forKey key: String) -> Bool {
        print("didSetUndefValue", value, target, key)
        return false
    }
    
    func virtualMachine(
        _ virtualMachine: Gravity.GravityVirtualMachine,
        xdata: UnsafeMutableRawPointer?,
        didGetUndefValueFrom target: Gravity.GSValue,
        forKey key: String
    ) throws -> Gravity.GSValue? {
        print("didGetUndefValueFrom", target, key)
        return nil
    }
    
    func virtualMachineDidReciveLog(_ virtualMachine: GravityVirtualMachine, message: String) {
        print("virtualMachineDidReciveLog", message)
    }
    
    func virtualMachineDidClearLog(_ virtualMachine: GravityVirtualMachine) {
        print("claer")
    }
    
    func virtualMachineBridgeEquals(_ virtualMachine: GravityVirtualMachine, lhsValue: GSValue, rhsValue: GSValue) -> Bool {
        print("virtualMachineBridgeEquals", lhsValue, rhsValue)
        return lhsValue == rhsValue
    }
    
    func virtalMachine(_ virtualMachine: GravityVirtualMachine, didRequestCloneFor object: GSValue) -> GSValue {
        print("didRequestCloneFor", object)
        return object
    }
    
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didExecuteIn ctx: GSValue,
        arguments: [GSValue],
        argumentsCount: Int16,
        vIndex: UInt32
    ) -> Bool {
        print("didExecuteIn", ctx, arguments, argumentsCount, vIndex)
        return false
    }
    
    func virtualMachine(
        _ virtualMachine: GravityVirtualMachine,
        didSetValue value: GSValue,
        in target: GSValue,
        forKey key: String
    ) -> Bool {
        print("didSetValue", value, target, key)
        return false
    }
    
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestStringWith length: UInt32) -> String {
        print("didRequestStringWith", length)
        return ""
    }
    
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestFree object: GSValue) {
        print("didRequestFree", object)
        return
    }
    
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didRequestSizeFor object: GSValue) -> UInt32 {
        print("didRequestSizeFor", object)
        return 1
    }
    
    func virtualMachine(_ virtualMachine: GravityVirtualMachine, didInitObjectIn ctx: GSValue, instance: UnsafeMutablePointer<gravity_instance_t>?, arguments: [GSValue], argumentsCount: Int16) -> Bool {
        print("didInitObjectIn", ctx, instance, arguments, argumentsCount)
        return false
    }
}

let vmDelegate = GVMDelegateImpl()

let settings = GravityVirtualMachine.Settings(
    reportNullErrors: true,
    disableGarbageCollectorCheck: false
)

let sourceCodePath = Bundle.module.path(forResource: "main", ofType: "gravity")!
let sourceCode = try String(contentsOfFile: sourceCodePath)

let vm = GravityVirtualMachine(settings: settings, delegate: vmDelegate)

@GSExportable
class SwiftObject: @unchecked Sendable {
    init() {
        print("SwiftObject Init")
    }
    
    var text: String = "kek"
    
    var random: String {
        return "Random string"
    }
    
    func printKek() {
        print(#function, text)
    }
    
    func debug(_ value: Int) -> String {
        return "Debug value is \(value)"
    }
}

struct SwiftStruct: GSExportable {
    init() {
        print("SwiftStruct Init")
    }

    var text: String = "text" {
        didSet {
            print("Did set text", text)
        }
    }

    func printText() {
        print(Self.self, #function, text)
    }

    static func export(in encoder: GravityExportEncoder) throws {
        let container = try encoder.makeContainer(for: SwiftStruct.self)
        container.bind(.constructor(Self.init))
        container.bind(.property(\Self.text, named: "text"))
        container.bind(.method(self.printText, named: "printText"))
    }
}

try! vm.bindClass(with: SwiftObject.self)
try! vm.bindClass(with: SwiftStruct.self)

vm.setValue(SwiftObject(), forKey: "sw")

let binary = vm.loadGravityFile(from: sourceCode)
if let res = vm.execute(binary) {
    print(res)
}

