//
//  GravityVirtualMachine.swift
//
//
//  Created by v.prusakov on 6/4/22.
//

@_exported import CGravity
import Foundation

/// Gravity Virtual Machine.
public final class GravityVirtualMachine {
    private var bridgeClassDescriptors: [String: GravityBridgeClassDescriptor] = [:]
    private var optionalClassNameStorage: [UnsafeMutablePointer<CChar>] = []
    private var optionalClassList: UnsafeMutablePointer<UnsafePointer<CChar>?>?
    
    public struct Settings {
        public var reportNullErrors: Bool
        public var disableGarbageCollectorCheck: Bool
        
        public init(
            reportNullErrors: Bool = false,
            disableGarbageCollectorCheck: Bool = false
        ) {
            self.reportNullErrors = reportNullErrors
            self.disableGarbageCollectorCheck = disableGarbageCollectorCheck
        }
    }
    
    unowned let delegate: GravityVirtualMachineDelegate
    
    internal let vmPtr: OpaquePointer
    private var vmDelegate: UnsafeMutablePointer<gravity_delegate_t>!

    public init(settings: Settings, delegate: GravityVirtualMachineDelegate) {
        var vmDelegate = gravity_delegate_t()
        vmDelegate.disable_gccheck_1 = settings.disableGarbageCollectorCheck
        vmDelegate.report_null_errors = settings.reportNullErrors
        vmDelegate.bridge_clone = bridgeClone
        vmDelegate.bridge_free = bridgeFree
        vmDelegate.bridge_equals = bridgeEquals
        vmDelegate.bridge_string = bridgeString
        vmDelegate.bridge_size = bridgeSize
        vmDelegate.bridge_initinstance = bridgeInitInstance
        vmDelegate.bridge_execute = bridgeExecute
        vmDelegate.bridge_setvalue = bridgeSetValue
        vmDelegate.bridge_getvalue = bridgeGetValue
        vmDelegate.bridge_setundef = bridgeSetUndefValue
        vmDelegate.bridge_getundef = bridgeGetUndefValue
        vmDelegate.loadfile_callback = bridgeLoadFileCallback
        vmDelegate.optional_classes = bridgeOptionalClasses
        vmDelegate.error_callback = errorCallback
        vmDelegate.log_clear = logClear
        vmDelegate.log_callback = logCallback
        let delegatePointer = UnsafeMutablePointer<gravity_delegate_t>.allocate(capacity: 1)
        delegatePointer.pointee = vmDelegate
        self.vmDelegate = delegatePointer
        self.vmPtr = gravity_vm_new(delegatePointer)
        self.delegate = delegate
        
        Self.register(self)

        delegatePointer.pointee.xdata = Unmanaged<AnyObject>.passUnretained(self).toOpaque()
    }
    
    deinit {
        Self.unregister(self)
        gravity_vm_free(self.vmPtr)
        releaseOptionalClassNames()
    }
    
    // MARK: - Public

    /// Load Gravity file in memory from source code.
    public func loadGravityFile(from source: String) -> GravityBinary {
        let compiler = GravityCompiler(delegate: vmDelegate)
        let binary = compiler.compile(source: source, debug: true)
        return binary
    }

    /// Execute gravity binary and return result.
    @discardableResult
    public func execute(_ binary: GravityBinary) -> GSValue? {
        binary.compiler.transferMem(to: self)
        let runResult = gravity_vm_runmain(self.vmPtr, binary.binary)

        if runResult {
            let result = gravity_vm_result(self.vmPtr)
            return GSValue(value: result, in: self)
        }
        
        return nil
    }
    
    @discardableResult
    public func execute(
        closure: UnsafeMutablePointer<gravity_closure_t>!,
        sender: GSValue? = nil,
        params: [GSValue] = []
    ) -> GSValue? {
        var args = params.map { $0.value }
        let sender = (sender ?? GSValue(nullIn: self)).value
        let runResult = gravity_vm_runclosure(self.vmPtr, closure, sender, &args, UInt16(args.count))
        
        if runResult {
            let result = gravity_vm_result(self.vmPtr)
            return GSValue(value: result, in: self)
        }
        
        return nil
    }

    /// Load gravity closure in memory
    public func loadClosure(
        closure: UnsafeMutablePointer<gravity_closure_t>!
    ) {
        gravity_vm_loadclosure(self.vmPtr, closure)
    }
    
    /// Off/On Garbage Collector
    public func setGCEnabled(_ isEnabled: Bool) {
        gravity_gc_setenabled(self.vmPtr, isEnabled)
    }

    /// Get virtual machine execution time
    public func getTime() -> Double {
        gravity_vm_time(self.vmPtr)
    }

    /// Reset memory from virtual machine.
    public func reset() {
        gravity_vm_reset(self.vmPtr)
    }
    
    public func getResult() -> GSValue {
        let value = gravity_vm_result(self.vmPtr)
        return GSValue(value: value, in: self)
    }
}

// MARK: - Bridging

public extension GravityVirtualMachine {
    /// Bind swift class or struct to virtual machine. Swift type will be available from Gravity using swift name.
    /// To change Swift type name in gravity use ``GSExportable/runtimeName-34zfb``.
    ///
    /// ```swift
    /// @GSExportable
    /// class MySwiftObject {
    ///     var property: String
    ///
    ///     init() {
    ///         property = "Hello from Swift"
    ///     }
    ///
    ///     func print() {
    ///         print(self.property)
    ///     }
    /// }
    /// ```
    ///
    /// in gravity
    ///
    /// ```swift
    /// var instance = MySwiftObject()
    /// instance.print() // "Hello from Swift"
    /// ```
    func bindClass<T: GSExportable>(with type: T.Type) throws {
        self.setGCEnabled(false)
        defer {
            self.setGCEnabled(true)
        }

        let encoder = GravityExportEncoder(vm: self)
        try type.export(in: encoder)
        
        // Collect all descriptors
        let descriptors = encoder.classDescriptors
        
        for descriptor in descriptors {
            assert(bridgeClassDescriptors[descriptor.registredName] == nil, "We have registred class with name - \(descriptor.registredName).")
            
            self.setValue(descriptor.gClass, forKey: descriptor.registredName)
            self.bridgeClassDescriptors[descriptor.registredName] = descriptor
        }

        rebuildOptionalClassNames()
    }

    /// Set value to gravity virtual machine.
    ///
    /// ```swift
    /// // Code in swift
    /// virtualMachine.setValue("Hello", forKey: "myValue")
    /// ```
    ///
    /// in gravity file:
    /// ```swift
    /// // Code in gravity
    /// extern var myValue: String
    ///
    /// ```
    func setValue<T>(_ value: T, forKey key: String) {
        let gsValue = GSValue(object: value, in: self)
        key.withCString { ptr in
            gravity_vm_setvalue(self.vmPtr, ptr, gsValue.value)
        }
    }

    /// Set value to gravity virtual machine.
    ///
    /// ```swift
    /// // Code in swift
    /// virtualMachine.setValue("Hello", forKey: "myValue")
    /// ```
    ///
    /// in gravity file:
    /// ```swift
    /// // Code in gravity
    /// extern var myValue: String
    ///
    /// ```
    func setValue(_ value: GSValue, forKey key: String) {
        key.withCString { ptr in
            gravity_vm_setvalue(self.vmPtr, ptr, value.value)
        }
    }

    subscript(_ key: String) -> GSValue {
        get {
            self.getValue(forKey: key)
        }
        set {
            self.setValue(newValue, forKey: key)
        }
    }

    /// Get value from global scope in virtual machine.
    func getValue(forKey key: String) -> GSValue {
        let value = key.withCString { ptr in
            return gravity_vm_getvalue(self.vmPtr, ptr, UInt32(key.count))
        }
        
        return GSValue(value: value, in: self)
    }
    
}

// MARK: - Bridging Internal

extension GravityVirtualMachine {
    func getClassDescriptor(for name: String) -> GravityBridgeClassDescriptor? {
        return bridgeClassDescriptors[name]
    }

    func registredClasses() -> [String] {
        return Array(bridgeClassDescriptors.keys)
    }

    func optionalClassNamesPointer() -> UnsafeMutablePointer<UnsafePointer<CChar>?> {
        if optionalClassList == nil {
            rebuildOptionalClassNames()
        }
        return optionalClassList!
    }

    private func rebuildOptionalClassNames() {
        releaseOptionalClassNames()

        optionalClassNameStorage = bridgeClassDescriptors.keys.sorted().map { name in
            let bytes = name.utf8CString
            let pointer = UnsafeMutablePointer<CChar>.allocate(capacity: bytes.count)
            bytes.withUnsafeBufferPointer { buffer in
                pointer.initialize(from: buffer.baseAddress!, count: buffer.count)
            }
            return pointer
        }

        let list = UnsafeMutablePointer<UnsafePointer<CChar>?>.allocate(capacity: optionalClassNameStorage.count + 1)
        for (index, pointer) in optionalClassNameStorage.enumerated() {
            list[index] = UnsafePointer(pointer)
        }
        list[optionalClassNameStorage.count] = nil
        optionalClassList = list
    }

    private func releaseOptionalClassNames() {
        optionalClassList?.deallocate()
        optionalClassList = nil
        optionalClassNameStorage.forEach { $0.deallocate() }
        optionalClassNameStorage.removeAll(keepingCapacity: false)
    }
}

extension GravityVirtualMachine {
    private final class WeakVirtualMachine {
        weak var value: GravityVirtualMachine?

        init(_ value: GravityVirtualMachine) {
            self.value = value
        }
    }

    private final class VirtualMachineRegistry: @unchecked Sendable {
        private let lock = NSLock()
        private var virtualMachines: [OpaquePointer: WeakVirtualMachine] = [:]

        func virtualMachine(for pointer: OpaquePointer) -> GravityVirtualMachine? {
            lock.lock()
            defer { lock.unlock() }
            return virtualMachines[pointer]?.value
        }

        func register(_ virtualMachine: GravityVirtualMachine) {
            lock.lock()
            defer { lock.unlock() }
            virtualMachines[virtualMachine.vmPtr] = WeakVirtualMachine(virtualMachine)
        }

        func unregister(_ virtualMachine: GravityVirtualMachine) {
            lock.lock()
            defer { lock.unlock() }
            virtualMachines.removeValue(forKey: virtualMachine.vmPtr)
        }
    }

    private static let registry = VirtualMachineRegistry()
    
    nonisolated static func getVM(_ pointer: OpaquePointer) -> GravityVirtualMachine? {
        registry.virtualMachine(for: pointer)
    }
    
    nonisolated static func register(_ vm: GravityVirtualMachine) {
        registry.register(vm)
    }
    
    nonisolated static func unregister(_ vm: GravityVirtualMachine) {
        registry.unregister(vm)
    }
}

extension GravityVirtualMachine {
    func getOrRegisterClass<T: GSExportable>(_ type: T.Type) -> UnsafeMutablePointer<gravity_class_t> {
        let clazzName = self.getValue(forKey: T.runtimeName)
        
        if clazzName.isClass {
            return clazzName.toGravityClass
        }

        let clazz = T.runtimeName.withCString { ptr in
            return gravity_class_new_pair(
                self.vmPtr, // vm
                ptr, // name
                nil, // parent class
                0, // nivar
                0 // nsvar
            )
        }

//        clazz?.pointee.is_struct = !T.isClass

        return clazz!
    }
}

extension String {
    func toPointer() -> UnsafePointer<CChar>? {
        guard let data = self.data(using: .utf8) else { return nil }
        
        let buffer = UnsafeMutablePointer<CChar>.allocate(capacity: data.count)
        let stream = OutputStream(toBuffer: buffer, capacity: data.count)
        
        stream.open()
        data.withUnsafeBytes { pointer in
            let charPointer = pointer.bindMemory(to: CChar.self)
            stream.write(charPointer.baseAddress!, maxLength: charPointer.count)
        }
        stream.close()
        
        return UnsafePointer<CChar>(buffer)
    }
}
