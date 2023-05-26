#pragma once

// Standard C++ includes
#include <unordered_map>

// GeNN code generator includes
#include "code_generator/groupMerged.h"

// GeNN transpiler includes
#include "transpiler/errorHandler.h"
#include "transpiler/typeChecker.h"

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::GroupMergedTypeEnvironment
//----------------------------------------------------------------------------
namespace GeNN::CodeGenerator
{
template<typename G>
class GroupMergedTypeEnvironment : public Transpiler::TypeChecker::EnvironmentBase
{
    using Token = Transpiler::Token;
    using ErrorHandlerBase = Transpiler::ErrorHandlerBase;
    using EnvironmentBase = Transpiler::TypeChecker::EnvironmentBase;
    using TypeCheckError = Transpiler::TypeChecker::TypeCheckError;

public:
    GroupMergedTypeEnvironment(G &groupMerged, EnvironmentBase *enclosing = nullptr)
    :   m_GroupMerged(groupMerged), m_Enclosing(enclosing)
    {
    }

    //---------------------------------------------------------------------------
    // EnvironmentBase virtuals
    //---------------------------------------------------------------------------
    virtual void define(const Transpiler::Token &name, const Type::ResolvedType&, ErrorHandlerBase &errorHandler) final
    {
        errorHandler.error(name, "Cannot declare variable in external environment");
        throw TypeCheckError();
    }

    virtual std::vector<Type::ResolvedType> getTypes(const Token &name, ErrorHandlerBase &errorHandler) final
    {
        auto type = m_Types.find(name.lexeme);
        if(type == m_Types.end()) {
            if(m_Enclosing) {
                return m_Enclosing->getTypes(name, errorHandler);
            }
            else {
                errorHandler.error(name, "Undefined identifier");
                throw TypeCheckError();
            }
        }
        else {
            // Add field to merged group if required
            addField(type->second);

            return {type->second.first};
        }
    }

    //---------------------------------------------------------------------------
    // Public API
    //---------------------------------------------------------------------------
    void defineField(const Type::ResolvedType &type, const std::string &name)
    {
        if(!m_Types.try_emplace(name, type, std::nullopt).second) {
            throw std::runtime_error("Redeclaration of '" + std::string{name} + "'");
        }
    }


    void defineField(const Type::ResolvedType &type, const std::string &name,
                     const Type::ResolvedType &fieldType, std::string_view fieldName, typename G::GetFieldValueFunc getFieldValue, 
                     GroupMergedFieldType mergedFieldType = GroupMergedFieldType::STANDARD)
    {
        if(!m_Types.try_emplace(name, std::piecewise_construct,
                                std::forward_as_tuple(type),
                                std::forward_as_tuple(std::in_place, fieldType, fieldName, getFieldValue, mergedFieldType)).second) 
        {
            throw std::runtime_error("Redeclaration of '" + std::string{name} + "'");
        }
    }

    void definePointerField(const Type::ResolvedType &type, const std::string &name,const std::string &prefix, VarAccessMode access)
    {
        const auto qualifiedType = (access & VarAccessModeAttribute::READ_ONLY) ? type.addQualifier(Type::Qualifier::CONSTANT) : type;
        defineField(qualifiedType, name,
                    type.createPointer(), name, [prefix](const auto &g, size_t) { return prefix + g.getName(); });
    }

    void definePointerField(const Type::UnresolvedType &type, const std::string &name, const std::string &prefix, VarAccessMode access)
    {
        definePointerField(type.resolve(m_GroupMerged.getTypeContext()), name, prefix, access);
    }

    void defineScalarField(const std::string &name, typename G::GetFieldDoubleValueFunc getFieldValue)
    {
        defineField(m_GroupMerged.getScalarType().addQualifier(Type::Qualifier::CONSTANT), name,
                    m_GroupMerged.getScalarType(), name,
                    [getFieldValue, this](const auto &g, size_t i)
                    {
                        return (Utils::writePreciseString(getFieldValue(g, i), m_GroupMerged.getScalarType().getNumeric().maxDigits10) 
                                + m_GroupMerged.getScalarType().getNumeric().literalSuffix);
                    });
    }
    
    template<typename T, typename P, typename H>
    void defineHeterogeneousParams(const Snippet::Base::StringVec &paramNames, const std::string &suffix,
                                   P getParamValues, H isHeterogeneous)
    {
        // Loop through params
        for(const auto &p : paramNames) {
            if (std::invoke(isHeterogeneous, m_GroupMerged, p)) {
                defineScalarField(p + suffix,
                                  [p, getParamValues](const auto &g, size_t)
                                  {
                                      return getParamValues(g).at(p);
                                  });
            }
            // Otherwise, just add a const-qualified scalar to the type environment
            else {
                defineField(m_GroupMerged.getScalarType().addQualifier(Type::Qualifier::CONSTANT), p + suffix);
            }
        }
    }

    template<typename T, typename D, typename H>
    void defineHeterogeneousDerivedParams(const Snippet::Base::DerivedParamVec &derivedParams, const std::string &suffix,
                                          D getDerivedParamValues, H isHeterogeneous)
    {
        // Loop through derived params
        for(const auto &d : derivedParams) {
            if (std::invoke(isHeterogeneous, m_GroupMerged, d.name)) {
                defineScalarField(d.name + suffix,
                                  [d, getDerivedParamValues](const auto &g, size_t)
                                  {
                                      return getDerivedParamValues(g).at(d.name);
                                  });
            }
            else {
                defineField(m_GroupMerged.getScalarType().addQualifier(Type::Qualifier::CONSTANT), d.name + suffix);
            }
        }
    }

    void defineVars(const Models::Base::VarVec &vars, const std::string &arrayPrefix)
    {
        // Loop through variables
        for(const auto &v : vars) {
            definePointerField(v.type, v.name, arrayPrefix, getVarAccessMode(v.access));
        }
    }

    template<typename V>
    void defineVarReferences(const Models::Base::VarRefVec &varReferences, const std::string &arrayPrefix, V getVarRefFn)
    {
        // Loop through variables
        for(const auto &v : varReferences) {
            // If variable access is read-only, qualify type with const
            const auto resolvedType = v.type.resolve(m_GroupMerged.getTypeContext());
            const auto qualifiedType = (v.access & VarAccessModeAttribute::READ_ONLY) ? resolvedType.addQualifier(Type::Qualifier::CONSTANT) : resolvedType;
            defineField(qualifiedType, v.name,
                        resolvedType.createPointer(), v.name,
                        [arrayPrefix, getVarRefFn, v](const auto &g, size_t) 
                        { 
                            const auto varRef = getVarRefFn(g).at(v.name);
                            return arrayPrefix + varRef.getVar().name + varRef.getTargetName(); 
                        });
        }
    }
  
    void defineEGPs(const Snippet::Base::EGPVec &egps, const std::string &arrayPrefix, const std::string &varName = "")
    {
        for(const auto &e : egps) {
            const auto pointerType = e.type.resolve(m_GroupMerged.getTypeContext()).createPointer();
            defineField(pointerType, e.name,
                        pointerType, e.name + varName,
                        [arrayPrefix, e, varName](const auto &g, size_t) 
                        {
                            return arrayPrefix + e.name + varName + g.getName(); 
                        },
                        GroupMergedFieldType::DYNAMIC);
        }
    }

private:
    //---------------------------------------------------------------------------
    // Private methods
    //---------------------------------------------------------------------------
    void addField(std::pair<Type::ResolvedType, std::optional<typename G::Field>> &type)
    {
        // If this type has an associated field
        if (type.second) {
            // Call function to add field to underlying merge group
            // **THINK** std::apply should work here but doesn't seem to
            /*std::apply(&G::addField, std::tuple_cat(std::make_tuple(m_GroupMerged),
                                                    *type.second));*/
            m_GroupMerged.addField(std::get<0>(*type.second), std::get<1>(*type.second),
                                   std::get<2>(*type.second), std::get<3>(*type.second));

            // Reset optional field so it doesn't get added again
            type.second.reset();
        }
    }
    //---------------------------------------------------------------------------
    // Members
    //---------------------------------------------------------------------------
    G &m_GroupMerged;
    EnvironmentBase *m_Enclosing;

    std::unordered_map<std::string, std::pair<Type::ResolvedType, std::optional<typename G::Field>>> m_Types;
};
}	// namespace GeNN::CodeGenerator
