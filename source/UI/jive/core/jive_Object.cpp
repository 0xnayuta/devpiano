//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_Object.h"

namespace jive {
class Object::InternalListener : public Object::Listener {
public:
    explicit InternalListener(Object& obj)
        : object { obj } {
        for (const auto& nested : findNestedObjects()) {
            nested->addListener(*this);
        }

        object.addListener(*this);
    }

    void propertyChanged(Object& objectThatChanged, const juce::Identifier& propertyName) final {
        if (&objectThatChanged == &object) {
            for (const auto& nested : findNestedObjects()) {
                nested->addListener(*this);
            }
        } else {
            object.listeners.callExcluding(this, &Listener::propertyChanged, objectThatChanged, propertyName);
        }
    }

private:
    juce::Array<Object*> findNestedObjects() {
        juce::Array<Object*> nestedObjects;

        for (const auto& [name, value] : object.getProperties()) {
            if (auto* nested = dynamic_cast<Object*>(value.getDynamicObject())) {
                nestedObjects.add(nested);
            }
        }

        return nestedObjects;
    }

    Object& object;
};

Object::Object()
    : internalListener { std::make_unique<InternalListener>(*this) } {
}

Object::Object(std::initializer_list<juce::NamedValueSet::NamedValue> initialProperties)
    : internalListener { std::make_unique<InternalListener>(*this) } {
    for (const auto& pair : initialProperties) {
        setProperty(pair.name, pair.value);
    }
}

Object::Object(const Object& other)
    : juce::DynamicObject { dynamic_cast<const DynamicObject&>(other) }
    , internalListener { std::make_unique<InternalListener>(*this) } {
}

Object::Object(Object&& other) noexcept
    : juce::DynamicObject { dynamic_cast<const DynamicObject&>(other) }
    , internalListener { std::make_unique<InternalListener>(*this) } {
}

Object::Object(const juce::DynamicObject& other)
    : juce::DynamicObject { other } {
}

void Object::didModifyProperty(const juce::Identifier& propertyName, const std::optional<juce::var>& newValue) {
    if (auto* childObject = dynamic_cast<Object*>(newValue.value_or(juce::var {}).getDynamicObject())) {
        childObject->parent = this;
    }

    listeners.call(&Listener::propertyChanged, *this, propertyName);
}

const juce::NamedValueSet& Object::getProperties() const {
    return dynamic_cast<juce::DynamicObject*>(const_cast<Object*>(this))->getProperties();
}

Object* Object::getParent() noexcept {
    return parent;
}

const Object* Object::getParent() const noexcept {
    return parent;
}

Object* Object::getRoot() noexcept {
    if (parent == nullptr) {
        return this;
    }

    return parent->getRoot();
}

const Object* Object::getRoot() const noexcept {
    if (parent == nullptr) {
        return this;
    }

    return parent->getRoot();
}

void Object::addListener(Listener& listener) const {
    listeners.add(&listener);
}

void Object::removeListener(Listener& listener) const {
    listeners.remove(&listener);
}

const juce::var& Object::operator[](const juce::Identifier& name) const noexcept {
    return getProperties()[name];
}

static void replaceDynamicObjectsWithJiveObjects(juce::var& value) {
    if (auto* dynamicObject = value.getDynamicObject()) {
        for (auto i = 0; i < dynamicObject->getProperties().size(); i++) {
            replaceDynamicObjectsWithJiveObjects(*dynamicObject->getProperties().getVarPointerAt(i));
        }

        Object::ReferenceCountedPointer object = new Object { *dynamicObject };
        value = object.get();
    }

    if (auto* array = value.getArray()) {
        for (auto& element : *array) {
            replaceDynamicObjectsWithJiveObjects(element);
        }
    }
}

juce::var parseJSON(const juce::String& jsonString) {
    auto value = juce::JSON::parse(jsonString);
    replaceDynamicObjectsWithJiveObjects(value);

    return value;
}
} // namespace jive

namespace juce {
jive::Object::ReferenceCountedPointer
VariantConverter<jive::Object::ReferenceCountedPointer>::fromVar(const var& value) {
    if (value.isString()) {
        return fromVar(jive::parseJSON(value.toString()));
    }

    if (auto* dynamicObject = value.getDynamicObject()) {
        if (auto* object = dynamic_cast<jive::Object*>(dynamicObject)) {
            return object;
        }

        jassertfalse;
    }

    return nullptr;
}

var VariantConverter<jive::Object::ReferenceCountedPointer>::toVar(
    const jive::Object::ReferenceCountedPointer& object) {
    return var { object.get() };
}
} // namespace juce
