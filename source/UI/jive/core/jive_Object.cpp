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
        syncNestedObjects();
        object.addListener(*this);
    }

    ~InternalListener() override {
        object.removeListener(*this);

        for (auto* nested : observedNested) {
            nested->removeListener(*this);
        }
    }

    void propertyChanged(Object& objectThatChanged, const juce::Identifier& propertyName) final {
        if (&objectThatChanged == &object) {
            // 自身属性变更由 Object::didModifyProperty 直接通知外部监听者，
            // 这里只同步当前直接嵌套对象的订阅集合。
            syncNestedObjects();
        } else {
            object.listeners.callExcluding(this, &Listener::propertyChanged, objectThatChanged, propertyName);
        }
    }

private:
    void syncNestedObjects() {
        const auto nested = findNestedObjects();

        for (auto i = observedNested.size(); --i >= 0;) {
            auto* previouslyObserved = observedNested.getUnchecked(i);

            if (!nested.contains(previouslyObserved)) {
                observedNested.remove(i);
                previouslyObserved->removeListener(*this);

                // 被移出父对象的嵌套对象不再拥有该父链，避免父对象先析构后留下悬空指针。
                if (previouslyObserved->parent == &object) {
                    previouslyObserved->parent = nullptr;
                }
            }
        }

        for (auto* child : nested) {
            if (!observedNested.contains(child)) {
                observedNested.add(child);
                child->addListener(*this);
            }
        }
    }

    juce::Array<Object*> findNestedObjects() const {
        juce::Array<Object*> nestedObjects;

        for (const auto& [name, value] : object.getProperties()) {
            if (auto* nested = dynamic_cast<Object*>(value.getDynamicObject())) {
                nestedObjects.add(nested);
            }
        }

        return nestedObjects;
    }

    Object& object;
    juce::Array<Object*> observedNested;
};

Object::Object()
    : internalListener { std::make_unique<InternalListener>(*this) } {
}

Object::Object(std::initializer_list<juce::NamedValueSet::NamedValue> initialProperties)
    : Object {} {
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

static void replaceDynamicObjectsWithJiveObjects(juce::var& value, Object* parent = nullptr);

// 嵌套对象必须在 InternalListener 创建前完成转换，才能在构造期就建立父子链。
[[nodiscard]] static juce::DynamicObject withNestedJiveObjects(const juce::DynamicObject& source, Object* parent) {
    juce::DynamicObject result { source };

    for (auto i = 0; i < result.getProperties().size(); i++) {
        replaceDynamicObjectsWithJiveObjects(*result.getProperties().getVarPointerAt(i), parent);
    }

    return result;
}

Object::Object(const juce::DynamicObject& other, Object* parentObject)
    : juce::DynamicObject { withNestedJiveObjects(other, this) }
    , internalListener { std::make_unique<InternalListener>(*this) }
    , parent { parentObject } {
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

static void replaceDynamicObjectsWithJiveObjects(juce::var& value, Object* parent) {
    if (auto* dynamicObject = value.getDynamicObject();
        dynamicObject != nullptr && dynamic_cast<Object*>(dynamicObject) == nullptr) {
        // 对象自身的嵌套属性由其构造函数递归转换，以获得正确的父链。
        value = new Object { *dynamicObject, parent };
    }

    if (auto* array = value.getArray()) {
        for (auto& element : *array) {
            replaceDynamicObjectsWithJiveObjects(element, parent);
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
