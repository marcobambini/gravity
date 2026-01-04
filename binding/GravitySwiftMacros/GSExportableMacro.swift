import SwiftSyntax
import Foundation
import SwiftSyntaxMacros
import SwiftSyntaxBuilder
import SwiftCompilerPlugin

enum MacroError: LocalizedError {
    case message(String)
    
    var errorDescription: String? {
        switch self {
        case .message(let string):
            string
        }
    }
}

/// Helper function to check if a declaration has the @GSExportableIgnore attribute
private func hasGSExportableIgnoreAttribute(_ attributes: AttributeListSyntax) -> Bool {
    attributes.contains { attr in
        guard let attributeSyntax = attr.as(AttributeSyntax.self) else {
            return false
        }
        
        // Check attribute name - can be IdentifierTypeSyntax or other type syntax
        let attributeName = attributeSyntax.attributeName.trimmedDescription
        // Remove the @ symbol if present and compare
        let name = attributeName.hasPrefix("@") ? String(attributeName.dropFirst()) : attributeName
        return name == "GSExportableIgnore"
    }
}

public struct GSExportableMacro: ExtensionMacro {
    public static func expansion(
        of node: AttributeSyntax,
        attachedTo declaration: some DeclGroupSyntax,
        providingExtensionsOf type: some TypeSyntaxProtocol,
        conformingTo protocols: [TypeSyntax],
        in context: some MacroExpansionContext
    ) throws -> [ExtensionDeclSyntax] {
        let typeDecl: any NamedDeclSyntax & DeclGroupSyntax
        if let classDecl = declaration.as(ClassDeclSyntax.self) {
            typeDecl = classDecl
        } else if let structDecl = declaration.as(StructDeclSyntax.self) {
            typeDecl = structDecl
        } else {
            throw MacroError.message("This macro can only be applied to classes and structs")
        }
        
        let typeName = typeDecl.name.text
        let members = typeDecl.memberBlock.members
        
        // Get custom name from macro attribute if provided
        let customName: String
        if let stringLiteral = node.arguments?.as(LabeledExprListSyntax.self)?.first?.expression.as(StringLiteralExprSyntax.self) {
            customName = stringLiteral.segments.first?.as(StringSegmentSyntax.self)?.content.text ?? typeName
        } else {
            customName = typeName
        }
        
        var propertyBinds: [String] = []
        var methodBinds: [String] = []
        var constructorBinds: [String] = []
        var staticPropertyBinds: [String] = []
        var staticMethodBinds: [String] = []
        
        for member in members {
            if let property = member.decl.as(VariableDeclSyntax.self) {
                let isStatic = property.modifiers.contains { $0.name.tokenKind == .keyword(.static) }
                let isPrivate = property.modifiers.contains { $0.name.tokenKind == .keyword(.private) }
                if isPrivate { continue }
                
                // Check if property has @GSExportableIgnore attribute
                if hasGSExportableIgnoreAttribute(property.attributes) { continue }
                
                for binding in property.bindings {
                    if let identifier = binding.pattern.as(IdentifierPatternSyntax.self) {
                        let propertyName = identifier.identifier.text
                        if isStatic {
                            staticPropertyBinds.append("container.bind(.staticProperty(\(typeName).\(propertyName), named: \"\(propertyName)\"))")
                        } else {
                            propertyBinds.append("container.bind(.property(\\\(typeName).\(propertyName), named: \"\(propertyName)\"))")
                        }
                    }
                }
            } else if let initializer = member.decl.as(InitializerDeclSyntax.self) {
                let isPrivate = initializer.modifiers.contains { $0.name.tokenKind == .keyword(.private) }
                if isPrivate { continue }
                constructorBinds.append("container.bind(.constructor(\(typeName).init))")
            } else if let function = member.decl.as(FunctionDeclSyntax.self) {
                let isStatic = function.modifiers.contains { $0.name.tokenKind == .keyword(.static) }
                let isPrivate = function.modifiers.contains { $0.name.tokenKind == .keyword(.private) }
                if isPrivate { continue }
                
                // Check if function has @GSExportableIgnore attribute
                if hasGSExportableIgnoreAttribute(function.attributes) { continue }
                
                let functionName = function.name.text
                if isStatic {
                    staticMethodBinds.append("container.bind(.staticMethod(\(typeName).\(functionName), named: \"\(functionName)\"))")
                } else {
                    methodBinds.append("container.bind(.method(\(typeName).\(functionName), named: \"\(functionName)\"))")
                }
            }
        }
        
        let allBinds = constructorBinds + staticMethodBinds + methodBinds + staticPropertyBinds + propertyBinds
        
        let extensionDecl = try ExtensionDeclSyntax(
            """
            extension \(raw: typeName): Gravity.GSExportable {
                static var runtimeName: String {
                    return "\(raw: customName)"
                }

                static func export(in encoder: Gravity.GravityExportEncoder) throws {
                    let container = try encoder.makeContainer(for: \(raw: typeName).self)
                    \(raw: allBinds.joined(separator: "\n    "))
                }
            }
            """
        )
        
        return [extensionDecl]
    }
}
