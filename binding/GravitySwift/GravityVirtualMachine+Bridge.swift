//
//  GravityVirtualMachine+Bridge.swift
//  Gravity
//
//  Created by Vladislav Prusakov on 04.01.2026.
//

import CGravity

func bridgeOptionalClasses(_ xdata: UnsafeMutableRawPointer?) -> UnsafeMutablePointer<UnsafePointer<Int8>?>? {
    guard let xdata, let vm = Unmanaged<AnyObject>.fromOpaque(xdata).takeUnretainedValue() as? GravityVirtualMachine else {
        return nil
    }

    return vm.optionalClassNamesPointer()
}

func logCallback(_ vmPointer: OpaquePointer?, message: UnsafePointer<CChar>?, xdata: UnsafeMutableRawPointer?) {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }
    vm.delegate.virtualMachineDidReciveLog(vm, message: String(cString: message!))
}

func logClear(_ vmPointer: OpaquePointer?, xdata: UnsafeMutableRawPointer?) {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }
    vm.delegate.virtualMachineDidClearLog(vm)
}

func errorCallback(
    _ vmPointer: OpaquePointer?,
    errType: error_type_t,
    message: UnsafePointer<CChar>!,
    errDesc: error_desc_t,
    xdata: UnsafeMutableRawPointer?
) {
    guard let xdata, let vm = Unmanaged<AnyObject>.fromOpaque(xdata).takeUnretainedValue() as? GravityVirtualMachine else {
        return
    }
    vm.delegate.virtualMachine(vm, didErrorWith: String(cString: message), errorType: errType, errorDescription: errDesc)
}

func bridgeFree(_ vmPointer: OpaquePointer?, objptr: UnsafeMutablePointer<gravity_object_t>?) {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }
    let value = GSValue(object: objptr, in: vm)

    if let delegate = vm.delegate as? GravityMemoryControlVMDelegate {
        delegate.virtualMachine(vm, didRequestFree: value)
        return
    }

    if let xData = value.xData {
        Unmanaged<AnyObject>.fromOpaque(xData).release()
    }
}

func bridgeSize(_ vmPointer: OpaquePointer?, objptr: UnsafeMutablePointer<gravity_object_t>?) -> UInt32 {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }
    let value = GSValue(object: objptr, in: vm)
    if let delegate = vm.delegate as? GravityMemoryControlVMDelegate {
        return delegate.virtualMachine(vm, didRequestSizeFor: value)
    }
    return 0
}

func bridgeClone(
    vmPointer: OpaquePointer!,
    objptr: UnsafeMutableRawPointer?
) -> UnsafeMutableRawPointer? {
    guard let vm = GravityVirtualMachine.getVM(vmPointer) else { fatalError("Cannot found Virtual Machine") }
    let value = GSValue(object: objptr, in: vm)

    if let delegate = vm.delegate as? GravityMemoryControlVMDelegate {
        return delegate.virtalMachine(vm, didRequestCloneFor: value).xData
    }

    return objptr
}

/// get description from object xdata
func bridgeString(
    vmPointer: OpaquePointer!,
    xdata: UnsafeMutableRawPointer?,
    length: UnsafeMutablePointer<UInt32>?
) -> UnsafePointer<CChar>? {
    guard let vm = GravityVirtualMachine.getVM(vmPointer) else { fatalError("Cannot found Virtual Machine") }
    let string = vm.delegate.virtualMachine(vm, didRequestStringWith: length!.pointee)
    length?.pointee = UInt32(string.utf8.count)
    return string.toPointer()
}

func bridgeEquals(
    vmPointer: OpaquePointer!,
    lhsPtr: UnsafeMutableRawPointer?,
    rhsPtr: UnsafeMutableRawPointer?
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer) else { fatalError("Cannot found Virtual Machine") }
    return vm.delegate.virtualMachineBridgeEquals(vm, lhsValue: GSValue(object: lhsPtr, in: vm), rhsValue: GSValue(object: lhsPtr, in: vm))
}

func bridgeInitInstance(
    _ vmPointer: OpaquePointer!,
    xdata: UnsafeMutableRawPointer?,
    ctx: gravity_value_t,
    instance: UnsafeMutablePointer<gravity_instance_t>?,
    args: UnsafeMutablePointer<gravity_value_t>?,
    argsCount: Int16
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer) else { fatalError("Cannot found Virtual Machine") }

    // first arg
    let arguments: [GSValue] = (1..<argsCount).map { index in
        let arg = args![Int(index)]
        return GSValue(object: arg, in: vm)
    }

    if let delegate = vm.delegate as? GravityMemoryControlVMDelegate {
        let context = GSValue(value: ctx, in: vm)
        return delegate.virtualMachine(
            vm,
            didInitObjectIn: context,
            instance: instance,
            arguments: arguments,
            argumentsCount: argsCount
        )
    }

    let method = Unmanaged<MethodDescriptor>.fromOpaque(xdata!).takeUnretainedValue()

    if arguments.count > method.argsCount {
        return GravityReturn.error("Passed more arguments, then expected. Method \(method.name) expected \(method.argsCount) arguments, but passed \(arguments.count) arguments.", vm: vm)
    }

    guard let object = method.callStatic(with: arguments) as? AnyObject else {
        return GravityReturn.error("Return value of init supports only reference types.", vm: vm)
    }

    let value = Unmanaged.passRetained(_ValueBox(value: object)).toOpaque()
    gravity_instance_setxdata(instance, value)

    return GravityReturn.noValue()
}

func bridgeExecute(
    vmPointer: OpaquePointer?,
    data: UnsafeMutableRawPointer?, // always return method descriptor
    ctx: gravity_value_t,
    args: UnsafeMutablePointer<gravity_value_t>!,
    argsCount: Int16,
    rIndex: UInt32
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }

    guard let methodDescRef = data else {
        return GravityReturn.error("Required xdata not passed", vm: vm)
    }

    var arguments: [GSValue] = (0..<argsCount).map { index in
        let value = args[Int(index)]
        return GSValue(object: value, in: vm)
    }

    // First value always contains instance
    let callee = arguments.removeFirst()
    let method = Unmanaged<MethodDescriptor>.fromOpaque(methodDescRef).takeUnretainedValue()

    if arguments.count > method.argsCount {
        return GravityReturn.error("Passed more arguments, then expected. Method \(method.name) expected \(method.argsCount) arguments, but passed \(arguments.count) arguments.", vm: vm)
    }

    if callee.xData == nil {
        return GravityReturn.error("Instance don't have any ref to allocated object.", vm: vm)
    }

    let value = method.call(in: callee, with: arguments)

    if value is Void {
        return GravityReturn.noValue()
    } else {
        return GravityReturn.value(GSValue(object: value, in: vm), rIndex: Int32(rIndex), vm: vm)
    }
}

func bridgeLoadFileCallback(
    _ file: UnsafePointer<CChar>?,
    _ size: UnsafeMutablePointer<size_t>?,
    _ fileId: UnsafeMutablePointer<UInt32>?,
    _ xdata: UnsafeMutableRawPointer?,
    _ isStatic: UnsafeMutablePointer<Bool>?
) -> UnsafePointer<CChar>? {
    guard let xdata, let vm = Unmanaged<AnyObject>.fromOpaque(xdata).takeUnretainedValue() as? GravityVirtualMachine else {
        return nil
    }

    guard let file, let size, let fileId, let isStatic else {
        return nil
    }

    // this callback is called each time an import statement is parsed
    // file arg represents what user wrote after the import keyword, for example:
    // import "file2"
    // import "file2.gravity"
    // import "../file2"
    // import "/full_path_to_file2"

    // it is callback's responsibility to resolve file path based on current working directory
    // or based on user defined search paths
    // and returns:
    // size of file in *size
    // fileid (if any) in *fileid
    // content of file as return value of the function

    // fileid will then be used each time an error is reported by the compiler
    // so it is responsibility of this function to map somewhere the association
    // between fileid and real file/path name

    // fileid is not used in this example
    // xdata not used here but it the xdata field set in the delegate
    // please note than in this simple example the imported file must be
    // in the same folder as the main input file

    guard let source = vm.delegate.virtualMachineLoadFile(
        vm,
        file: String(cString: file),
        fileId: &fileId.pointee,
        isStatic: &isStatic.pointee
    ) else {
        return nil
    }

    size.pointee = source.count
    return source.toPointer()
}


func bridgeSetValue(
    vmPointer: OpaquePointer!,
    xdata: UnsafeMutableRawPointer?,
    target: gravity_value_t,
    key: UnsafePointer<CChar>?,
    value: gravity_value_t
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }

    guard let xdata = xdata else {
        return GravityReturn.error("Extra data for bridging not passed!", vm: vm)
    }

    let target = GSValue(value: target, in: vm)
    let newValue = GSValue(value: value, in: vm)

    let propertyDescriptor = Unmanaged<PropertyDescriptor>.fromOpaque(xdata).takeUnretainedValue()

    if propertyDescriptor.isReadonly {
        return GravityReturn.error("Unexpected calling setter in readonly property!", vm: vm)
    }

    propertyDescriptor.setValue(newValue, in: target)

    return GravityReturn.noValue()
}

func bridgeSetUndefValue(
    vmPointer: OpaquePointer!,
    xdata: UnsafeMutableRawPointer?,
    target: gravity_value_t,
    key: UnsafePointer<CChar>?,
    value: gravity_value_t
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }

    let value = GSValue(object: value, in: vm)
    let target = GSValue(object: target, in: vm)

    return vm.delegate.virtualMachine(vm, didSetUndefValue: value, in: target, forKey: String(cString: key!))
}

func bridgeGetValue(
    vmPointer: OpaquePointer!,
    xdata: UnsafeMutableRawPointer?,
    target: gravity_value_t,
    key: UnsafePointer<CChar>?,
    rIndex: UInt32
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }

    guard let xdata = xdata else {
        return GravityReturn.error("Extra data for bridging not passed!", vm: vm)
    }

    let target = GSValue(value: target, in: vm)

    let propertyDescriptor = Unmanaged<PropertyDescriptor>.fromOpaque(xdata).takeUnretainedValue()
    let value = propertyDescriptor.getValue(in: target)

    return GravityReturn.value(value, rIndex: Int32(rIndex), vm: vm)
}

func bridgeGetUndefValue(
    _ vmPointer: OpaquePointer?,
    xdata: UnsafeMutableRawPointer?,
    target: gravity_value_t,
    key: UnsafePointer<CChar>?,
    vindex: UInt32
) -> Bool {
    guard let vm = GravityVirtualMachine.getVM(vmPointer!) else { fatalError("Cannot found Virtual Machine") }
    let target = GSValue(object: target, in: vm)
    do {
        if let value = try vm.delegate.virtualMachine(vm, didGetUndefValueFrom: target, forKey: String(cString: key!)) {
            return GravityReturn.value(value, rIndex: Int32(vindex), vm: vm)
        }
        return GravityReturn.noValue()
    } catch {
        return GravityReturn.error(error.localizedDescription, rIndex: Int32(vindex), vm: vm)
    }
}
