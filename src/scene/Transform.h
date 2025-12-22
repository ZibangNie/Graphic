#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <algorithm>

class Transform {
public:
    Transform() = default;

    // ---- Local TRS setters ----
    void setLocalPosition(const glm::vec3& p) { m_localPos = p; markDirty(); }
    void setLocalRotation(const glm::quat& q) { m_localRot = q; markDirty(); }
    void setLocalScale(const glm::vec3& s)    { m_localScale = s; markDirty(); }

    // Helpers
    void setLocalRotationEulerDeg(const glm::vec3& eulerDeg) {
        // order: X then Y then Z
        glm::quat qx = glm::angleAxis(glm::radians(eulerDeg.x), glm::vec3(1,0,0));
        glm::quat qy = glm::angleAxis(glm::radians(eulerDeg.y), glm::vec3(0,1,0));
        glm::quat qz = glm::angleAxis(glm::radians(eulerDeg.z), glm::vec3(0,0,1));
        setLocalRotation(qz * qy * qx);
    }

    const glm::vec3& localPosition() const { return m_localPos; }
    const glm::quat& localRotation() const { return m_localRot; }
    const glm::vec3& localScale()    const { return m_localScale; }

    // ---- Hierarchy ----
    void setParent(Transform* newParent) {
        if (m_parent == newParent) return;

        // Remove from old parent children list
        if (m_parent) {
            auto& sibs = m_parent->m_children;
            sibs.erase(std::remove(sibs.begin(), sibs.end(), this), sibs.end());
        }

        m_parent = newParent;

        // Add to new parent children list
        if (m_parent) {
            m_parent->m_children.push_back(this);
        }

        markDirty();
    }

    Transform* parent() const { return m_parent; }
    const std::vector<Transform*>& children() const { return m_children; }

    // ---- World ----
    const glm::mat4& worldMatrix() const {
        if (m_dirty) {
            glm::mat4 local = localMatrix();
            if (m_parent) {
                m_world = m_parent->worldMatrix() * local;
            } else {
                m_world = local;
            }
            m_dirty = false;
        }
        return m_world;
    }

    glm::vec3 worldPosition() const {
        const glm::mat4& w = worldMatrix();
        return glm::vec3(w[3][0], w[3][1], w[3][2]);
    }

private:
    void markDirty() {
        if (!m_dirty) m_dirty = true;
        // propagate to descendants
        for (Transform* c : m_children) {
            if (c) c->markDirty();
        }
    }

    glm::mat4 localMatrix() const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, m_localPos);
        m *= glm::mat4_cast(m_localRot);
        m = glm::scale(m, m_localScale);
        return m;
    }

private:
    glm::vec3 m_localPos{0.f, 0.f, 0.f};
    glm::quat m_localRot{1.f, 0.f, 0.f, 0.f}; // w,x,y,z
    glm::vec3 m_localScale{1.f, 1.f, 1.f};

    Transform* m_parent = nullptr;
    std::vector<Transform*> m_children;

    mutable bool m_dirty = true;
    mutable glm::mat4 m_world{1.f};
};
