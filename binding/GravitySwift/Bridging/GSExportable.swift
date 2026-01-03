//
//  GSExportable.swift
//  
//
//  Created by v.prusakov on 6/4/22.
//

/// Base protocol to collection information about class or struct.
/// You can define class name, methods and static methods which called from Gravity Script.
public protocol GSExportable {

    static var runtimeName: String { get }

    static func export(in encoder: GravityExportEncoder) throws
}

public extension GSExportable {
    static var runtimeName: String {
        return String(describing: self)
    }

    static var isClass: Bool {
        self is AnyClass
    }
}
