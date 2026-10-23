//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

#include "UI/jive/core/values/jive_Identifiers.h"
#include "UI/jive/core/values/jive_Object.h"
#include "UI/jive/core/values/jive_PropertyBehaviours.h"
#include "UI/jive/core/values/jive_VariantConvertion.h"

#include "UI/jive/core/values/jive_Transitions.h"
#include "UI/jive/core/values/jive_Visitor.h"

namespace jive {
template <typename T> struct isReferenceCountedObjectPointer : std::false_type { };

template <typename T> struct isReferenceCountedObjectPointer<juce::ReferenceCountedObjectPtr<T>> : std::true_type { };

template <typename ValueType, Inheritance inheritance = Inheritance::doNotInherit,
          Accumulation accumulation = Accumulation::doNotAccumulate,
          bool autoParseStrings = isReferenceCountedObjectPointer<ValueType>::value,
          Responsiveness responsiveness = Responsiveness::respondToChanges>
class Property : protected juce::ValueTree::Listener, protected Object::Listener, private Transition::Listener {
public:
    using Source = std::variant<juce::ValueTree, Object::ReferenceCountedPointer>;

    Property(Source propertySource, const juce::Identifier& propertyID)
        : id { propertyID }
        , source { std::move(propertySource) }
        , transitionSourceID { propertyID } {
        initialise();
    }

    Property(const Property& other)
        : id { other.id } {
        *this = other;
    }

    Property(Property&& other) noexcept
        : id { other.id } {
        *this = std::move(other);
    }

    Property& operator=(const Property& other) {
        jassert(id == other.id);
        transitionSourceID = other.transitionSourceID;
        source = other.source;
        onValueChange = other.onValueChange;

        initialise();

        return *this;
    }

    Property& operator=(Property&& other) noexcept {
        jassert(id == other.id);
        transitionSourceID = std::move(other.transitionSourceID);
        source = std::move(other.source);
        onValueChange = std::move(other.onValueChange);

        initialise();

        return *this;
    }

    ~Property() override {
        observeTransition(nullptr);
        observeObject(nullptr);
        removeThisAsListener(source);
        removeThisAsListener(listenerTarget);
    }

    [[nodiscard]] ValueType get() const {
        return getFrom(getRootOfInheritance());
    }

    [[nodiscard]] auto getOr(const ValueType& valueIfNoneSpecified) const {
        if (auto root = getRootOfInheritance(); isValid(root)) {
            return getFrom(root);
        }

        if constexpr (accumulation == Accumulation::accumulate) {
            if (auto src = getFirstDescendantWithProperty(source); src.isValid()) {
                return getFrom(src);
            }
        }

        return valueIfNoneSpecified;
    }

    [[nodiscard]] auto calculateCurrent() const {
        if (auto* transition = getTransition();
            transition != nullptr && !transition->source.isVoid() && !transition->target.isVoid()) {
            return transition->template calculateCurrent<ValueType>();
        }

        return get();
    }

    void set(const ValueType& newValue) {
        set(source, toVar(newValue));
    }

    void set(std::function<ValueType()> function) {
        set(source,
            juce::var {
                [function](const auto&) { return toVar(function()); },
            });
    }

    void setAuto() {
        set(source, "auto");
    }

    void clear() {
        std::visit(Visitor {
                       [this](juce::ValueTree& sourceTree) { sourceTree.removeProperty(id, nullptr); },
                       [this](const Object::ReferenceCountedPointer& sourceObject) {
                           if (sourceObject != nullptr) {
                               sourceObject->removeProperty(id);
                           }
                       },
                   },
                   source);
    }

    [[nodiscard]] auto exists() const {
        return !getVar(source, id).isVoid();
    }

    [[nodiscard]] auto isAuto() const {
        return (!exists()) || toString().trim().equalsIgnoreCase("auto");
    }

    [[nodiscard]] auto isFunctional() const {
        if (!exists()) {
            return false;
        }

        return std::visit(Visitor {
                              [this](const juce::ValueTree& sourceTree) { return sourceTree[id].isMethod(); },
                              [this](const Object::ReferenceCountedPointer& sourceObject) {
                                  if (sourceObject != nullptr) {
                                      return sourceObject->getProperty(id).isMethod();
                                  }

                                  return false;
                              },
                          },
                          source);
    }

    [[nodiscard]] auto toString() const {
        if (!exists()) {
            return juce::String {};
        }

        return std::visit(Visitor {
                              [this](const juce::ValueTree& sourceTree) { return sourceTree[id].toString(); },
                              [this](const Object::ReferenceCountedPointer& sourceObject) {
                                  return sourceObject->getProperty(id).toString();
                              },
                          },
                          source);
    }

    void setTransitionSourceProperty(const juce::Identifier& sourceID) {
        observeTransition(nullptr);
        currentTransition = nullptr;
        currentTransitions = nullptr;
        transitionSourceID = sourceID;

        if (auto* transition = getTransition()) {
            observeTransition(transition);
        }
    }

    [[nodiscard]] Transitions::ReferenceCountedPointer readTransitions() const {
        const auto transitionDefinition = getVar(source, ids::transition());

        if (auto* transitions = dynamic_cast<Transitions*>(transitionDefinition.getObject())) {
            return transitions;
        }

        if (transitionDefinition.isString()) {
            return Transitions::fromString(transitionDefinition.toString());
        }

        return nullptr;
    }

    [[nodiscard]] Transition* getTransition() {
        if (currentTransition == nullptr) {
            currentTransitions = readTransitions();

            if (currentTransitions != nullptr) {
                currentTransition = (*currentTransitions)[transitionSourceID.toString()];
            }
        }

        if (currentTransition != nullptr && currentTransition->source.isVoid()) {
            currentTransition->source = getVar(source, id);
            currentTransition->target = currentTransition->source;
            currentTransition->commencement = now();
        }

        return currentTransition;
    }

    [[nodiscard]] const Transition* getTransition() const {
        return const_cast<Property*>(this)->getTransition();
    }

    [[nodiscard]] auto isTransitioning() const {
        return getTransition() != nullptr;
    }

    /** 供忽略自身监听的 Property 的拥有者转发变更：保持过渡状态与 onValueChange 生效。
        返回 true 表示变更属于本属性，此时回调可能已销毁拥有者。 */
    bool handlePropertyChange(juce::ValueTree& tree, const juce::Identifier& property) {
        const auto isThisProperty = property == id;
        valueTreePropertyChanged(tree, property);
        return isThisProperty;
    }

    [[nodiscard]] operator ValueType() const {
        return get();
    }

    auto& operator=(const ValueType& newValue) {
        set(newValue);
        return *this;
    }

    auto& operator=(std::function<ValueType()> function) {
        set(function);
        return *this;
    }

    auto& operator=(std::function<ValueType(std::vector<juce::var>)> function) {
        set(function);
        return *this;
    }

    const juce::Identifier id;
    mutable std::function<void(void)> onValueChange = nullptr;
    mutable std::function<void(void)> onTransitionProgressed = nullptr;

protected:
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyChanged,
                                  const juce::Identifier& property) override {
        if (property != id) {
            if (property == ids::transition() && isSource(treeWhosePropertyChanged)) {
                transitionsChanged();
            }

            return;
        }
        if (!respondToPropertyChanges(treeWhosePropertyChanged)) {
            return;
        }

        valueChanged();

        if (onValueChange != nullptr) {
            onValueChange();
        }
    }

    void propertyChanged(Object&, const juce::Identifier& property) override {
        if constexpr (std::is_same_v<ValueType, Object::ReferenceCountedPointer>) {
            if (std::holds_alternative<juce::ValueTree>(source)) {
                valueChanged();

                if (onValueChange != nullptr) {
                    onValueChange();
                }

                return;
            }
        } else {
            if (property != id) {
                return;
            }
        }

        valueChanged();

        if (onValueChange != nullptr) {
            onValueChange();
        }
    }

    [[nodiscard]] auto respondToPropertyChanges(juce::ValueTree& treeWhosePropertyChanged) const {
        if (treeWhosePropertyChanged == std::get<juce::ValueTree>(source)) {
            return true;
        }

        switch (inheritance) {
        case Inheritance::inheritFromParent:
            return treeWhosePropertyChanged == std::get<juce::ValueTree>(source).getParent();
        case Inheritance::inheritFromAncestors:
            return std::get<juce::ValueTree>(source).isAChildOf(treeWhosePropertyChanged);
        case Inheritance::doNotInherit:
            break;
        }

        switch (accumulation) {
        case Accumulation::accumulate:
            return treeWhosePropertyChanged.isAChildOf(std::get<juce::ValueTree>(source));
        case Accumulation::doNotAccumulate:
            break;
        }

        return false;
    }

    [[nodiscard]] auto respondToPropertyChanges(Object& objectWhosePropertyChanged) const {
        return &objectWhosePropertyChanged == static_cast<Object*>(std::get<Object::ReferenceCountedPointer>(source));
    }

    [[nodiscard]] auto getRootOfInheritance() const {
        if (exists() || accumulation == Accumulation::accumulate) {
            return source;
        }

        if constexpr (inheritance == Inheritance::inheritFromParent) {
            if (auto parent = getParent(source); !getVar(parent, id).isVoid()) {
                return parent;
            }
        }
        if constexpr (inheritance == Inheritance::inheritFromAncestors) {
            for (auto ancestor = getParent(source); isValid(ancestor); ancestor = getParent(ancestor)) {
                if (!getVar(ancestor, id).isVoid()) {
                    return ancestor;
                }
            }
        }

        return Source {};
    }

    [[nodiscard]] auto getFirstDescendantWithProperty(const Source& root) const {
        for (auto i = 0; i < getNumChildren(root); i++) {
            if (auto child = getChild(root, i); !getVar(child, id).isVoid()) {
                return child;
            }
        }

        for (auto i = 0; i < getNumChildren(root); i++) {
            if (auto descendant = getFirstDescendantWithProperty(getChild(root, i)); isValid(descendant)) {
                return descendant;
            }
        }

        return Source {};
    }

    [[nodiscard]] ValueType getFrom(const juce::ValueTree& root) const {
        if (!isValid(root)) {
            return fromVar<ValueType>(juce::var {});
        }

        if constexpr (accumulation == Accumulation::accumulate) {
            auto result = fromVar<ValueType>(getVar(root, id));

            for (auto i = 0; i < getNumChildren(root); i++) {
                result += getFrom(getChild(root, i));
            }

            return result;
        } else {
            auto var = root.hasProperty(id) ? root[id] : getVar(getFirstDescendantWithProperty(root), id);

            if (var.isMethod()) {
                juce::var::NativeFunctionArgs args { var, nullptr, 0 };
                var = var.getNativeFunction()(args);
            }

            return fromVar<ValueType>(var);
        }
    }

    [[nodiscard]] ValueType getFrom(Object* root) const {
        if (!isValid(root)) {
            return fromVar<ValueType>(juce::var {});
        }

        if constexpr (accumulation == Accumulation::accumulate) {
            auto result = fromVar<ValueType>(getVar(root, id));

            for (auto i = 0; i < getNumChildren(root); i++) {
                result += getFrom(getChild(root, i));
            }

            return result;
        } else {
            if (root->hasMethod(id)) {
                juce::var::NativeFunctionArgs args { juce::var {}, nullptr, 0 };
                return fromVar<ValueType>(root->invokeMethod(id, args));
            }

            return fromVar<ValueType>(root->getProperty(id));
        }
    }

    [[nodiscard]] auto getFrom(const Source& root) const {
        return std::visit(
            Visitor {
                [this](const juce::ValueTree& sourceTree) { return getFrom(sourceTree); },
                [this](const Object::ReferenceCountedPointer& sourceObject) { return getFrom(sourceObject.get()); },
            },
            root);
    }

    [[nodiscard]] auto isValid(const Source& src) const {
        return std::visit(
            Visitor {
                [](const juce::ValueTree& sourceTree) { return sourceTree.isValid(); },
                [](const Object::ReferenceCountedPointer& sourceObject) { return sourceObject != nullptr; },
            },
            src);
    }

    [[nodiscard]] juce::var getVar(const Source& src, const juce::Identifier& name) const {
        return std::visit(Visitor {
                              [name](const juce::ValueTree& sourceTree) { return sourceTree[name]; },
                              [name](const Object::ReferenceCountedPointer& sourceObject) {
                                  if (sourceObject != nullptr) {
                                      return sourceObject->getProperty(name);
                                  }

                                  return juce::var {};
                              },
                          },
                          src);
    }

    [[nodiscard]] auto getParent(const Source& src) const {
        return std::visit(
            Visitor {
                [](const juce::ValueTree& sourceTree) -> Source { return sourceTree.getParent(); },
                [](const Object::ReferenceCountedPointer& sourceObject) -> Source { return sourceObject->getParent(); },
            },
            src);
    }

    [[nodiscard]] auto getNumChildren(const Source& src) const {
        return std::visit(Visitor {
                              [](const juce::ValueTree& sourceTree) { return sourceTree.getNumChildren(); },
                              [](const Object::ReferenceCountedPointer& sourceObject) {
                                  if (sourceObject == nullptr) {
                                      return 0;
                                  }

                                  return static_cast<int>(std::count_if(
                                      std::begin(sourceObject->getProperties()),
                                      std::end(sourceObject->getProperties()), [](const auto& property) {
                                          return dynamic_cast<const Object*>(property.value.getDynamicObject())
                                              != nullptr;
                                      }));
                              },
                          },
                          src);
    }

    [[nodiscard]] auto getChild(const Source& src, int index) const {
        return std::visit(
            Visitor {
                [index](const juce::ValueTree& sourceTree) -> Source { return sourceTree.getChild(index); },
                [index](const Object::ReferenceCountedPointer& sourceObject) -> Source {
                    if (sourceObject == nullptr) {
                        return nullptr;
                    }

                    auto current = 0;

                    for (const auto& [_, value] : sourceObject->getProperties()) {
                        if (auto* child = dynamic_cast<Object*>(value.getDynamicObject())) {
                            if (current++ == index) {
                                return child;
                            }
                        }
                    }

                    return nullptr;
                },
            },
            src);
    }

    [[nodiscard]] auto findListenerTarget(const Source& src) const {
        switch (inheritance) {
        case Inheritance::inheritFromParent:
            return getParent(src);
        case Inheritance::inheritFromAncestors:
            return getRoot(src);
        case Inheritance::doNotInherit:
            return src;
        }

        jassertfalse;
        return src;
    }

    [[nodiscard]] auto getRoot(const Source& src) const {
        return std::visit(
            Visitor {
                [](const juce::ValueTree& sourceTree) -> Source { return sourceTree.getRoot(); },
                [](const Object::ReferenceCountedPointer& sourceObject) -> Source { return sourceObject->getRoot(); },
            },
            src);
    }

    void addThisAsListener(Source& src) {
        std::visit(Visitor {
                       [this](juce::ValueTree& sourceTree) { sourceTree.addListener(this); },
                       [this](const Object::ReferenceCountedPointer& sourceObject) {
                           if (sourceObject != nullptr) {
                               sourceObject->addListener(*this);
                           }
                       },
                   },
                   src);
    }

    void removeThisAsListener(Source& src) {
        std::visit(Visitor {
                       [this](juce::ValueTree& sourceTree) { sourceTree.removeListener(this); },
                       [this](const Object::ReferenceCountedPointer& sourceObject) {
                           if (sourceObject != nullptr) {
                               sourceObject->removeListener(*this);
                           }
                       },
                   },
                   src);
    }

    void set(Source& src, const juce::var& value) {
        std::visit(Visitor {
                       [this, &value](juce::ValueTree& sourceTree) { sourceTree.setProperty(id, value, nullptr); },
                       [this, &value](const Object::ReferenceCountedPointer& sourceObject) {
                           sourceObject->setProperty(id, value);
                       },
                   },
                   src);
    }

    virtual void transitionProgressed() {
    }

    Source source;

private:
    void initialise() {
        listenerTarget = findListenerTarget(source);

        if (!isValid(listenerTarget)) {
            listenerTarget = source;
        }

        if constexpr (responsiveness == Responsiveness::respondToChanges) {
            addThisAsListener(listenerTarget);
        }

        observeHeldObject();

        if constexpr (autoParseStrings) {
            if (auto value = getVar(source, id); value.isString()) {
                if (const auto parsed = get(); parsed != nullptr) {
                    set(parsed);
                }
            }
        }

        if constexpr (!std::is_same<ValueType, Transitions::ReferenceCountedPointer>()) {
            updateTransition();
        }
    }

    void transitionProgressed(const juce::String& propertyName, const Transition&) final {
        jassertquiet(propertyName == transitionSourceID.toString());

        transitionProgressed();

        if (onTransitionProgressed != nullptr) {
            onTransitionProgressed();
        }
    }

    void valueChanged() {
        observeHeldObject();

        if constexpr (!std::is_same<ValueType, Transitions::ReferenceCountedPointer>()) {
            updateTransition();
        }
    }

    // tree 里的对象可能被替换，监听必须跟随当前持有的对象。
    void observeHeldObject() {
        if constexpr (std::is_same_v<ValueType, Object::ReferenceCountedPointer>) {
            if (std::holds_alternative<juce::ValueTree>(source)) {
                observeObject(get());
            }
        }
    }

    void observeObject(Object::ReferenceCountedPointer object) {
        if (observedObject == object) {
            return;
        }

        if (observedObject != nullptr) {
            observedObject->removeListener(*this);
        }

        observedObject = std::move(object);

        if (observedObject != nullptr) {
            observedObject->addListener(*this);
        }
    }

    [[nodiscard]] bool isSource(const juce::ValueTree& tree) const {
        return std::holds_alternative<juce::ValueTree>(source) && std::get<juce::ValueTree>(source) == tree;
    }

    // 新过渡从旧过渡的当前值继续，避免中途修改 timing 时数值跳变。
    void transitionsChanged() {
        // 在替换引用前快照旧过渡状态，替换后不再解引用旧容器内的对象。
        const auto* previous = currentTransition;
        const auto hadPreviousTransition = previous != nullptr;
        const auto preservePrevious = previous != nullptr && previous->calculateProgress() < 1.0
            && !previous->source.isVoid() && !previous->target.isVoid();
        const auto previousValue
            = preservePrevious ? toVar(previous->template calculateCurrent<ValueType>()) : juce::var {};
        const auto previousTarget = preservePrevious ? previous->target : juce::var {};

        const auto nextTransitions = readTransitions();
        auto* nextTransition = nextTransitions != nullptr ? (*nextTransitions)[transitionSourceID.toString()] : nullptr;
        const auto sameTransition = nextTransition == previous;

        observeTransition(nullptr);
        currentTransition = nullptr;
        currentTransitions = nextTransitions;

        if (nextTransition != nullptr && nextTransition->source.isVoid()) {
            nextTransition->source = getVar(source, id);
            nextTransition->target = nextTransition->source;
            nextTransition->commencement = now();
        }

        if (nextTransition != nullptr && !sameTransition && preservePrevious) {
            nextTransition->source = previousValue;
            nextTransition->target = previousTarget;
            nextTransition->commencement = now();
        }

        currentTransition = nextTransition;
        observeTransition(nextTransition);

        // 移除活动过渡后，消费者仍停留在最后一个中间值；必须显式通知一次，
        // 让它读取 calculateCurrent() 的最终目标。回调可能销毁本对象，
        // 因此这是函数的最后一条语句。
        if (hadPreviousTransition && nextTransition == nullptr && onValueChange != nullptr) {
            onValueChange();
        }
    }

    void updateTransition() {
        const auto var = getVar(source, id);

        if (auto* transition = getTransition()) {
            const auto isUninitialised = transition->source.isVoid();
            const auto cannotAnimate = transition->target.isVoid() || var.isVoid();
            const auto isAlreadyUpToDate = transition->target == var;

            if (isUninitialised || cannotAnimate) {
                // 属性被移除或重新出现时没有可插值的端点，直接跳到当前值。
                transition->source = var;
                transition->target = var;
                transition->commencement = now();
            } else if (!isAlreadyUpToDate) {
                transition->source = toVar(transition->template calculateCurrent<ValueType>());
                transition->target = var;
                transition->commencement = now();
            }

            observeTransition(transition);
        } else {
            observeTransition(nullptr);
        }
    }

    void observeTransition(Transition* transition) {
        if (observedTransition == transition) {
            return;
        }

        if (observedTransition != nullptr) {
            observedTransition->removeListener(*this);
        }

        observedTransition = transition;

        if (transition != nullptr) {
            transition->addListener(*this);
        }
    }

    Source listenerTarget;
    juce::Identifier transitionSourceID;
    Transitions::ReferenceCountedPointer currentTransitions;
    Transition* currentTransition = nullptr;
    Transition* observedTransition = nullptr;
    Object::ReferenceCountedPointer observedObject;
};

template <typename ValueType, Accumulation accumulation = Accumulation::doNotAccumulate>
using PassiveProperty = Property<ValueType, Inheritance::doNotInherit, accumulation,
                                 isReferenceCountedObjectPointer<ValueType>::value, Responsiveness::ignoreChanges>;

/** 把一次 ValueTree 属性变更转发给被动属性，遇到属于该属性的位置即停止，
    因为它的回调可能销毁这些属性的拥有者。返回是否有属性匹配本次变更。 */
template <typename... Properties>
bool forwardPropertyChange(juce::ValueTree& tree, const juce::Identifier& property, Properties&... properties) {
    return (properties.handlePropertyChange(tree, property) || ...);
}
} // namespace jive
